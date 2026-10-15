#include "bklv/widgets/appearance_view.hpp"

#include "bklv/glass/glass_service.hpp"
#include "bklv/widgets/appearance_children.hpp"
#include "bklv/widgets/layer_draw.hpp"
#include "bklv/compile/layer_registry.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "bklv/core/font.hpp"
#include "bklv/core/icon_font.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/shadow.hpp"
#include "bklv/core/time.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

AppearanceView::AppearanceView(std::string appearanceName, bool focusable)
    : Box(Axis::COLUMN) // 外观驱动控件也是容器：弹窗要在里面放按钮行
    , appearanceName(std::move(appearanceName))
    , focusable(focusable)
{
    // 默认不装测量函数：装了之后 Yoga 不许这个节点再有子节点。
    // 需要"按内容定尺寸"的叶子控件（Button / TabCell / Header …）自己调 enableContentMeasurement()。

    // 构造时就套用主题：不然直接用 AppearanceView 的地方会拿到空外观
    // （遮罩/面板一笔都画不出来，排查起来很费时间——踩过）。
    this->applyTheme();

    // 外观里声明了子节点就建出来（`children` 段；没写就是空，什么都不做）。
    // 注意时机：必须放在 applyTheme() **之后**，否则拿不到刚套好的外观。
    appearance::buildChildren(*this, this->appearance.getAppearance().getChildren());
}

namespace
{
// 聚焦放大：数值**写死在代码里**（用户要求焦点相关表现不受主题 JSON 控制）。
// 想改就走 setFocusScale()/setFocusScaleEnabled()。
constexpr float kFocusScaleSeconds = 0.13f;
} // namespace

focus_ring::Outline AppearanceView::getFocusOutline() const
{
    // 几何交给基类（世界矩形 + 变换链），这里只补"我有多圆"。
    focus_ring::Outline outline = View::getFocusOutline();
    outline.cornerRadius        = this->getCornerRadius();
    return outline;
}

float AppearanceView::getCornerRadius() const
{
    // 取 bg 层的圆角：外观里 radius 写到 >= 短边一半时，这里读出来就是"圆"，
    // 于是聚焦框自然变成圆框 —— 形状不用逐控件声明。
    const std::vector<AppearanceLayer>& layers = this->appearance.getAppearance().getLayers();
    for (size_t index = 0; index < layers.size(); index++)
    {
        if (layers[index].id == "bg")
            return this->appearance.getNumber((int)index, AppearanceProp::RADIUS);
    }
    return 0.0f;
}

void AppearanceView::updateFocusScale()
{
    if (!this->focusScaleEnabled)
    {
        if (this->focusScaleApplied != 1.0f)
        {
            this->focusScaleApplied = 1.0f;
            this->setScaleX(1.0f);
            this->setScaleY(1.0f);
        }
        return;
    }

    const bool focused = this->isFocused();
    if (!this->focusScaleInitialized)
    {
        this->focusScaleInitialized = true;
        this->focusScaleWasFocused  = focused;
        this->focusScaleAnim.reset(1.0f);
    }

    if (focused != this->focusScaleWasFocused)
    {
        this->focusScaleWasFocused = focused;
        this->focusScaleAnim.addStep(focused ? this->focusScaleTarget : 1.0f, kFocusScaleSeconds,
            EasingFunction::QUADRATIC_OUT);
        this->focusScaleAnim.start();
    }

    if (this->focusScaleAnim.isStopped())
        return;

    this->focusScaleAnim.update();

    const float scale = this->focusScaleAnim.getValue();
    if (std::abs(scale - this->focusScaleApplied) <= 1e-4f)
        return;

    this->focusScaleApplied = scale;
    this->setScaleX(scale);
    this->setScaleY(scale);
}

void AppearanceView::enableContentMeasurement()
{
    // 固有尺寸 = 各层里最大的那个"内容尺寸 + 两侧 inset"。
    // 文字层的宽度随字号变，所以字号改了要 markMeasureDirty（applyTheme/setText 里会做）。
    this->setMeasureFunction([this](const void*, float width, YGMeasureMode widthMode, float,
                                 YGMeasureMode heightMode) -> YGSize {
        const Appearance& current = this->appearance.getAppearance();

        float intrinsicWidth  = 0.0f;
        float intrinsicHeight = 0.0f;
        float sumWidth        = 0.0f;

        for (const AppearanceLayer& layer : current.getLayers())
        {
            float layerWidth  = layer.width;
            float layerHeight = layer.height;

            ImFont* font = this->getLayerFont(layer);
            if (layer.op == AppearanceLayer::Op::TEXT && font != nullptr)
            {
                const std::string content = this->getLayerText(layer.source);
                if (!content.empty())
                {
                    const float size = layer.fontSize > 0.0f ? layer.fontSize : 15.0f;
                    const ImVec2 measured =
                        font->CalcTextSizeA(size, FLT_MAX, 0.0f, content.c_str());
                    layerWidth  = measured.x;
                    layerHeight = measured.y;
                }
            }
            else if (layer.op == AppearanceLayer::Op::IMAGE)
            {
                const TextureRef texture = this->getLayerImage(layer.source);
                if (texture.isValid() && layerWidth <= 0.0f)
                    layerWidth = (float)texture.width;
                if (texture.isValid() && layerHeight <= 0.0f)
                    layerHeight = (float)texture.height;
            }

            const float layerOuterWidth = layerWidth + layer.inset[1] + layer.inset[3];
            intrinsicWidth  = std::max(intrinsicWidth, layerOuterWidth);
            intrinsicHeight = std::max(intrinsicHeight, layerHeight + layer.inset[0] + layer.inset[2]);

            // 一行里放多个内容层（图标 + 文字 + 信息）时宽度要**累加**：只取最大值会让
            // 文字溢出控件。组合外观在 box 里写 measureContent: "sum" 打开这条。
            if (layer.op == AppearanceLayer::Op::TEXT || layer.op == AppearanceLayer::Op::IMAGE)
                sumWidth += layerOuterWidth;
        }

        if (current.getBox().measureSum && sumWidth > 0.0f)
            intrinsicWidth = std::max(intrinsicWidth, sumWidth);

        // 受约束时收缩到可用宽度（文字层会按自己的规则截断，这里只上报尺寸）
        if (widthMode != YGMeasureModeUndefined && !std::isnan(width))
            intrinsicWidth = std::min(intrinsicWidth, width);

        YGSize size;
        size.width  = intrinsicWidth;
        size.height = intrinsicHeight;
        (void)heightMode;
        return size;
    });
}

void AppearanceView::setText(const std::string& value)
{
    if (this->text == value)
        return;

    this->text = value;

    // 文字变了，固有尺寸跟着变（外观驱动控件靠测量函数定宽）
    this->markMeasureDirty();
    this->invalidate();
}

void AppearanceView::setAppearanceName(const std::string& name)
{
    if (this->appearanceName == name)
        return;

    this->appearanceName = name;
    this->applyTheme();
}

void AppearanceView::setAppearanceVariant(const std::string& value)
{
    if (this->variant == value)
        return;

    this->variant = value;
    this->applyTheme();
}

bool AppearanceView::applyAppearanceJson(const std::string& json, std::string& error)
{
    std::shared_ptr<const Appearance> compiled = Appearance::compile(json, getTheme(), error);
    if (compiled == nullptr)
        return false;

    this->appearance.setAppearance(compiled);
    applyAppearanceBox(this, compiled->getBox());

    // 状态按当前活跃情况重新补一遍（和换肤同一条路径）
    if (this->isAppearanceStateActive("focused"))
        this->appearance.setState("focused", true);

    this->markMeasureDirty();
    this->invalidate();
    return true;
}

void AppearanceView::applyTheme()
{
    // 换肤时重新取外观。AppearanceInstance 会抄新初值并清掉正在跑的补间，
    // 然后按当前活跃状态重新补——所以换肤过程中按钮不会卡在中间色。
    const std::string name = this->variant.empty() ? this->appearanceName
                                                   : this->appearanceName + "." + this->variant;
    std::shared_ptr<const Appearance> loaded = getWidgetAppearance(name);
    this->appearance.setAppearance(loaded);

    // "缺失"看的是有没有装载到外观，**不是"层数为 0"**：
    // 纯 box 的外观（如 status_area：只要布局不要装饰）层数本来就是 0，
    // 用层数判断会把它误报成"主题里没有这个外观"。
    if (loaded == nullptr)
        BKLV_LOG_WARNING("主题里没有控件外观 \"%s\"（%s/%s.json）", name.c_str(),
            getTheme().getName().c_str(), name.c_str());

    // 状态在换肤后重新打开，让新外观按当前状态补到位
    if (this->isAppearanceStateActive("focused"))
        this->appearance.setState("focused", true);

    // 盒模型/布局样式也来自同一份 JSON（顶层 "box" 段）：
    // 只套用写了的字段，所以控件在代码里设的值不会被空 box 冲掉。
    applyAppearanceBox(this, this->appearance.getAppearance().getBox());

    // 字号/内容可能变了，固有尺寸要重算
    this->markMeasureDirty();
    this->invalidate();
}

void AppearanceView::setAppearanceState(const std::string& state, bool active)
{
    this->appearance.setState(state, active);
}

bool AppearanceView::isAppearanceStateActive(const std::string& state) const
{
    return this->appearance.isStateActive(state);
}

void AppearanceView::onFocusGained()
{
    this->setAppearanceState("focused", true);
}

void AppearanceView::onFocusLost()
{
    this->setAppearanceState("focused", false);
    this->setAppearanceState("pressed", false);
    this->pressed = false;
}

void AppearanceView::update()
{
    // AppearanceView 也是容器（底栏里装提示栏/状态区、页头里装图标与标题、
    // 弹窗面板里装按钮行），必须先递归子树——不递归的话子树永远不会 update
    Box::update();

    // 聚焦反馈：轻微放大（画框在浮层上，这边只管"变大一点"）
    this->updateFocusScale();

    if (!this->appearance.isAnimating())
        return;

    // 补间在跑：用帧间隔推进（tweeny 内部按毫秒插值）。
    // getFrameDeltaSeconds() 在没有 ImGui 上下文时给 60fps 的估算值，
    // 所以无窗口测试里也能让动效往前走。
    this->appearance.update(getFrameDeltaSeconds());
}

void AppearanceView::setIcon(const std::string& glyph)
{
    if (this->icon == glyph)
        return;

    this->icon = glyph;
    this->markMeasureDirty();
    this->invalidate();
}

void AppearanceView::setInfoText(const std::string& value)
{
    if (this->infoText == value)
        return;

    this->infoText = value;
    this->markMeasureDirty();
    this->invalidate();
}

std::string AppearanceView::getLayerText(const std::string& source)
{
    if (source == "text")
        return this->text;
    if (source == "icon")
        return this->icon;
    if (source == "info")
        return this->infoText;
    return {};
}

TextureRef AppearanceView::getLayerImage(const std::string&)
{
    return TextureRef();
}

ImFont* AppearanceView::iconFontFor(IconFontKind kind)
{
    // 注意：这里的 getIconFont() 是 core/font.hpp 的自由函数（switch 那套）；
    // icon::font() 是 Material。两套都可能还没装好，返回 nullptr 让调用方回退。
    return kind == IconFontKind::SWITCH ? bklv::getIconFont() : icon::font();
}

void AppearanceView::setLayerIconFontKind(const std::string& layerId, IconFontKind kind)
{
    if (layerId.empty())
        return;

    this->layerIconFontKinds[layerId] = kind;

    // 字体变了要重新测量（字宽不一样）
    this->markMeasureDirty();
}

void AppearanceView::clearLayerIconFontKind(const std::string& layerId)
{
    if (this->layerIconFontKinds.erase(layerId) > 0)
        this->markMeasureDirty();
}

bool AppearanceView::hasLayerIconFontKind(const std::string& layerId) const
{
    return this->layerIconFontKinds.find(layerId) != this->layerIconFontKinds.end();
}

IconFontKind AppearanceView::getLayerIconFontKind(const std::string& layerId) const
{
    const auto found = this->layerIconFontKinds.find(layerId);
    return found != this->layerIconFontKinds.end() ? found->second : this->iconFontKind;
}

ImFont* AppearanceView::getLayerFont(const AppearanceLayer& layer)
{
    // 图标层必须用图标字体：私有区字形不在正文字体里，用错会量错尺寸、画出方块。
    // 哪一套由"这一层的 kind"决定：显式指定过就按指定的，否则跟随 setIconFontKind 的全局那套。
    const bool isIconLayer = layer.id == "icon" || layer.source == "icon";
    if (isIconLayer || this->hasLayerIconFontKind(layer.id))
    {
        if (ImFont* font = iconFontFor(this->getLayerIconFontKind(layer.id)))
            return font;
    }
    return getDefaultFont();
}

void AppearanceView::layerRect(const AppearanceLayer& layer, float x, float y, float width,
    float height, float out[4]) const
{
    float left   = x + layer.inset[3] + layer.offset[0];
    float top    = y + layer.inset[0] + layer.offset[1];
    float right  = x + width - layer.inset[1] + layer.offset[0];
    float bottom = y + height - layer.inset[2] + layer.offset[1];

    if (right < left)
        right = left;
    if (bottom < top)
        bottom = top;

    // 显式宽高：改尺寸后按 align/valign 在原来的范围里摆放。
    // 装饰件靠这个表达：色条 width=4、下边线 height=1 + valign=bottom。
    if (layer.width > 0.0f)
    {
        if (enumMatches(layer.align, "center"))
        {
            const float center = (left + right) * 0.5f;
            left               = center - layer.width * 0.5f;
            right              = left + layer.width;
        }
        else if (enumMatches(layer.align, "right"))
        {
            left = right - layer.width;
        }
        else
        {
            right = left + layer.width;
        }
    }

    if (layer.height > 0.0f)
    {
        if (enumMatches(layer.valign, "center"))
        {
            const float middle = (top + bottom) * 0.5f;
            top                = middle - layer.height * 0.5f;
            bottom             = top + layer.height;
        }
        else if (enumMatches(layer.valign, "bottom"))
        {
            top = bottom - layer.height;
        }
        else
        {
            bottom = top + layer.height;
        }
    }

    out[0] = left;
    out[1] = top;
    out[2] = std::max(left, right);
    out[3] = std::max(top, bottom);
}

void AppearanceView::draw(ImDrawList* drawList, float x, float y, float width, float height, FrameContext*)
{
    layers::ensureLayerDrawsRegistered(); // 幂等

    if (width <= 0.0f || height <= 0.0f)
        return;

    const Appearance& current = this->appearance.getAppearance();
    const std::vector<AppearanceLayer>& layers = current.getLayers();
    if (layers.empty())
        return;

    for (size_t i = 0; i < layers.size(); i++)
    {
        const AppearanceLayer& layer = layers[i];
        const int index              = (int)i;

        // 圆角优先级：**代码显式设的 > 外观里的 > 控件自己的（0 时才用）
        // 以前只读外观，代码里的 setCornerRadius(60) 完全不生效
        // （想画圆形头像，画出来是圆角方块 —— 实测踩过）。
        float radius = this->appearance.getNumber(index, AppearanceProp::RADIUS);
        if (this->isCornerRadiusExplicit() || radius <= 0.0f)
            radius = this->getCornerRadius();
        const float opacity     = this->appearance.getNumber(index, AppearanceProp::OPACITY);

        float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        this->layerRect(layer, x, y, width, height, rect);

        // 撞边抖动只作用在高亮层（hlBg / hlGlow / hlBorder，和 borealis 的
        // drawHighlight 一致）：纯绘制期位移，不进布局、不改命中矩形。
        if (layer.id.compare(0, 2, "hl") == 0)
        {
            const Point shake = this->getHighlightShakeOffset();
            if (shake.x != 0.0f || shake.y != 0.0f)
            {
                rect[0] += shake.x;
                rect[1] += shake.y;
                rect[2] += shake.x;
                rect[3] += shake.y;
            }
        }

        // 逐层透明度：和控件自身的 alpha 相乘
        const float alphaScale = std::max(0.0f, std::min(1.0f, opacity)) * this->getAlpha();

        switch (layer.op)
        {
            case AppearanceLayer::Op::RECT:
            {
                // ③：内置层走注册表钩子（原地实现保留为兜底，全部搬完后再删）
                if (const compile::LayerType* rectType = compile::findLayer("rect");
                    rectType != nullptr && rectType->draw)
                {
                    rectType->draw(compile::LayerDrawContext{ layer, this->appearance, (int)index,
                        drawList,
                        { rect[0], rect[1], rect[2], rect[3],
                            this->appearance.getNumber(index, AppearanceProp::RADIUS), alphaScale },
                        this });
                    break;
                }
            }

            case AppearanceLayer::Op::GLASS:
            {
                // ③：走注册表钩子（原地实现留作兜底）
                if (const compile::LayerType* glassType = compile::findLayer("glass");
                    glassType != nullptr && glassType->draw)
                {
                    glassType->draw(compile::LayerDrawContext{ layer, this->appearance, (int)index,
                        drawList,
                        { rect[0], rect[1], rect[2], rect[3],
                            this->appearance.getNumber(index, AppearanceProp::RADIUS), alphaScale },
                        this });
                    break;
                }
            }

            case AppearanceLayer::Op::SHADOW:
            {
                // ③：走注册表钩子（原地实现留作兜底）
                if (const compile::LayerType* shadowType = compile::findLayer("shadow");
                    shadowType != nullptr && shadowType->draw)
                {
                    shadowType->draw(compile::LayerDrawContext{ layer, this->appearance, (int)index,
                        drawList,
                        { rect[0], rect[1], rect[2], rect[3],
                            this->appearance.getNumber(index, AppearanceProp::RADIUS), alphaScale },
                        this });
                    break;
                }
            }

            case AppearanceLayer::Op::TEXT:
            {
                // ③：走注册表钩子（原地实现留作兜底）。内容槽在这里现算，行为与原来一致
                if (const compile::LayerType* textType = compile::findLayer("text");
                    textType != nullptr && textType->draw)
                {
                    const std::string textContent = this->getLayerText(layer.source);
                    ImFont* layerFont = this->getLayerFont(layer);
                    textType->draw(compile::LayerDrawContext{ layer, this->appearance, (int)index,
                        drawList,
                        { rect[0], rect[1], rect[2], rect[3],
                            this->appearance.getNumber(index, AppearanceProp::RADIUS), alphaScale },
                        this, &textContent, layerFont, nullptr, nullptr });
                    break;
                }
            }

            case AppearanceLayer::Op::IMAGE:
            {
                // ③：走注册表钩子（原地实现留作兜底）。图与有效性在这里现算，行为与原来一致
                if (const compile::LayerType* imageType = compile::findLayer("image");
                    imageType != nullptr && imageType->draw)
                {
                    const TextureRef layerImage = this->getLayerImage(layer.source);
                    imageType->draw(compile::LayerDrawContext{ layer, this->appearance, (int)index,
                        drawList,
                        { rect[0], rect[1], rect[2], rect[3],
                            this->appearance.getNumber(index, AppearanceProp::RADIUS), alphaScale },
                        this, nullptr, nullptr,
                        layerImage.isValid() ? (void*)layerImage.texture : nullptr, nullptr });
                    break;
                }
            }

            case AppearanceLayer::Op::EXTENSION:
            {
                // 扩展 op：绘制交给**注册表里的 draw 钩子**（第三方注册时提供）。
                // 引擎这里不写任何 `if (op == ...)`，所以别人加新层级不用改这个文件。
                const compile::LayerType* type = compile::findLayer(layer.extensionType);
                if (type == nullptr || !type->draw)
                    break;
                if (alphaScale <= 0.001f)
                    break;

                compile::LayerRect hookRect;
                hookRect.minX   = rect[0];
                hookRect.minY   = rect[1];
                hookRect.maxX   = rect[2];
                hookRect.maxY   = rect[3];
                hookRect.radius = this->appearance.getNumber(index, AppearanceProp::RADIUS);
                hookRect.alpha  = alphaScale;

                compile::LayerDrawContext ctx{ layer, this->appearance, (int)index, drawList,
                    hookRect, this };
                type->draw(ctx);
                break;
            }

            case AppearanceLayer::Op::CUSTOM:
                // ③：走注册表钩子（原地实现留作兜底）
                if (const compile::LayerType* customType = compile::findLayer("custom");
                    customType != nullptr && customType->draw)
                {
                    customType->draw(compile::LayerDrawContext{ layer, this->appearance, (int)index,
                        drawList,
                        { rect[0], rect[1], rect[2], rect[3],
                            this->appearance.getNumber(index, AppearanceProp::RADIUS), alphaScale },
                        this, nullptr, nullptr, nullptr, nullptr });
                    break;
                }
                this->drawCustomLayer(drawList, layer, rect[0], rect[1], rect[2] - rect[0],
                    rect[3] - rect[1], this->appearance);
                break;
        }
    }
}

} // namespace bklv

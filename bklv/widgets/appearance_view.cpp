#include "bklv/widgets/appearance_view.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "bklv/core/font.hpp"
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

            intrinsicWidth  = std::max(intrinsicWidth, layerWidth + layer.inset[1] + layer.inset[3]);
            intrinsicHeight = std::max(intrinsicHeight, layerHeight + layer.inset[0] + layer.inset[2]);
        }

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

    if (!this->appearance.isAnimating())
        return;

    // 补间在跑：用帧间隔推进（tweeny 内部按毫秒插值）。
    // getFrameDeltaSeconds() 在没有 ImGui 上下文时给 60fps 的估算值，
    // 所以无窗口测试里也能让动效往前走。
    this->appearance.update(getFrameDeltaSeconds());
}

std::string AppearanceView::getLayerText(const std::string& source)
{
    if (source == "text")
        return this->text;
    return {};
}

TextureRef AppearanceView::getLayerImage(const std::string&)
{
    return TextureRef();
}

ImFont* AppearanceView::getLayerFont(const AppearanceLayer&)
{
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

        const ImU32 fill        = this->appearance.getColor(index, AppearanceProp::FILL);
        const ImU32 stroke      = this->appearance.getColor(index, AppearanceProp::STROKE);
        const ImU32 textColor   = this->appearance.getColor(index, AppearanceProp::TEXT_COLOR);
        const float strokeWidth = this->appearance.getNumber(index, AppearanceProp::STROKE_WIDTH);
        // 圆角优先级：**代码显式设的 > 外观里的 > 控件自己的（0 时才用）
        // 以前只读外观，代码里的 setCornerRadius(60) 完全不生效
        // （想画圆形头像，画出来是圆角方块 —— 实测踩过）。
        float radius = this->appearance.getNumber(index, AppearanceProp::RADIUS);
        if (this->isCornerRadiusExplicit() || radius <= 0.0f)
            radius = this->getCornerRadius();
        const float opacity     = this->appearance.getNumber(index, AppearanceProp::OPACITY);
        const float fontSize    = this->appearance.getNumber(index, AppearanceProp::FONT_SIZE);

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
        auto scaleAlpha        = [alphaScale](ImU32 color) {
            const unsigned int a = (color >> 24) & 0xFF;
            const unsigned int scaled = (unsigned int)std::round((float)a * alphaScale);
            return (color & 0x00FFFFFFu) | ((scaled & 0xFFu) << 24);
        };

        switch (layer.op)
        {
            case AppearanceLayer::Op::RECT:
            {
                if (alphaScale <= 0.001f)
                    break;
                if ((fill >> 24) != 0)
                    drawList->AddRectFilled(ImVec2(rect[0], rect[1]), ImVec2(rect[2], rect[3]),
                        scaleAlpha(fill), radius);
                if (strokeWidth > 0.0f && (stroke >> 24) != 0)
                    drawList->AddRect(ImVec2(rect[0], rect[1]), ImVec2(rect[2], rect[3]),
                        scaleAlpha(stroke), radius, 0, strokeWidth);
                break;
            }

            case AppearanceLayer::Op::SHADOW:
            {
                if (alphaScale <= 0.001f || (fill >> 24) == 0)
                    break;

                // 阴影形状 = 层矩形（inset/width/height/align 都按层语法来），再按 offset 平移；
                // 这几个值都从 AppearanceInstance 取，所以状态动效（offsetY / feather / spread）算数
                const float feather = this->appearance.getNumber(index, AppearanceProp::FEATHER);
                const float spread  = this->appearance.getNumber(index, AppearanceProp::SPREAD);
                const float offsetX = this->appearance.getNumber(index, AppearanceProp::OFFSET_X);
                const float offsetY = this->appearance.getNumber(index, AppearanceProp::OFFSET_Y);

                shadow::drawSoft(drawList, rect[0] + offsetX, rect[1] + offsetY,
                    rect[2] + offsetX, rect[3] + offsetY, radius, fill, alphaScale, feather,
                    spread, layer.cutout);
                break;
            }

            case AppearanceLayer::Op::TEXT:
            {
                ImFont* font = this->getLayerFont(layer);
                const std::string content = this->getLayerText(layer.source);
                if (content.empty() || font == nullptr || alphaScale <= 0.001f)
                    break;

                const float size = fontSize > 0.0f ? fontSize : 15.0f;
                const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, content.c_str());

                float textX = rect[0];
                if (enumMatches(layer.align, "center"))
                    textX = rect[0] + ((rect[2] - rect[0]) - textSize.x) * 0.5f;
                else if (enumMatches(layer.align, "right"))
                    textX = rect[2] - textSize.x;

                float textY = rect[1];
                if (enumMatches(layer.valign, "center"))
                    textY = rect[1] + ((rect[3] - rect[1]) - textSize.y) * 0.5f;
                else if (enumMatches(layer.valign, "bottom"))
                    textY = rect[3] - textSize.y;

                drawList->AddText(font, size, ImVec2(textX, textY), scaleAlpha(textColor), content.c_str());
                break;
            }

            case AppearanceLayer::Op::IMAGE:
            {
                const TextureRef texture = this->getLayerImage(layer.source);
                if (!texture.isValid() || alphaScale <= 0.001f)
                    break;

                const ImVec2 min(rect[0], rect[1]);
                const ImVec2 max(rect[2], rect[3]);
                const ImU32 tint = scaleAlpha(IM_COL32(255, 255, 255, 255));

                if (radius > 0.0f)
                    drawList->AddImageRounded(texture.texture, min, max, ImVec2(0, 0), ImVec2(1, 1), tint,
                        radius);
                else
                    drawList->AddImage(texture.texture, min, max, ImVec2(0, 0), ImVec2(1, 1), tint);
                break;
            }

            case AppearanceLayer::Op::CUSTOM:
                this->drawCustomLayer(drawList, layer, rect[0], rect[1], rect[2] - rect[0],
                    rect[3] - rect[1], this->appearance);
                break;
        }
    }
}

} // namespace bklv

#include "bklv/ui/widgets/appearance/appearance_view.hpp"

#include "bklv/ui/widgets/appearance/content_ids.hpp"

#include "bklv/graphics/render/glass/glass_service.hpp"
#include "bklv/ui/widgets/appearance/appearance_children.hpp"
#include "bklv/ui/layer/layer_draw.hpp"
#include "bklv/ui/theme/layer_registry.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "bklv/system/media/font.hpp"
#include "bklv/system/media/icon_font.hpp"
#include "bklv/graphics/render/graphics.hpp"
#include "bklv/foundation/core/logger.hpp"
#include "bklv/graphics/paint/shadow.hpp"
#include "bklv/foundation/anim/time.hpp"
#include "bklv/foundation/core/resource.hpp"
#include "bklv/ui/theme/theme_data.hpp"

namespace bklv
{

namespace
{

// CUSTOM 层的语义绘制：内置层的绘制实现（bklv/ui/layer/）只认上下文里的这个回调，
// 不认识 AppearanceView —— 否则就是 layer → widgets 的反向依赖。
void customDrawAdapter(const compile::LayerDrawContext& ctx)
{
    auto* view = static_cast<AppearanceView*>(ctx.view);
    if (view == nullptr)
        return;
    view->drawCustomLayer(static_cast<ImDrawList*>(ctx.drawList), ctx.layer, ctx.rect.minX,
        ctx.rect.minY, ctx.rect.maxX - ctx.rect.minX, ctx.rect.maxY - ctx.rect.minY, ctx.instance);
}

} // namespace

AppearanceView::AppearanceView(std::string appearanceName, bool focusable)
    : Box(Axis::COLUMN) // 外观驱动控件也是容器：弹窗要在里面放按钮行
    , appearanceName(std::move(appearanceName))
    , focusable(focusable)
{
    // 默认不装测量函数：装了之后 Yoga 不许这个节点再有子节点。
    // 需要"按内容定尺寸"的叶子控件（Button / TabButton / Header …）自己调 enableContentMeasurement()。

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

    // 没有 bg 层（只画 border / shadow 的外观，例如 tabitem）时退回 box 段的
    // cornerRadius（也包含代码里 setCornerRadius 显式设的值）—— 否则聚焦框会被
    // 画成直角矩形，和控件视觉上的圆角对不上。注意 box 里的键叫 cornerRadius，
    // "radius" 只在 layers 的层里有效。
    return View::getCornerRadius();
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
                const std::string content = this->getLayerText(layer.dataId);
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
                const TextureRef texture = this->resolveLayerImage(layer);
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

AppearanceView::ContentSlot* AppearanceView::findSlot(const std::string& id)
{
    for (ContentSlot& slot : this->contentSlots)
        if (slot.id == id)
            return &slot;
    return nullptr;
}

const AppearanceView::ContentSlot* AppearanceView::findSlot(const std::string& id) const
{
    for (const ContentSlot& slot : this->contentSlots)
        if (slot.id == id)
            return &slot;
    return nullptr;
}

bool AppearanceView::hasSlot(const std::string& id) const
{
    return this->findSlot(id) != nullptr;
}

const std::string& AppearanceView::getSlotText(const std::string& id) const
{
    static const std::string empty;
    const ContentSlot* slot = this->findSlot(id);
    return slot != nullptr ? slot->text : empty;
}

void AppearanceView::setText(const std::string& id, const std::string& value)
{
    if (id.empty())
        return;

    ContentSlot* slot = this->findSlot(id);
    if (slot == nullptr)
    {
        ContentSlot created;
        created.id   = id;
        created.kind = ContentSlot::Kind::TEXT;
        this->contentSlots.push_back(created);
        slot = &this->contentSlots.back();
    }
    if (slot->kind == ContentSlot::Kind::TEXT && slot->text == value)
        return;

    slot->kind = ContentSlot::Kind::TEXT;
    slot->text = value;
    slot->image = TextureRef();

    this->markMeasureDirty();
    this->invalidate();
}

void AppearanceView::setIcon(const std::string& id, const std::string& glyph)
{
    if (id.empty())
        return;

    ContentSlot* slot = this->findSlot(id);
    if (slot == nullptr)
    {
        ContentSlot created;
        created.id = id;
        this->contentSlots.push_back(created);
        slot = &this->contentSlots.back();
    }
    if (slot->kind == ContentSlot::Kind::ICON && slot->text == glyph)
        return;

    slot->kind = ContentSlot::Kind::ICON;
    slot->text = glyph;
    slot->image = TextureRef();

    this->markMeasureDirty();
    this->invalidate();
}

void AppearanceView::setImage(const std::string& id, const TextureRef& texture)
{
    if (id.empty())
        return;

    ContentSlot* slot = this->findSlot(id);
    if (slot == nullptr)
    {
        ContentSlot created;
        created.id = id;
        this->contentSlots.push_back(created);
        slot = &this->contentSlots.back();
    }
    slot->kind  = ContentSlot::Kind::IMAGE;
    slot->image = texture;
    slot->text.clear();

    this->markMeasureDirty();
    this->invalidate();
}

void AppearanceView::clearSlot(const std::string& id)
{
    for (size_t i = 0; i < this->contentSlots.size(); i++)
    {
        if (this->contentSlots[i].id != id)
            continue;
        this->contentSlots.erase(this->contentSlots.begin() + (long)i);
        this->markMeasureDirty();
        this->invalidate();
        return;
    }
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

namespace
{

EasingFunction easingFromName(const std::string& name)
{
    if (enumMatches(name, "linear")) return EasingFunction::LINEAR;
    if (enumMatches(name, "quadraticIn")) return EasingFunction::QUADRATIC_IN;
    if (enumMatches(name, "quadraticOut")) return EasingFunction::QUADRATIC_OUT;
    if (enumMatches(name, "quadraticInOut")) return EasingFunction::QUADRATIC_IN_OUT;
    if (enumMatches(name, "cubicIn")) return EasingFunction::CUBIC_IN;
    if (enumMatches(name, "cubicOut")) return EasingFunction::CUBIC_OUT;
    if (enumMatches(name, "cubicInOut")) return EasingFunction::CUBIC_IN_OUT;
    if (enumMatches(name, "quarticOut")) return EasingFunction::QUARTIC_OUT;
    if (enumMatches(name, "quinticOut")) return EasingFunction::QUINTIC_OUT;
    if (enumMatches(name, "sinusoidalIn")) return EasingFunction::SINE_IN;
    if (enumMatches(name, "sinusoidalOut")) return EasingFunction::SINE_OUT;
    if (enumMatches(name, "sinusoidalInOut")) return EasingFunction::SINE_IN_OUT;
    if (enumMatches(name, "exponentialOut")) return EasingFunction::EXPONENTIAL_OUT;
    if (enumMatches(name, "circularOut")) return EasingFunction::CIRCULAR_OUT;
    if (enumMatches(name, "backOut")) return EasingFunction::BACK_OUT;
    if (enumMatches(name, "backInOut")) return EasingFunction::BACK_IN_OUT;
    if (enumMatches(name, "bounceOut")) return EasingFunction::BOUNCE_OUT;
    if (enumMatches(name, "elasticOut")) return EasingFunction::ELASTIC_OUT;
    return EasingFunction::LINEAR;
}

float defaultViewChannelValue(ViewChannel channel)
{
    switch (channel)
    {
        case ViewChannel::SCALE_X:
        case ViewChannel::SCALE_Y:
        case ViewChannel::ALPHA: return 1.0f;
        default: return 0.0f;
    }
}

} // namespace

void AppearanceView::refreshStateViewChannels()
{
    if (!this->stateViewInit)
    {
        this->stateViewInit = true;
        for (int i = 0; i < (int)ViewChannel::COUNT; i++)
            this->stateViewAnims[(size_t)i].reset(defaultViewChannelValue((ViewChannel)i));
    }

    const Appearance& spec = this->appearance.getAppearance();

    for (int i = 0; i < (int)ViewChannel::COUNT; i++)
    {
        const ViewChannel channel = (ViewChannel)i;
        const bool wasDriven = this->stateViewDriven[(size_t)i];

        float target       = defaultViewChannelValue(channel);
        float durationMs   = 0.0f;
        std::string easing = "linear";
        bool  hasSource    = false;
        float source       = 0.0f;
        bool  hasClip      = false;

        // 按激活顺序扫：后激活的状态覆盖先激活的（pressed 覆盖 focused）
        for (const std::string& state : this->activeStateOrder)
        {
            for (const ViewChannelClip& clip : spec.getStateViewClips(state))
            {
                if (clip.channel != channel)
                    continue;
                target     = clip.target;
                durationMs = clip.durationMs;
                easing     = clip.easing;
                hasSource  = clip.hasSource;
                source     = clip.source;
                hasClip    = true;
            }
        }

        Animatable& anim = this->stateViewAnims[(size_t)i];

        // 没有任何激活状态驱动这个通道：如果以前驱动过，平滑回到默认值
        if (!hasClip)
        {
            if (wasDriven && anim.getValue() != target)
            {
                anim.reset(anim.getValue());
                anim.addStep(target, 0.12f, EasingFunction::CUBIC_OUT);
                anim.start();
            }
            continue;
        }

        this->stateViewDriven[(size_t)i] = true;

        if (durationMs <= 0.0f)
        {
            anim.reset(hasSource ? source : target);
            continue;
        }

        // 先撤掉旧补间（否则排队会串），再从当前值起新的一段
        anim.reset(hasSource ? source : anim.getValue());
        anim.addStep(target, durationMs / 1000.0f, easingFromName(easing));
        anim.start();
    }
}

void AppearanceView::stepStateViewChannels(float deltaSeconds)
{
    if (deltaSeconds > 0.0f)
        for (Animatable& anim : this->stateViewAnims)
            if (!anim.isStopped())
                anim.update(deltaSeconds);

    // 只写"被状态接管过"的通道：没写 self 的控件连 1.0 的 scale 都不碰，
    // 否则会把内置的聚焦放大（focus scale）覆盖掉。
    auto apply = [this](ViewChannel channel, auto setter) {
        if (this->stateViewDriven[(size_t)channel])
            setter(this->stateViewAnims[(size_t)channel].getValue());
    };
    apply(ViewChannel::TRANSLATE_X, [this](float v) { this->setTranslateX(v); });
    apply(ViewChannel::TRANSLATE_Y, [this](float v) { this->setTranslateY(v); });
    apply(ViewChannel::SCALE_X, [this](float v) { this->setScaleX(v); });
    apply(ViewChannel::SCALE_Y, [this](float v) { this->setScaleY(v); });
    apply(ViewChannel::ALPHA, [this](float v) { this->setAlpha(v); });
    apply(ViewChannel::ROTATE, [this](float v) { this->setRotate(v); });
    apply(ViewChannel::SKEW_X, [this](float v) { this->setSkewX(v); });
    apply(ViewChannel::SKEW_Y, [this](float v) { this->setSkewY(v); });
}

void AppearanceView::setAppearanceState(const std::string& state, bool active)
{
    this->appearance.setState(state, active);

    // 维护激活顺序：激活时追加到末尾（= 优先级最高），关闭时移除
    const auto found = std::find(this->activeStateOrder.begin(), this->activeStateOrder.end(), state);
    if (active)
    {
        if (found == this->activeStateOrder.end())
            this->activeStateOrder.push_back(state);
    }
    else if (found != this->activeStateOrder.end())
    {
        this->activeStateOrder.erase(found);
    }

    this->refreshStateViewChannels();
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

    // 状态驱动的视图级通道（states.<状态>.self）；没写 self 时不碰任何变换
    this->stepStateViewChannels(getFrameDeltaSeconds());

    if (!this->appearance.isAnimating())
        return;

    // 补间在跑：用帧间隔推进（tweeny 内部按毫秒插值）。
    // getFrameDeltaSeconds() 在没有 ImGui 上下文时给 60fps 的估算值，
    // 所以无窗口测试里也能让动效往前走。
    this->appearance.update(getFrameDeltaSeconds());
}

namespace
{

void setAppearanceTextAttr(View* v, const ResolvedValue& r)
{
    static_cast<AppearanceView*>(v)->setText(content::TEXT, r.text);
}

void setAppearanceIconAttr(View* v, const ResolvedValue& r)
{
    static_cast<AppearanceView*>(v)->setIcon(content::ICON, r.text);
}

void setAppearanceInfoAttr(View* v, const ResolvedValue& r)
{
    static_cast<AppearanceView*>(v)->setText(content::INFO, r.text);
}

void setAppearanceIconFontAttr(View* v, const ResolvedValue& r)
{
    AppearanceView* view = static_cast<AppearanceView*>(v);
    if (enumMatches(r.text, "material"))
        view->setIconFontKind(IconFontKind::MATERIAL);
    else if (enumMatches(r.text, "switch"))
        view->setIconFontKind(IconFontKind::SWITCH);
    else
        warnUnknownEnumValue("iconFont", r.text, "material|switch");
}

} // namespace

const bklv::AttributeTable& AppearanceView::appearanceAttributes()
{
    static const AttributeTable table = {
        { "text", AttributeKind::TEXT, setAppearanceTextAttr, nullptr },
        { "icon", AttributeKind::TEXT, setAppearanceIconAttr, nullptr },
        { "info", AttributeKind::TEXT, setAppearanceInfoAttr, nullptr },
        { "iconFont", AttributeKind::ENUM, setAppearanceIconFontAttr, "material|switch" },
    };
    return table;
}

const bklv::AttributeTable& AppearanceView::attributes() const
{
    return appearanceAttributes();
}

void AppearanceView::setDesignSelection(int layerIndex, ImU32 color)
{
    this->designSelection      = layerIndex;
    this->designSelectionColor = color;
}

void AppearanceView::setDesignGuides(bool on, ImU32 color)
{
    this->designGuides      = on;
    this->designGuidesColor = color;
}

bool AppearanceView::getLayerRect(
    int index, float x, float y, float width, float height, float outRect[4]) const
{
    const std::vector<AppearanceLayer>& layers = this->getActiveAppearance().getLayers();
    if (layers.empty())
        return false;
    if (index < 0 || index >= (int)layers.size())
        return false;

    const AppearanceLayer& layer = layers[(size_t)index];
    float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    this->layerRect(layer, x, y, width, height, rect);
    for (int i = 0; i < 4; i++)
        outRect[i] = rect[i];
    return true;
}

std::string AppearanceView::getLayerText(const std::string& source)
{
    return this->getSlotText(source);
}

TextureRef AppearanceView::resolveLayerImage(const AppearanceLayer& layer)
{
    TextureRef texture = this->getLayerImage(layer.dataId);
    if (!texture.isValid() && !layer.src.empty())
        texture = loadTexture(resourcePath(layer.src));
    return texture;
}

TextureRef AppearanceView::getLayerImage(const std::string& source)
{
    const ContentSlot* slot = this->findSlot(source);
    return slot != nullptr ? slot->image : TextureRef();
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
    // 字体只由**层上声明**决定（font: "icon" / "text"），通用类不认识具体槽 id。
    // 显式设过 kind 的层（setLayerIconFontKind）也按图标字体处理。
    if (layer.fontKind == "text")
        return getDefaultFont();
    if (layer.fontKind == "icon" || this->hasLayerIconFontKind(layer.id))
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

    // 设计期叠加：用**绘制期同一个 rect**（下面循环里逐层算出来的那个），
    // 不用 getX/getWidth —— 两者在绘制空间里不是一个量，混用会画到别处。
    float selectedRect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool  hasSelectedRect  = false;
    float contentRect[4]   = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool  hasContentRect   = false;

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

        float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // [left, top, right, bottom]
        this->layerRect(layer, x, y, width, height, rect);

        // 状态里补间的 inset：layerRect 用的是静态 inset（作为初值），这里补差值。
        // 之前只解析不应用，导致 insetLeft 之类的位移动画完全看不到效果。
        {
            const float dLeft = this->appearance.getNumber(index, AppearanceProp::INSET_LEFT)
                - layer.inset[3];
            const float dRight = this->appearance.getNumber(index, AppearanceProp::INSET_RIGHT)
                - layer.inset[1];
            const float dTop = this->appearance.getNumber(index, AppearanceProp::INSET_TOP)
                - layer.inset[0];
            const float dBottom = this->appearance.getNumber(index, AppearanceProp::INSET_BOTTOM)
                - layer.inset[2];
            if (dLeft != 0.0f)
            {
                rect[0] += dLeft;
                rect[2] += dLeft;
            }
            if (dRight != 0.0f)
            {
                rect[0] -= dRight;
                rect[2] -= dRight;
            }
            if (dTop != 0.0f)
            {
                rect[1] += dTop;
                rect[3] += dTop;
            }
            if (dBottom != 0.0f)
            {
                rect[1] -= dBottom;
                rect[3] -= dBottom;
            }
        }

        // 状态里补间的 offsetX/offsetY：layerRect 用的是静态 offset（作为初值），
        // 这里按「动画值 - 静态值」平移绘制矩形。之前只解析不应用，导致
        // Toggle 的旋钮只有颜色变、位置不动。
        const float animOffsetX = this->appearance.getNumber(index, AppearanceProp::OFFSET_X)
            - layer.offset[0];
        const float animOffsetY = this->appearance.getNumber(index, AppearanceProp::OFFSET_Y)
            - layer.offset[1];
        if (animOffsetX != 0.0f || animOffsetY != 0.0f)
        {
            rect[0] += animOffsetX;
            rect[2] += animOffsetX;
            rect[1] += animOffsetY;
            rect[3] += animOffsetY;
        }

        // 设计期叠加：记下被选中层的绘制矩形，以及所有层的并集（画板边框用）
        if ((int)i == this->designSelection)
        {
            std::copy(rect, rect + 4, selectedRect);
            hasSelectedRect = true;
        }
        if (!hasContentRect)
        {
            std::copy(rect, rect + 4, contentRect);
            hasContentRect = true;
        }
        else
        {
            contentRect[0] = std::min(contentRect[0], rect[0]);
            contentRect[1] = std::min(contentRect[1], rect[1]);
            contentRect[2] = std::max(contentRect[2], rect[2]);
            contentRect[3] = std::max(contentRect[3], rect[3]);
        }

        // 撞边抖动只作用在高亮层（约定用 hl 前缀命名，如 hlBg / hlGlow / hlBorder，
        // 和 borealis 的 drawHighlight 一致）：纯绘制期位移，不进布局、不改命中矩形。
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

        // 这一层是哪个类型：内置走**数组下标查表**，扩展按名字查注册表。
        // 以前每个 case 里都写 `compile::findLayer("rect")` —— 每层每帧都要构造一次
        // std::string 再做哈希查找；现在内置只剩一次数组索引。
        const compile::LayerType* layerType = layer.op == AppearanceLayer::Op::EXTENSION
            ? compile::findLayer(layer.extensionType)
            : compile::builtinLayer(layer.op);

        // 钩子上下文：内容槽（文字 / 字体 / 图）由各分支按层填。
        // type 一起带进去 —— 扩展层靠它把 layer.params[i] 对应回参数名与类型。
        auto hookContext = [&](const std::string* text, void* font, void* image,
                               int imageWidth = 0, int imageHeight = 0) {
            return compile::LayerDrawContext{ layer, this->appearance, index, drawList,
                { rect[0], rect[1], rect[2], rect[3],
                    this->appearance.getNumber(index, AppearanceProp::RADIUS), alphaScale },
                this, text, font, image, imageWidth, imageHeight, &customDrawAdapter, layerType };
        };

        // 没有钩子就什么都不画：注册表是绘制层的**唯一出口**。唯一例外是 CUSTOM ——
        // 控件可以自己实现 drawCustomLayer（原地兜底，见下面的分支）。
        if (layerType == nullptr || !layerType->draw)
        {
            if (layer.op == AppearanceLayer::Op::CUSTOM)
                this->drawCustomLayer(drawList, layer, rect[0], rect[1], rect[2] - rect[0],
                    rect[3] - rect[1], this->appearance);
            continue;
        }

        if (layer.clip)
            drawList->PushClipRect(ImVec2(rect[0], rect[1]), ImVec2(rect[2], rect[3]), true);

        switch (layer.op)
        {
            case AppearanceLayer::Op::TEXT:
            {
                // 内容槽在这里现算：getLayerText 返回 std::string，要活到钩子调用结束
                const std::string textContent = this->getLayerText(layer.dataId);
                layerType->draw(hookContext(&textContent, this->getLayerFont(layer), nullptr));
                break;
            }

            case AppearanceLayer::Op::IMAGE:
            {
                // 图与"是否有效"在这里现算；TextureRef 同样要活到调用之后
                const TextureRef layerImage = this->resolveLayerImage(layer);
                layerType->draw(hookContext(nullptr, nullptr,
                    layerImage.isValid() ? (void*)layerImage.texture : nullptr,
                    layerImage.width, layerImage.height));
                break;
            }

            case AppearanceLayer::Op::EXTENSION:
                // 扩展层先替它确认这一层可见：第三方钩子不一定会查 alpha
                if (alphaScale <= 0.001f)
                    break;
                layerType->draw(hookContext(nullptr, nullptr, nullptr));
                break;

            default: // RECT / SHADOW / GLASS / CUSTOM
                layerType->draw(hookContext(nullptr, nullptr, nullptr));
                break;
        }

        if (layer.clip)
            drawList->PopClipRect();
    }

    // ---- 设计期叠加（默认关闭）：画板边框 + 选中层描边与四角把手 ----
    // 放在**所有层之后**：它是"标注"，不该被内容盖住；也不参与布局、不影响运行期绘制。
    // ---- 设计期叠加（默认关闭）：画板边框 + 选中层描边与四角把手 ----
    // 用**绘制期算出来的 rect**（[left, top, right, bottom]），和绘制同源，天然对齐。
    if (this->designGuides && hasContentRect)
    {
        const ImU32 color = this->designGuidesColor != 0 ? this->designGuidesColor : 0x66FFFFFF;
        drawList->AddRect(ImVec2(contentRect[0], contentRect[1]),
            ImVec2(contentRect[2], contentRect[3]), color, 0.0f, 0, 1.0f);
    }
    if (this->designSelection >= 0 && hasSelectedRect)
    {
        const ImU32 color
            = this->designSelectionColor != 0 ? this->designSelectionColor : 0xFFD9822B;
        const ImVec2 min(selectedRect[0] - 1.0f, selectedRect[1] - 1.0f);
        const ImVec2 max(selectedRect[2] + 1.0f, selectedRect[3] + 1.0f);
        drawList->AddRect(min, max, color, 0.0f, 0, 1.5f);
        const float  handle = 4.0f;
        const ImVec2 corners[4] = { min, ImVec2(max.x, min.y), max, ImVec2(min.x, max.y) };
        for (const ImVec2& corner : corners)
            drawList->AddRectFilled(ImVec2(corner.x - handle, corner.y - handle),
                ImVec2(corner.x + handle, corner.y + handle), color);
    }
}
} // namespace bklv

#include "bklv/ui/shell/shell.hpp"

#include "bklv/foundation/anim/time.hpp"
#include "bklv/foundation/core/i18n.hpp"
#include "bklv/foundation/core/logger.hpp"
#include "bklv/ui/input/input.hpp"

namespace bklv
{

namespace
{

// 每个层都是绝对定位 + 四边 0 —— 铺满外壳，不参与彼此的 flex 排布。
Box* makeLayer()
{
    Box* layer = new Box(Axis::COLUMN);
    layer->setPositionType(PositionType::ABSOLUTE);
    layer->setPositionTop(0.0f);
    layer->setPositionRight(0.0f);
    layer->setPositionBottom(0.0f);
    layer->setPositionLeft(0.0f);
    return layer;
}

} // namespace

Shell::Shell()
    : Box(Axis::COLUMN)
{
    this->backdropLayer = makeLayer();
    this->contentLayer  = makeLayer();
    this->overlayLayer  = makeLayer();
    this->hudLayer      = makeLayer();
    this->debugLayer    = makeLayer();

    // 绘制顺序 = 子节点顺序：背景 → 内容 → 覆盖 → 浮层 → 调试层
    // （调试层最后：日志窗口必须在所有层之上）
    this->addView(this->backdropLayer);
    this->addView(this->contentLayer);
    this->addView(this->overlayLayer);
    this->addView(this->hudLayer);
    this->addView(this->debugLayer);

    // 焦点框挂在浮层上：永远画在内容/覆盖层之上（弹窗、页头也盖得住）。
    // 它自己每帧去问"当前焦点是谁、你是什么形状"，控件侧不需要任何登记。
    this->focusRingView = new FocusRingView();
    this->hudLayer->addView(this->focusRingView);

    // 内容层的层级栈：内容由外壳的 pushContentView/popContentView 管，
    // 宿主也可以绕过它直接往 getContentLayer() 里 addView（两者不冲突）。
    this->contentStack.setHostLayer(this->contentLayer);
}

Shell::~Shell()
{
    // 脱离树的那几层（在栈里、但没挂着的）归 ContentStack 释放；
    // 挂在树里的那个随 Box 的析构回收，所以先让栈清掉自己那份。
    this->contentStack.releaseDetached();

    // 五个层与焦点框、过渡层都是外壳 new 的，由 Box 的析构统一释放子视图，
    // 这里只把指向它们的裸指针清掉，避免析构顺序不同导致的悬垂（调试期看得见）。
    this->focusRingView = nullptr;
    this->transitionHost = nullptr;
    this->backdropLayer = nullptr;
    this->contentLayer  = nullptr;
    this->overlayLayer  = nullptr;
    this->hudLayer      = nullptr;
    this->debugLayer    = nullptr;
}

void Shell::setFocusRingEnabled(bool enabled)
{
    if (this->focusRingView != nullptr)
        this->focusRingView->setRingEnabled(enabled);
}

bool Shell::isFocusRingEnabled() const
{
    return this->focusRingView != nullptr && this->focusRingView->isRingEnabled();
}

void Shell::setFocusRingType(focus_ring::Type type)
{
    if (this->focusRingView != nullptr)
        this->focusRingView->setRingType(type);
}

focus_ring::Type Shell::getFocusRingType() const
{
    return this->focusRingView != nullptr ? this->focusRingView->getRingType()
                                          : focus_ring::Type::NONE;
}

focus_ring::Style& Shell::getFocusRingStyle()
{
    if (this->focusRingView == nullptr)
    {
        // 理论上不会发生（构造时就建好了）；万一挂空，返回一张静态默认值
        static focus_ring::Style fallback;
        return fallback;
    }
    return this->focusRingView->getRingStyle();
}

const focus_ring::Style& Shell::getFocusRingStyle() const
{
    if (this->focusRingView == nullptr)
    {
        static const focus_ring::Style fallback;
        return fallback;
    }
    return this->focusRingView->getRingStyle();
}

void Shell::setSafeAreaInsets(float top, float right, float bottom, float left)
{
    this->safeTop    = top;
    this->safeRight  = right;
    this->safeBottom = bottom;
    this->safeLeft   = left;
    this->applySafeAreaToLayers();
}

void Shell::applySafeAreaToLayers()
{
    // 五层统一内缩：用 padding 而不是 position，层本身仍然四边 0 铺满外壳
    // （否则会改掉"层 = 视口"的坐标基准，子视图的百分比尺寸会跟着错位）。
    Box* layers[] = { this->backdropLayer, this->contentLayer, this->overlayLayer,
        this->hudLayer, this->debugLayer };
    for (Box* layer : layers)
    {
        if (layer == nullptr)
            continue;
        layer->setPaddingTop(this->safeTop);
        layer->setPaddingRight(this->safeRight);
        layer->setPaddingBottom(this->safeBottom);
        layer->setPaddingLeft(this->safeLeft);
    }
}

Box* Shell::getTransitionHost()
{
    if (this->transitionHost == nullptr)
    {
        this->transitionHost = makeLayer();
        // 加在最后 = 画在所有层之上（含浮层与调试层）
        this->addView(this->transitionHost);
    }
    return this->transitionHost;
}

void Shell::clearTransitionHost()
{
    if (this->transitionHost == nullptr)
        return;
    this->removeView(this->transitionHost, true);
    this->transitionHost = nullptr;
}

void Shell::releaseTransitionHostIfEmpty()
{
    if (this->transitionHost == nullptr)
        return;
    if (this->transitionHost->getChildrenCount() != 0)
        return;
    this->clearTransitionHost();
}

void Shell::fillLayer(View* view)
{
    if (view == nullptr)
        return;
    view->setPositionType(PositionType::ABSOLUTE);
    view->setPositionTop(0.0f);
    view->setPositionRight(0.0f);
    view->setPositionBottom(0.0f);
    view->setPositionLeft(0.0f);
}

void Shell::update()
{
    Box::update();

    // 内容层转场按帧推进（出场播完的回调里会挂新层、起入场）
    this->contentStack.step(getFrameDeltaSeconds());

    this->releaseTransitionHostIfEmpty();
}

// ---- 液态玻璃 ----

void Shell::setGlassEnabled(bool enabled)
{
    glass::GlassService::getInstance().setEnabled(enabled);
}

bool Shell::isGlassEnabled() const
{
    return glass::GlassService::getInstance().isEnabled();
}

void Shell::setGlassQuality(glass::GlassQuality quality)
{
    glass::GlassService::getInstance().setQuality(quality);
}

glass::GlassQuality Shell::getGlassQuality() const
{
    return glass::GlassService::getInstance().getQuality();
}

glass::GlassTuning& Shell::getGlassTuning()
{
    return glass::GlassService::getInstance().getTuning();
}

const glass::GlassTuning& Shell::getGlassTuning() const
{
    return glass::GlassService::getInstance().getTuning();
}

glass::EffectCapabilities Shell::getGlassCapabilities() const
{
    return glass::GlassService::getInstance().getCapabilities();
}

bool Shell::isGlassSupported() const
{
    return glass::GlassService::getInstance().isSupported();
}

// ---- 内容层：层级导航 ----

void Shell::pushContentView(View* view, const std::string& memoryKey)
{
    this->contentStack.push(view, memoryKey);
}

void Shell::replaceContentView(View* view, const std::string& memoryKey)
{
    this->contentStack.replace(view, memoryKey);
}

void Shell::popContentView(std::function<void()> onFinished)
{
    this->contentStack.pop(std::move(onFinished));
}

// ---- B 键路由 ----

void Shell::setOnBackRequested(std::function<bool()> handler)
{
    this->onBackRequested = std::move(handler);

    if (this->onBackRequested != nullptr && !this->backActionRegistered)
    {
        // 只在有 handler 时注册：动作一旦注册，底栏就会显示「⊖ 返回」
        this->backActionId = this->registerAction(ControllerButton::BACK, translate("hints/back"),
            [this](View*) { this->requestBack(); });
        this->backActionRegistered = true;
    }
    else if (this->onBackRequested == nullptr && this->backActionRegistered)
    {
        this->unregisterAction(this->backActionId);
        this->backActionRegistered = false;
    }
}

bool Shell::requestBack()
{
    if (this->onBackRequested == nullptr)
        return false;
    return this->onBackRequested();
}

} // namespace bklv

#include "bklv/widgets/shell.hpp"

#include "bklv/core/appearance.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/time.hpp"
#include "bklv/views/padding.hpp"
#include "bklv/views/fps_overlay.hpp"

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

// 居中类角落的包装盒已经挪到 widgets/placement.cpp（和摆放规则放一起，只有一份）。

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

    // 按页背景挂背景层；登记与交叉淡入的逻辑在 BackdropManager 里
    this->backdropManager.setHostLayer(this->backdropLayer);

    // 内容栈挂内容层；"交焦点"这一步由外壳注入（ContentStack 不认识 InputManager）
    this->contentStack.setHostLayer(this->contentLayer);
    this->contentStack.setFocusSink([](View* view) { getInputManager().giveFocus(view); });
}

void Shell::setGlassEnabled(bool enabled)
{
    glass::getGlassService().setEnabled(enabled);
}

bool Shell::isGlassEnabled() const
{
    return glass::getGlassService().isEnabled();
}

void Shell::setGlassQuality(glass::GlassQuality quality)
{
    glass::getGlassService().setQuality(quality);
}

glass::GlassQuality Shell::getGlassQuality() const
{
    return glass::getGlassService().getQuality();
}

glass::GlassTuning& Shell::getGlassTuning()
{
    return glass::getGlassService().getTuning();
}

const glass::GlassTuning& Shell::getGlassTuning() const
{
    return glass::getGlassService().getTuning();
}

glass::EffectCapabilities Shell::getGlassCapabilities() const
{
    return glass::getGlassService().getCapabilities();
}

bool Shell::isGlassSupported() const
{
    return glass::getGlassService().isSupported();
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
    return this->focusRingView != nullptr ? this->focusRingView->getRingType() : focus_ring::Type::NONE;
}

focus_ring::Style& Shell::getFocusRingStyle()
{
    static focus_ring::Style fallback;
    return this->focusRingView != nullptr ? this->focusRingView->getRingStyle() : fallback;
}

const focus_ring::Style& Shell::getFocusRingStyle() const
{
    static const focus_ring::Style fallback;
    return this->focusRingView != nullptr ? this->focusRingView->getRingStyle() : fallback;
}

void Shell::setHeaderView(View* view)
{
    this->headerView = view;
    if (view != nullptr)
        this->overlayLayer->addView(view);
}

void Shell::setFooterView(View* view)
{
    this->footerView = view;
    if (view != nullptr)
        this->overlayLayer->addView(view);
}

void Shell::setHeaderVisible(bool visible)
{
    this->headerVisible = visible;
    if (this->headerView != nullptr)
        this->headerView->setVisibility(visible ? Visibility::VISIBLE : Visibility::GONE);
}

void Shell::setFooterVisible(bool visible)
{
    this->footerVisible = visible;
    if (this->footerView != nullptr)
        this->footerView->setVisibility(visible ? Visibility::VISIBLE : Visibility::GONE);
}

// insets 由上层显式给出（页头折叠动画期间它应该是**目标值**，这样内容只重排一次）
void Shell::setHeaderInset(float inset)
{
    this->headerInset = inset;
}

void Shell::setFooterInset(float inset)
{
    this->footerInset = inset;
}

void Shell::setSafeAreaInsets(float top, float right, float bottom, float left)
{
    this->safeTop    = top;
    this->safeRight  = right;
    this->safeBottom = bottom;
    this->safeLeft   = left;
    this->applySafeAreaToLayers();
}

// 各层统一按安全区内缩；内容层与覆盖层参与布局，背景层/浮层/调试层跟随视口
void Shell::applySafeAreaToLayers()
{
    Box* layers[] = { this->backdropLayer, this->contentLayer, this->overlayLayer, this->hudLayer,
        this->debugLayer };
    for (Box* layer : layers)
    {
        if (layer == nullptr)
            continue;
        layer->setPositionTop(this->safeTop);
        layer->setPositionRight(this->safeRight);
        layer->setPositionBottom(this->safeBottom);
        layer->setPositionLeft(this->safeLeft);
    }
}

// 角落 / 贴边 / 铺满：实现在 widgets/placement.cpp（自由函数，任何容器都能用），
// 这里只把外壳自己的覆盖层传进去。
void Shell::addToCorner(View* view, Corner corner, float insetX, float insetY)
{
    placeInCorner(this->overlayLayer, view, corner, insetX, insetY);
}

void Shell::addToEdge(View* view, Edge edge, float inset)
{
    placeOnEdge(this->overlayLayer, view, edge, inset);
}

void Shell::fillLayer(View* view)
{
    fillLayerArea(this->overlayLayer, view);
}

// ---- 内容层的层级下钻（同级换页是 Pager 的事）----

Shell::~Shell()
{
    // 内容栈里"脱离树"的那几层归 Shell 所有（挂在树里的那个会随 Box 析构回收）
    this->contentStack.releaseDetached();
}

// 转场与栈操作全部转发给 ContentStack（widgets/content_stack.*）：
// 入场/出场两段、出场播完才摘/删、焦点交接这一串顺序敏感的逻辑都在那边，只有一份。

void Shell::playEnterTransition(View* view)
{
    this->contentStack.playEnter(view);
}

void Shell::playExitTransition(View* view, std::function<void()> onFinished)
{
    this->contentStack.playExit(view, std::move(onFinished));
}

void Shell::stepContentTransition(float deltaSeconds)
{
    this->contentStack.step(deltaSeconds);
}

// 往下钻一层 / 退回上一层：全部转发给 ContentStack（widgets/content_stack.*）。
// 那串"旧层先出场 → 出场播完才摘它并挂新层 → 交焦点 → 新层入场 → 弹层释放与回调"
// 的顺序逻辑在那边，只有一份。

void Shell::pushContentView(View* view)
{
    this->contentStack.push(view);
}

void Shell::popContentView(std::function<void()> callback)
{
    this->contentStack.pop(std::move(callback));
}




// 弹窗：挂覆盖层顶部 + 切模态（模态栈会保存当前焦点，关闭时自动恢复；
// 指针命中与方向导航也被限制在弹窗子树内）。
void Shell::pushDialog(View* dialog, const DialogOptions& options)
{
    if (dialog == nullptr)
        return;

    // 遮罩先进树：子节点顺序 = 绘制顺序，所以它画在弹窗**之下**、内容层之上
    if (options.dim)
    {
        Box* dim = new Box(Axis::COLUMN);
        dim->setPositionType(PositionType::ABSOLUTE);
        dim->setPositionTop(0.0f);
        dim->setPositionRight(0.0f);
        dim->setPositionBottom(0.0f);
        dim->setPositionLeft(0.0f);
        dim->setBackgroundColor((ImU32)options.dimColor);
        this->overlayLayer->addView(dim);
        this->dimStack.push_back(dim);
    }
    else
    {
        this->dimStack.push_back(nullptr);
    }

    this->overlayLayer->addView(dialog);
    this->dialogStack.push_back(dialog);
    this->dialogOptionStack.push_back(options);

    // B 键的读取点：模态子树里没人处理 B 时由外壳决定关不关（closeOnBack）。
    // 用 ModalEntry 上的兜底而不是全局槽位 —— 生命周期随模态栈结束，
    // 不会留下指向已销毁 Shell 的回调；先拷贝再调（回调里会 popModal）。
    getInputManager().pushModal(dialog, options.closeOnBack,
        [this](ControllerButton button) { return this->handleModalUnhandled(button); });

    // 点弹窗之外的区域关闭（closeOnDimClick）。
    // F1(b)：不动 hitTest/focusFromPointer 的焦点语义，只登记"模态内空白点击"回调，
    // 由这里判点是否落在弹窗矩形内。弹窗自己的空白区域不算"外面"，不关。
    if (options.closeOnDimClick)
    {
        getInputManager().setModalEmptyClickHandler([this, dialog](const Point& position) {
            if (this->dialogStack.empty() || this->dialogStack.back() != dialog)
                return;
            if (!dialog->getFrame().contains(position))
                this->popDialog(dialog);
        });
    }
}

bool Shell::handleModalUnhandled(ControllerButton button)
{
    if (button != ControllerButton::B || this->dialogStack.empty())
        return false;

    if (!this->dialogOptionStack.empty() && !this->dialogOptionStack.back().closeOnBack)
        return false;

    this->popDialog(this->dialogStack.back());
    return true;
}

void Shell::setOnBackRequested(std::function<bool()> handler)
{
    this->onBackRequested = std::move(handler);

    // 非模态时的 B 路由：注册一个隐藏的 B 动作挂在外壳自己身上，
    // 焦点在内容树里按 B 会沿父链冒泡到外壳（页头/底栏不在焦点链里，不受影响）。
    if (!this->backActionRegistered)
    {
        this->backActionRegistered = true;
        this->registerAction(ControllerButton::B, "", [this](View*) {
            if (this->onBackRequested)
                this->onBackRequested();
        }, true);
    }
}

void Shell::popDialog(View* dialog, bool free)
{
    if (this->dialogStack.empty())
        return;

    // 只允许按栈序弹出（弹窗套弹窗时，先关最上面那个）
    if (dialog != nullptr && this->dialogStack.back() != dialog)
        return;

    View* top = this->dialogStack.back();
    this->dialogStack.pop_back();
    if (!this->dialogOptionStack.empty())
        this->dialogOptionStack.pop_back();

    // 顺序要紧（F3）：先在**仍在树里**时 popModal —— 模态栈用它保存的原焦点恢复，
    // 而它只校验 savedFocus->getParent()；若先摘视图，恢复出来的焦点会落到已脱离树的面板上。
    getInputManager().popModal();

    // 再从树上摘掉：不摘的话弹窗会永久留在覆盖层里（既泄漏又继续被绘制）。
    // free 的默认值与 App::pushOverlay/popOverlay 一致：挂上去之后所有权归外壳。
    if (top != nullptr && top->getParent() == this->overlayLayer)
        this->overlayLayer->removeView(top, free);

    // 遮罩与弹窗一一对应，跟着一起摘；它是外壳 new 的，所以永远由外壳释放
    if (!this->dimStack.empty())
    {
        View* dim = this->dimStack.back();
        this->dimStack.pop_back();
        if (dim != nullptr && dim->getParent() == this->overlayLayer)
            this->overlayLayer->removeView(dim, true);
    }
}

// ---- 按页背景（切页淡入）----
//
// 全部转发给 BackdropManager（widgets/backdrop_manager.*）。外壳只负责"背景挂在背景层"，
// 登记、可见性、交叉淡入的补间都在那边 —— 那部分逻辑与外壳无关，拆出去才读得动。

void Shell::setBackdropFor(const std::string& pageId, View* backdrop)
{
    this->backdropManager.add(pageId, backdrop);
}

View* Shell::getBackdropFor(const std::string& pageId) const
{
    return this->backdropManager.get(pageId);
}

void Shell::showBackdropFor(const std::string& pageId)
{
    this->backdropManager.show(pageId);
}

void Shell::stepBackdropTransition(float deltaSeconds)
{
    this->backdropManager.step(deltaSeconds);
}

// ---- 过渡层（仅动画期间存在）----

Box* Shell::getTransitionHost()
{
    if (this->transitionHost != nullptr)
        return this->transitionHost;

    // 铺满、绝对定位、不吃输入（Box 默认不可聚焦），挂在**最后一个**子节点位置 = 最后绘制
    Box* host = new Box(Axis::COLUMN);
    host->setVisibility(Visibility::GONE); // 默认不可见；有飞行元素时由调用方设 VISIBLE
    this->addView(host);
    this->transitionHost = host;
    return host;
}

void Shell::clearTransitionHost()
{
    if (this->transitionHost == nullptr)
        return;

    this->transitionHost->removeAllViews(); // 只摘不删：飞行元素的所有权在调用方
    this->releaseTransitionHostIfEmpty();
}

void Shell::releaseTransitionHostIfEmpty()
{
    if (this->transitionHost == nullptr)
        return;

    if (!this->transitionHost->getChildren().empty())
        return;

    if (this->transitionHost->getParent() == this)
        this->removeView(this->transitionHost, true);
    else
        delete this->transitionHost;

    this->transitionHost = nullptr;
}

void Shell::applyTheme()
{
    // 尺寸（铺满）与页面底色来自 themes/<主题>/widgets/applet_frame.json ——
    // 这份外观原来是旧 AppletFrame 的，现在归外壳用（名字保持不动，用户主题不用改）。
    applyWidgetBox(this, "widget/applet_frame");
}

void Shell::update()
{
    Box::update();

    this->stepBackdropTransition(getFrameDeltaSeconds());
    this->stepContentTransition(getFrameDeltaSeconds());
    this->releaseTransitionHostIfEmpty();
}

void Shell::setFpsOverlayEnabled(bool enabled)
{
    if (enabled == (this->fpsOverlay != nullptr))
        return;

    if (!enabled)
    {
        if (this->fpsOverlay != nullptr && this->debugLayer != nullptr)
            this->debugLayer->removeView(this->fpsOverlay);
        this->fpsOverlay = nullptr;
        return;
    }

    if (this->debugLayer == nullptr)
        return;

    this->fpsOverlay = new FpsOverlay();
    this->debugLayer->addView(this->fpsOverlay);
}

} // namespace bklv

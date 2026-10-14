#include "bklv/widgets/shell.hpp"

#include "bklv/core/appearance.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/time.hpp"
#include "bklv/views/padding.hpp"

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

// 居中类的角落：内容要"在这条带上居中"，用一层铺满的包装盒 + 对齐实现；
// 真角落（四个角）直接给两条边偏移，不需要包装盒。
Box* makeCenteringWrapper(Axis axis, JustifyContent justify, AlignItems align)
{
    Box* wrapper = makeLayer();
    wrapper->setAxis(axis);
    wrapper->setJustifyContent(justify);
    wrapper->setAlignItems(align);
    return wrapper;
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

void Shell::addToCorner(View* view, Corner corner, float insetX, float insetY)
{
    if (view == nullptr)
        return;

    switch (corner)
    {
        case Corner::TOP_LEFT:
            this->overlayLayer->addView(view);
            view->setPositionType(PositionType::ABSOLUTE);
            view->setPositionTop(insetY);
            view->setPositionLeft(insetX);
            break;
        case Corner::TOP_RIGHT:
            this->overlayLayer->addView(view);
            view->setPositionType(PositionType::ABSOLUTE);
            view->setPositionTop(insetY);
            view->setPositionRight(insetX);
            break;
        case Corner::BOTTOM_LEFT:
            this->overlayLayer->addView(view);
            view->setPositionType(PositionType::ABSOLUTE);
            view->setPositionBottom(insetY);
            view->setPositionLeft(insetX);
            break;
        case Corner::BOTTOM_RIGHT:
            this->overlayLayer->addView(view);
            view->setPositionType(PositionType::ABSOLUTE);
            view->setPositionBottom(insetY);
            view->setPositionRight(insetX);
            break;
        case Corner::TOP_CENTER:
        {
            Box* wrapper = makeCenteringWrapper(Axis::ROW, JustifyContent::CENTER, AlignItems::FLEX_START);
            wrapper->setPositionTop(insetY);
            wrapper->setPaddingTop(0.0f);
            this->overlayLayer->addView(wrapper);
            wrapper->addView(view);
            break;
        }
        case Corner::BOTTOM_CENTER:
        {
            Box* wrapper = makeCenteringWrapper(Axis::ROW, JustifyContent::CENTER, AlignItems::FLEX_END);
            wrapper->setPositionBottom(insetY);
            this->overlayLayer->addView(wrapper);
            wrapper->addView(view);
            break;
        }
        case Corner::CENTER_LEFT:
        {
            Box* wrapper = makeCenteringWrapper(Axis::COLUMN, JustifyContent::CENTER, AlignItems::FLEX_START);
            wrapper->setPositionLeft(insetX);
            this->overlayLayer->addView(wrapper);
            wrapper->addView(view);
            break;
        }
        case Corner::CENTER_RIGHT:
        {
            Box* wrapper = makeCenteringWrapper(Axis::COLUMN, JustifyContent::CENTER, AlignItems::FLEX_END);
            wrapper->setPositionRight(insetX);
            this->overlayLayer->addView(wrapper);
            wrapper->addView(view);
            break;
        }
        case Corner::CENTER:
        {
            Box* wrapper = makeCenteringWrapper(Axis::COLUMN, JustifyContent::CENTER, AlignItems::CENTER);
            this->overlayLayer->addView(wrapper);
            wrapper->addView(view);
            break;
        }
    }
}

void Shell::addToEdge(View* view, Edge edge, float inset)
{
    if (view == nullptr)
        return;

    this->overlayLayer->addView(view);
    view->setPositionType(PositionType::ABSOLUTE);
    switch (edge)
    {
        case Edge::TOP:
            view->setPositionTop(inset);
            view->setPositionLeft(0.0f);
            view->setPositionRight(0.0f);
            break;
        case Edge::RIGHT:
            view->setPositionRight(inset);
            view->setPositionTop(0.0f);
            view->setPositionBottom(0.0f);
            break;
        case Edge::BOTTOM:
            view->setPositionBottom(inset);
            view->setPositionLeft(0.0f);
            view->setPositionRight(0.0f);
            break;
        case Edge::LEFT:
            view->setPositionLeft(inset);
            view->setPositionTop(0.0f);
            view->setPositionBottom(0.0f);
            break;
    }
}

// 铺满指定层（先加进该层再四边归零）：全屏遮罩、loading
void Shell::fillLayer(View* view)
{
    if (view == nullptr)
        return;

    this->overlayLayer->addView(view);
    view->setPositionType(PositionType::ABSOLUTE);
    view->setPositionTop(0.0f);
    view->setPositionRight(0.0f);
    view->setPositionBottom(0.0f);
    view->setPositionLeft(0.0f);
}

// ---- 内容层的层级下钻（同级换页是 Pager 的事）----

Shell::~Shell()
{
    // 内容栈里"脱离树"的那几层归 Shell 所有（挂在树里的那个会随 Box 析构回收）
    for (View* view : this->contentStack)
        if (view != nullptr && view != this->contentView && view->getParent() == nullptr)
            delete view;
}

void Shell::playEnterTransition(View* view)
{
    const float width = this->contentLayer != nullptr ? this->contentLayer->getWidth() : 0.0f;
    this->transition.playEnter(view, width);
}

void Shell::playExitTransition(View* view, std::function<void()> onFinished)
{
    const float width = this->contentLayer != nullptr ? this->contentLayer->getWidth() : 0.0f;
    this->transition.playExit(view, width, std::move(onFinished));
}

void Shell::stepContentTransition(float deltaSeconds)
{
    this->transition.step(deltaSeconds);
}

// 把 view 挂到内容层里，并把上一任从树上摘下来（**不删**：它还在栈里）
void Shell::attachContentView(View* view)
{
    if (view == nullptr)
        return;

    if (this->contentView != nullptr && this->contentView->getParent() == this->contentLayer)
        this->contentLayer->removeView(this->contentView, false);

    this->contentView = view;
    this->contentLayer->addView(view);
}

View* Shell::detachContentView()
{
    View* leaving = this->contentView;
    if (leaving != nullptr && leaving->getParent() == this->contentLayer)
        this->contentLayer->removeView(leaving, false);

    this->contentView = nullptr;
    return leaving;
}

void Shell::pushContentView(View* view)
{
    if (view == nullptr)
        return;

    // 上一段转场还没播完：先落位（否则栈与画面会对不上）
    this->transition.finishNow();

    View* outgoing = this->contentView;
    this->contentStack.push_back(view);

    // 首屏：没有旧层可让位，直接挂上（也不做入场：刚起界面就滑一下很怪）
    if (outgoing == nullptr)
    {
        this->attachContentView(view);
        return;
    }

    // **旧层先出场**（它还挂在树上，所以看得到），出场播完再摘它、挂新层、新层入场。
    // 旧层不删：下钻时它留在栈里，返回时还要用。
    this->playExitTransition(outgoing, [this, view] {
        this->detachContentView();

        this->attachContentView(view);

        // 焦点落到新层里的第一个可聚焦项
        if (View* inner = this->contentLayer->getDefaultFocus())
            getInputManager().giveFocus(inner);

        this->playEnterTransition(view);
    });
}

void Shell::popContentView(std::function<void()> callback)
{
    if (this->contentStack.size() <= 1)
    {
        if (callback)
            callback();
        return;
    }

    this->transition.finishNow();

    View* outgoing = this->contentStack.back();
    this->contentStack.pop_back();

    View* incoming = this->contentStack.back();
    this->pendingPopCallback = callback;

    // **被弹出的临时页先出场**，出场播完再释放它、挂回上一层、让上一层入场。
    // 注意顺序：先把它从内容层摘下来再 delete，不能让 contentView 指着已释放的对象。
    this->playExitTransition(outgoing, [this, incoming, outgoing] {
        View* leaving = outgoing;
        if (this->contentView == leaving)
            this->detachContentView();

        if (leaving->getParent() == this->contentLayer)
            this->contentLayer->removeView(leaving, true);
        else
            delete leaving;

        this->attachContentView(incoming);

        if (View* inner = this->contentLayer->getDefaultFocus())
            getInputManager().giveFocus(inner);

        this->playEnterTransition(incoming);

        // pop 的回调等**出场**播完才跑（此时被弹出的页已经可以释放了）
        std::function<void()> done = this->pendingPopCallback;
        this->pendingPopCallback   = nullptr;
        if (done)
            done();
    });
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

void Shell::setBackdropFor(const std::string& pageId, View* backdrop)
{
    if (backdrop == nullptr)
        return;

    this->backdrops[pageId] = backdrop;
    this->backdropLayer->addView(backdrop);
    backdrop->setVisibility(Visibility::GONE);

    // 第一份背景直接当当前背景（否则"切页前"什么都没有）
    if (this->currentBackdrop == nullptr)
    {
        this->currentBackdrop   = backdrop;
        this->currentBackdropId = pageId;
        backdrop->setVisibility(Visibility::VISIBLE);
        backdrop->setAlpha(1.0f);
    }
}

View* Shell::getBackdropFor(const std::string& pageId) const
{
    auto it = this->backdrops.find(pageId);
    return it == this->backdrops.end() ? nullptr : it->second;
}

void Shell::showBackdropFor(const std::string& pageId)
{
    if (pageId == this->currentBackdropId)
        return;

    auto it = this->backdrops.find(pageId);
    if (it == this->backdrops.end() || it->second == nullptr)
    {
        BKLV_LOG_WARNING("Shell::showBackdropFor: 页面 %s 没有注册背景", pageId.c_str());
        return;
    }

    View* next = it->second;

    // 新的移到背景层末尾：子节点顺序 = 绘制顺序，淡入的那份要画在旧背景之上
    if (next->getParent() == this->backdropLayer)
        this->backdropLayer->removeView(next, false);
    this->backdropLayer->addView(next);

    // 旧的先留着垫在下面，补间结束再 GONE（否则淡入期间会露出画布底色）
    this->previousBackdrop = this->currentBackdrop;
    if (this->previousBackdrop != nullptr)
        this->previousBackdrop->setAlpha(1.0f);

    next->setVisibility(Visibility::VISIBLE);
    next->setAlpha(0.0f);

    this->currentBackdrop   = next;
    this->currentBackdropId = pageId;

    if (this->backdropDuration <= 0.0f)
    {
        next->setAlpha(1.0f);
        if (this->previousBackdrop != nullptr)
            this->previousBackdrop->setVisibility(Visibility::GONE);
        this->previousBackdrop = nullptr;
        return;
    }

    this->backdropAnim.stop();
    this->backdropAnim.reset(0.0f);
    this->backdropAnim.addStep(1.0f, this->backdropDuration, EasingFunction::CUBIC_OUT);
    this->backdropAnim.start();
}

void Shell::stepBackdropTransition(float deltaSeconds)
{
    if (this->backdropAnim.isStopped())
        return;

    this->backdropAnim.update(deltaSeconds);

    const float alpha = (float)this->backdropAnim;
    if (this->currentBackdrop != nullptr)
        this->currentBackdrop->setAlpha(alpha);

    if (alpha >= 1.0f)
    {
        if (this->previousBackdrop != nullptr)
        {
            this->previousBackdrop->setVisibility(Visibility::GONE);
            this->previousBackdrop = nullptr;
        }
    }
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
    this->transition.step(getFrameDeltaSeconds());
    this->releaseTransitionHostIfEmpty();
}

} // namespace bklv

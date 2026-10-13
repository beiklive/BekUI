#include "bklv/widgets/shell.hpp"

#include "bklv/core/geometry.hpp"
#include "bklv/core/input.hpp"
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

    // 绘制顺序 = 子节点顺序：背景 → 内容 → 覆盖 → 浮层
    this->addView(this->backdropLayer);
    this->addView(this->contentLayer);
    this->addView(this->overlayLayer);
    this->addView(this->hudLayer);
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

// 四层统一按安全区内缩；内容层与覆盖层参与布局，背景层与浮层跟随视口
void Shell::applySafeAreaToLayers()
{
    Box* layers[] = { this->backdropLayer, this->contentLayer, this->overlayLayer, this->hudLayer };
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

} // namespace bklv

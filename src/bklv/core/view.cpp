#include "bklv/core/view.hpp"

#include <cmath>
#include <cstdint>

#include "bklv/core/app.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/font.hpp"
#include "bklv/core/logger.hpp"

namespace bklv
{

// ---------------------------------------------------------------------------
// Yoga 全局配置与事件桥接
// ---------------------------------------------------------------------------

static YGConfigRef getYogaConfig()
{
    static YGConfigRef config = nullptr;
    if (config == nullptr)
    {
        config = YGConfigNew();
        // 与 borealis 一致：启用 Web 默认值（flex-direction 默认 row、flex-shrink 默认 1）。
        YGConfigSetUseWebDefaults(config, true);
    }
    return config;
}

// 测量回调：Yoga 用它询问"这个叶子节点想占多大"。
// 注意这版 Yoga 的 measure 签名第一个参数是 const 指针。
static YGSize onYogaMeasure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height,
    YGMeasureMode heightMode)
{
    // context 里存的是非常量 View*，这里只是读回来用。
    View* view = static_cast<View*>(YGNodeGetContext(const_cast<YGNodeRef>(node)));
    if (view == nullptr)
        return YGSize { width, height };
    return view->measure(width, widthMode, height, heightMode);
}

// ---------------------------------------------------------------------------
// 构造与析构
// ---------------------------------------------------------------------------

View::View()
{
    // 核心一步：C++ 对象与 Yoga 节点互相绑定。
    this->ygNode = YGNodeNewWithConfig(getYogaConfig());
    YGNodeSetContext(this->ygNode, this);

    // 注意：默认不给节点挂测量回调。
    // Yoga 会把"有测量函数的节点"当作叶子节点，如果容器也挂了，子节点就不会被布局。
    // 需要按内容算尺寸的子类请在构造函数里调用 setMeasureFunction()。

    YGNodeStyleSetWidthAuto(this->ygNode);
    YGNodeStyleSetHeightAuto(this->ygNode);
}

View::~View()
{
    // 正在被销毁的视图可能还在焦点路径上，先让输入层换一个焦点，避免悬空指针。
    getInputManager().notifyViewDeleted(this);

    if (this->ygNode == nullptr)
        return;

    // 避免 Yoga 在释放过程中回调到正在析构的对象。
    YGNodeSetContext(this->ygNode, nullptr);

    if (this->parent != nullptr)
        this->parent->removeView(this, false);

    YGNodeFreeRecursive(this->ygNode);
    this->ygNode = nullptr;
}

// ---------------------------------------------------------------------------
// 布局：尺寸
// ---------------------------------------------------------------------------

void View::setWidth(float width)
{
    if (std::isnan(width))
        YGNodeStyleSetWidthAuto(this->ygNode);
    else
        YGNodeStyleSetWidth(this->ygNode, width);
    this->invalidate();
}

void View::setHeight(float height)
{
    if (std::isnan(height))
        YGNodeStyleSetHeightAuto(this->ygNode);
    else
        YGNodeStyleSetHeight(this->ygNode, height);
    this->invalidate();
}

void View::setDimensions(float width, float height)
{
    this->setWidth(width);
    this->setHeight(height);
}

void View::setSize(Size size)
{
    this->setDimensions(size.width, size.height);
}

void View::setWidthPercentage(float percentage)
{
    YGNodeStyleSetWidthPercent(this->ygNode, percentage);
    this->invalidate();
}

void View::setHeightPercentage(float percentage)
{
    YGNodeStyleSetHeightPercent(this->ygNode, percentage);
    this->invalidate();
}

void View::setMinWidth(float minWidth)
{
    YGNodeStyleSetMinWidth(this->ygNode, minWidth);
    this->invalidate();
}

void View::setMinHeight(float minHeight)
{
    YGNodeStyleSetMinHeight(this->ygNode, minHeight);
    this->invalidate();
}

void View::setMaxWidth(float maxWidth)
{
    YGNodeStyleSetMaxWidth(this->ygNode, maxWidth);
    this->invalidate();
}

void View::setMaxHeight(float maxHeight)
{
    YGNodeStyleSetMaxHeight(this->ygNode, maxHeight);
    this->invalidate();
}

void View::setMinWidthPercentage(float percentage)
{
    YGNodeStyleSetMinWidthPercent(this->ygNode, percentage);
    this->invalidate();
}

void View::setMinHeightPercentage(float percentage)
{
    YGNodeStyleSetMinHeightPercent(this->ygNode, percentage);
    this->invalidate();
}

void View::setMaxWidthPercentage(float percentage)
{
    YGNodeStyleSetMaxWidthPercent(this->ygNode, percentage);
    this->invalidate();
}

void View::setMaxHeightPercentage(float percentage)
{
    YGNodeStyleSetMaxHeightPercent(this->ygNode, percentage);
    this->invalidate();
}

// ---------------------------------------------------------------------------
// 布局：弹性
// ---------------------------------------------------------------------------

void View::setGrow(float grow)
{
    YGNodeStyleSetFlexGrow(this->ygNode, grow);
    this->invalidate();
}

void View::setShrink(float shrink)
{
    YGNodeStyleSetFlexShrink(this->ygNode, shrink);
    this->invalidate();
}

float View::getGrow() const
{
    return YGNodeStyleGetFlexGrow(this->ygNode);
}

// ---------------------------------------------------------------------------
// 布局：外边距
// ---------------------------------------------------------------------------

void View::setMargins(float top, float right, float bottom, float left)
{
    YGNodeStyleSetMargin(this->ygNode, YGEdgeTop, top);
    YGNodeStyleSetMargin(this->ygNode, YGEdgeRight, right);
    YGNodeStyleSetMargin(this->ygNode, YGEdgeBottom, bottom);
    YGNodeStyleSetMargin(this->ygNode, YGEdgeLeft, left);
    this->invalidate();
}

void View::setMargin(float margin)
{
    this->setMargins(margin, margin, margin, margin);
}

void View::setMarginTop(float top)
{
    YGNodeStyleSetMargin(this->ygNode, YGEdgeTop, top);
    this->invalidate();
}

void View::setMarginRight(float right)
{
    YGNodeStyleSetMargin(this->ygNode, YGEdgeRight, right);
    this->invalidate();
}

void View::setMarginBottom(float bottom)
{
    YGNodeStyleSetMargin(this->ygNode, YGEdgeBottom, bottom);
    this->invalidate();
}

void View::setMarginLeft(float left)
{
    YGNodeStyleSetMargin(this->ygNode, YGEdgeLeft, left);
    this->invalidate();
}

static float toFloat(YGValue value)
{
    return value.unit == YGUnitUndefined ? 0.0f : value.value;
}

float View::getMarginTop() const
{
    return toFloat(YGNodeStyleGetMargin(this->ygNode, YGEdgeTop));
}

float View::getMarginRight() const
{
    return toFloat(YGNodeStyleGetMargin(this->ygNode, YGEdgeRight));
}

float View::getMarginBottom() const
{
    return toFloat(YGNodeStyleGetMargin(this->ygNode, YGEdgeBottom));
}

float View::getMarginLeft() const
{
    return toFloat(YGNodeStyleGetMargin(this->ygNode, YGEdgeLeft));
}

// ---------------------------------------------------------------------------
// 布局：定位
// ---------------------------------------------------------------------------

void View::setPositionTop(float pos)
{
    YGNodeStyleSetPosition(this->ygNode, YGEdgeTop, pos);
    this->invalidate();
}

void View::setPositionRight(float pos)
{
    YGNodeStyleSetPosition(this->ygNode, YGEdgeRight, pos);
    this->invalidate();
}

void View::setPositionBottom(float pos)
{
    YGNodeStyleSetPosition(this->ygNode, YGEdgeBottom, pos);
    this->invalidate();
}

void View::setPositionLeft(float pos)
{
    YGNodeStyleSetPosition(this->ygNode, YGEdgeLeft, pos);
    this->invalidate();
}

void View::setPositionTopPercentage(float percentage)
{
    YGNodeStyleSetPositionPercent(this->ygNode, YGEdgeTop, percentage);
    this->invalidate();
}

void View::setPositionRightPercentage(float percentage)
{
    YGNodeStyleSetPositionPercent(this->ygNode, YGEdgeRight, percentage);
    this->invalidate();
}

void View::setPositionBottomPercentage(float percentage)
{
    YGNodeStyleSetPositionPercent(this->ygNode, YGEdgeBottom, percentage);
    this->invalidate();
}

void View::setPositionLeftPercentage(float percentage)
{
    YGNodeStyleSetPositionPercent(this->ygNode, YGEdgeLeft, percentage);
    this->invalidate();
}

void View::setPositionType(PositionType type)
{
    YGNodeStyleSetPositionType(
        this->ygNode, type == PositionType::ABSOLUTE ? YGPositionTypeAbsolute : YGPositionTypeRelative);
    this->invalidate();
}

void View::setAlignSelf(AlignSelf align)
{
    switch (align)
    {
        case AlignSelf::AUTO:
            YGNodeStyleSetAlignSelf(this->ygNode, YGAlignAuto);
            break;
        case AlignSelf::FLEX_START:
            YGNodeStyleSetAlignSelf(this->ygNode, YGAlignFlexStart);
            break;
        case AlignSelf::CENTER:
            YGNodeStyleSetAlignSelf(this->ygNode, YGAlignCenter);
            break;
        case AlignSelf::FLEX_END:
            YGNodeStyleSetAlignSelf(this->ygNode, YGAlignFlexEnd);
            break;
        case AlignSelf::STRETCH:
            YGNodeStyleSetAlignSelf(this->ygNode, YGAlignStretch);
            break;
        case AlignSelf::BASELINE:
            YGNodeStyleSetAlignSelf(this->ygNode, YGAlignBaseline);
            break;
    }
    this->invalidate();
}

void View::setVisibility(Visibility visibility)
{
    this->visibility = visibility;

    switch (visibility)
    {
        case Visibility::VISIBLE:
            YGNodeStyleSetDisplay(this->ygNode, YGDisplayFlex);
            break;
        case Visibility::INVISIBLE:
            // 保留占位，但不绘制。
            YGNodeStyleSetDisplay(this->ygNode, YGDisplayFlex);
            break;
        case Visibility::GONE:
            YGNodeStyleSetDisplay(this->ygNode, YGDisplayNone);
            break;
    }

    this->invalidate();
}

// ---------------------------------------------------------------------------
// 布局结果读取
// ---------------------------------------------------------------------------

float View::getLocalX() const
{
    // Yoga 返回的是相对父节点的偏移，再叠加自身的 translation。
    return YGNodeLayoutGetLeft(this->ygNode) + this->translation.x;
}

float View::getLocalY() const
{
    return YGNodeLayoutGetTop(this->ygNode) + this->translation.y;
}

float View::getX() const
{
    // getLocalX() 里已经含了自身 translation，这里只负责累加父节点位置。
    if (this->hasParent())
        return this->parent->getX() + this->getLocalX();
    return this->getLocalX();
}

float View::getY() const
{
    if (this->hasParent())
        return this->parent->getY() + this->getLocalY();
    return this->getLocalY();
}

float View::getWidth() const
{
    return YGNodeLayoutGetWidth(this->ygNode);
}

float View::getHeight() const
{
    return YGNodeLayoutGetHeight(this->ygNode);
}

Rect View::getFrame() const
{
    return Rect(this->getX(), this->getY(), this->getWidth(), this->getHeight());
}

Rect View::getLocalFrame() const
{
    return Rect(this->getLocalX(), this->getLocalY(), this->getWidth(), this->getHeight());
}

// ---------------------------------------------------------------------------
// 外观
// ---------------------------------------------------------------------------

void View::setBackgroundColor(ImU32 color)
{
    this->backgroundColor = color;
}

void View::setBorderColor(ImU32 color)
{
    this->borderColor = color;
}

void View::setBorderThickness(float thickness)
{
    this->borderThickness = thickness;
}

void View::setCornerRadius(float radius)
{
    this->cornerRadius = radius;
}

void View::setAlpha(float alpha)
{
    this->alpha = alpha;
}

float View::getAlpha(bool child) const
{
    // 透明度沿父链相乘，等价于 borealis 的 View::getAlpha。
    if (child && this->hasParent())
        return this->alpha * this->parent->getAlpha(true);
    return this->alpha;
}

// ---------------------------------------------------------------------------
// 视图树
// ---------------------------------------------------------------------------

void View::setId(const std::string& value)
{
    this->id = value;
}

std::string View::getClassString() const
{
    return "bklv::View";
}

std::string View::describe() const
{
    return "<" + this->getClassString() + " id=\"" + this->id + "\">";
}

View* View::getView(const std::string& targetId)
{
    // 叶子节点的默认实现：只看自己，容器由 Box 重写后递归。
    if (this->id == targetId)
        return this;
    return nullptr;
}

// ---------------------------------------------------------------------------
// 失效与测量
// ---------------------------------------------------------------------------

void View::markMeasureDirty()
{
    if (YGNodeHasMeasureFunc(this->ygNode))
        YGNodeMarkDirty(this->ygNode);
}

void View::setMeasureFunction(MeasureFunction function)
{
    this->measureFunction = std::move(function);

    if (this->measureFunction)
        YGNodeSetMeasureFunc(this->ygNode, onYogaMeasure);
}

void View::notifyLayout()
{
    this->onLayout();
}

void View::refreshFontDependentLayout()
{
    // 默认字体是在 App::run() 里加载的，而视图树通常在 RunDemo 的参数里就已经建好。
    // 第一次构建时文本会测出 0 宽并被 Yoga 缓存，字体就位后必须整体重算一次。
    if (this->fontGeneration == getFontGeneration())
        return;

    this->fontGeneration = getFontGeneration();
    this->markMeasureDirty();

    if (this->hasParent() && !this->detached)
        this->parent->refreshFontDependentLayout();
    else
    {
        YGNodeCalculateLayout(this->ygNode, YGUndefined, YGUndefined, YGDirectionLTR);
        this->notifyLayout();
    }
}

void View::invalidate()
{
    this->markMeasureDirty();

    // 冒泡到根节点，由根节点做一次全树重算。
    if (this->hasParent() && !this->detached)
    {
        this->parent->invalidate();
        return;
    }

    // 根节点：用最近一次布局的画布尺寸重算。
    // 不要传 YGUndefined —— 那样百分比宽高会全部失去参照，
    // 整棵树会塌成"内容尺寸"（这里踩过一次：在 update 里新建 cell 触发的重排把根压成了 1196x182）。
    // 还没有过任何布局时先跳过，第一帧 App::frame 会算。
    if (App::hasLayoutSize())
    {
        YGNodeCalculateLayout(this->ygNode, App::getLayoutWidth(), App::getLayoutHeight(), YGDirectionLTR);
        // 布局更新后通知整棵子树，等价于 borealis 订阅的 Yoga NodeLayout 事件。
        this->notifyLayout();
    }
}

YGSize View::measure(float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode)
{
    if (this->measureFunction)
        return this->measureFunction(nullptr, width, widthMode, height, heightMode);

    const float measuredWidth  = widthMode == YGMeasureModeUndefined ? 0.0f : width;
    const float measuredHeight = heightMode == YGMeasureModeUndefined ? 0.0f : height;
    return YGSize { measuredWidth, measuredHeight };
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

void View::drawBackground(ImDrawList* drawList, float x, float y, float width, float height)
{
    if (this->backgroundColor == 0 || width <= 0.0f || height <= 0.0f)
        return;

    const ImU32 color = colorWithAlpha(this->backgroundColor, this->getAlpha());
    if (this->cornerRadius > 0.0f)
        drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), color, this->cornerRadius);
    else
        drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), color);
}

void View::drawBorder(ImDrawList* drawList, float x, float y, float width, float height)
{
    if (this->borderColor == 0 || this->borderThickness <= 0.0f || width <= 0.0f || height <= 0.0f)
        return;

    const ImU32 color = colorWithAlpha(this->borderColor, this->getAlpha());
    const float inset = this->borderThickness * 0.5f;
    drawList->AddRect(ImVec2(x + inset, y + inset), ImVec2(x + width - inset, y + height - inset), color,
        this->cornerRadius, 0, this->borderThickness);
}

void View::frame(FrameContext* ctx)
{
    if (this->visibility != Visibility::VISIBLE)
        return;

    const float x      = this->getX();
    const float y      = this->getY();
    const float width  = this->getWidth();
    const float height = this->getHeight();

    Style style = getDefaultStyle();

    this->drawBackground(ctx->drawList, x, y, width, height);

    if (this->borderThickness > 0.0f)
        this->drawBorder(ctx->drawList, x, y, width, height);

    // 裁剪：只有显式设置 clipsToBounds 的节点才限制子节点绘制范围。
    const bool clipped = this->clipsToBounds && width > 0.0f && height > 0.0f;
    if (clipped)
        ctx->drawList->PushClipRect(ImVec2(x, y), ImVec2(x + width, y + height), true);

    this->draw(ctx->drawList, x, y, width, height, style, ctx);
    this->drawChildren(ctx->drawList, x, y, width, height, style, ctx);

    if (clipped)
        ctx->drawList->PopClipRect();
}

// ---------------------------------------------------------------------------
// 位移
// ---------------------------------------------------------------------------

void View::setTranslation(float x, float y)
{
    this->translation.x = x;
    this->translation.y = y;
}

// ---------------------------------------------------------------------------
// 焦点与动作
// ---------------------------------------------------------------------------

View* View::getNextFocus(FocusDirection direction, View* currentView)
{
    // 叶子节点的默认行为：往上交给父容器。
    if (this->hasParent())
        return this->parent->getNextFocus(direction, currentView);
    return nullptr;
}

View* View::hitTest(Point point)
{
    // 约定：point 是传入者坐标系里的点。
    // 记录自己左上角的位置，先把它变成自己的局部坐标再判断是否落在矩形内。
    const float originX = this->getX();
    const float originY = this->getY();

    const Point local(point.x - originX, point.y - originY);
    const Rect bounds(0.0f, 0.0f, this->getWidth(), this->getHeight());

    if (!bounds.contains(local))
        return nullptr;

    if (!this->isFocusable())
        return nullptr;

    // 返回时把点还原成屏幕坐标，调用方拿到的是同一个坐标系。
    return this;
}

ActionIdentifier View::registerAction(ControllerButton button, const std::string& hintText,
    const ActionListener& listener, bool hidden)
{
    InputAction action;
    action.identifier = this->nextActionIdentifier++;
    action.button     = button;
    action.hintText   = hintText;
    action.listener   = listener;
    action.hidden     = hidden;

    this->actions.push_back(action);
    return action.identifier;
}

void View::unregisterAction(ActionIdentifier identifier)
{
    for (auto it = this->actions.begin(); it != this->actions.end(); ++it)
    {
        if (it->identifier == identifier)
        {
            this->actions.erase(it);
            return;
        }
    }
}

bool View::triggerAction(ControllerButton button, bool pointer)
{
    for (const InputAction& action : this->actions)
    {
        if (action.button != button || !action.listener)
            continue;

        // 鼠标点击默认只"选中"，不执行动作 —— 否则点一下列表项就会误触发。
        // 控件想支持点一下就执行，把 onPointerAction 覆写成 true 即可。
        if (pointer && !this->onPointerAction)
            return false;

        action.listener(this);
        return true;
    }
    return false;
}

} // namespace bklv

#include "bklv/core/box.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/core/logger.hpp"

namespace bklv
{

// Axis 到 Yoga flex-direction 的映射。
static YGFlexDirection getYGFlexDirection(Axis axis)
{
    return axis == Axis::ROW ? YGFlexDirectionRow : YGFlexDirectionColumn;
}

Box::Box(Axis axis)
    : axis(axis)
{
    YGNodeStyleSetFlexDirection(this->ygNode, getYGFlexDirection(axis));
}

Box::~Box()
{
    // 关键顺序：先让子节点从 Yoga 树上摘掉并释放，再让基类释放自己的节点。
    // 否则儿子析构时会去访问已经消失的父节点。
    this->clearViews();
}

// ---------------------------------------------------------------------------
// 视图树管理
//
// 这里的所有操作都必须同时更新 children 和 Yoga 节点树，
// 否则布局结果会和 C++ 对象树不一致。
// ---------------------------------------------------------------------------

void Box::addView(View* view)
{
    this->addView(view, this->children.size());
}

void Box::addView(View* view, size_t position)
{
    if (view == nullptr)
    {
        Logger::error("Box::addView: 视图为空");
        return;
    }

    if (position > this->children.size())
    {
        Logger::error("Box::addView: 插入位置越界");
        return;
    }

    if (view->getParent() != nullptr)
    {
        Logger::error("Box::addView: 该视图已有父节点，请先 removeView");
        return;
    }

    // Yoga 不允许带测量函数的节点再有子节点（会直接抛异常崩掉）。
    // 这里提前拦成一条明确报错：外观驱动控件默认是叶子，容器就不该开测量。
    if (YGNodeHasMeasureFunc(this->ygNode))
    {
        Logger::error("Box::addView: " + this->getClassString()
            + " 注册了测量函数，不能再有子节点（Yoga 限制）");
        return;
    }

    // 1. C++ 对象树
    this->children.insert(this->children.begin() + static_cast<ptrdiff_t>(position), view);

    // 2. Yoga 节点树
    view->setParent(this);
    YGNodeInsertChild(this->ygNode, view->getYGNode(), position);

    this->invalidate();
}

void Box::removeView(View* view, bool free)
{
    if (view == nullptr)
        return;

    auto it = std::find(this->children.begin(), this->children.end(), view);
    if (it == this->children.end())
        return;

    this->children.erase(it);

    // Yoga 节点树同步移除；YGNodeRemoveChild 会保证节点的所有权回到调用方。
    YGNodeRemoveChild(this->ygNode, view->getYGNode());

    view->setParent(nullptr);

    this->invalidate();

    if (free)
        delete view;
}

void Box::removeAllViews()
{
    // 逐个摘除并同步 Yoga 树，但不 delete。
    while (!this->children.empty())
        this->removeView(this->children.back(), false);
}

void Box::clearViews()
{
    // 逐个摘除：removeView 会同步 Yoga 树，也可以安全地触发子节点析构。
    while (!this->children.empty())
        this->removeView(this->children.back(), true);

    this->invalidate();
}

View* Box::getView(const std::string& id)
{
    // 先看自己（子类可能覆盖），再深度优先遍历子树。
    if (this->getId() == id)
        return this;

    for (View* child : this->children)
    {
        View* found = child->getView(id);
        if (found != nullptr)
            return found;
    }

    return nullptr;
}

View* Box::getDefaultFocus()
{
    // 深度优先，返回第一个可聚焦的后代。
    for (View* child : this->children)
    {
        if (child->getVisibility() != Visibility::VISIBLE)
            continue;

        if (child->isFocusable())
            return child;

        if (View* nested = child->getDefaultFocus())
            return nested;
    }
    return nullptr;
}

View* Box::getNextFocus(FocusDirection direction, View* currentView)
{
    if (direction == FocusDirection::NONE)
        return nullptr;

    // 注意：这里不能因为自己没有子节点就直接返回。
    // 叶子 Box（比如列表里的一行）要交给父容器去找同层的下一个目标。

    const Point origin = currentView != nullptr ? Point(currentView->getFrame().getCenterX(),
                                              currentView->getFrame().getCenterY())
                                                : Point(this->getFrame().getCenterX(),
                                                      this->getFrame().getCenterY());

    View* best      = nullptr;
    float bestScore = 0.0f;

    for (View* child : this->children)
    {
        if (child == currentView || child->getVisibility() != Visibility::VISIBLE)
            continue;

        // 先看这一层有没有可聚焦的子节点；没有就进子树找。
        View* candidate = child->isFocusable() ? child : child->getDefaultFocus();
        if (candidate == nullptr)
            continue;

        const Point target(candidate->getFrame().getCenterX(), candidate->getFrame().getCenterY());
        const float dx = target.x - origin.x;
        const float dy = target.y - origin.y;

        // 只考虑确实位于该方向上的候选：主轴偏移要够大，交叉轴偏差不能太大。
        float primary   = 0.0f;
        float secondary = 0.0f;
        switch (direction)
        {
            case FocusDirection::RIGHT:
                primary = dx;
                secondary = std::fabs(dy);
                break;
            case FocusDirection::LEFT:
                primary = -dx;
                secondary = std::fabs(dy);
                break;
            case FocusDirection::DOWN:
                primary = dy;
                secondary = std::fabs(dx);
                break;
            case FocusDirection::UP:
                primary = -dy;
                secondary = std::fabs(dx);
                break;
            default:
                return nullptr;
        }

        const bool sameRow = this->axis == Axis::ROW ? std::fabs(dy) < 2.0f : true;
        const bool sameCol = this->axis == Axis::COLUMN ? std::fabs(dx) < 2.0f : true;

        // 主轴方向上必须真的前进；主轴上对齐时优先选交叉轴最近的那个。
        if (primary < 1.0f)
        {
            if (!(primary > -1.0f && (sameRow || sameCol)))
                continue;
            // 主轴几乎对齐：允许纯交叉轴方向移动（比如在列表行内左右切换）。
            if (!sameRow && !sameCol)
                continue;
        }

        // 打分：交叉轴偏差权重远高于主轴距离，
        // 这样"下一行同一列"优先于"当前行右边的下一行"，不会走成阶梯。
        const float score = secondary * 1000.0f + primary;

        if (best == nullptr || score < bestScore)
        {
            best      = candidate;
            bestScore = score;
        }

        // 交叉轴完全对齐时不可能再有更优解（除非距离是负的），可以提前结束。
        if (secondary < 0.5f)
            break;
    }

    // 本层没找到就交给父容器继续找。
    if (best == nullptr && this->hasParent())
        return this->parent->getNextFocus(direction, currentView);

    return best;
}

View* Box::hitTest(Point point)
{
    // point 是屏幕坐标；先换算成自己的局部坐标，再往下传。
    const float localX = point.x - this->getX();
    const float localY = point.y - this->getY();

    const Rect bounds(0.0f, 0.0f, this->getWidth(), this->getHeight());
    const Point local(localX, localY);

    // 裁剪的容器只认自己范围内的点。
    if (this->clipsToBounds && !bounds.contains(local))
        return nullptr;

    // 从最后添加的子节点往前找：后添加的在更上层。
    // 传给子节点的是屏幕坐标，子节点自己会再换算。
    for (auto it = this->children.rbegin(); it != this->children.rend(); ++it)
    {
        View* child = *it;
        if (child->getVisibility() != Visibility::VISIBLE)
            continue;

        if (View* hit = child->hitTest(point))
            return hit;
    }

    // 自己兜底：可聚焦且点在自己范围内。
    if (this->isFocusable() && bounds.contains(local))
        return this;

    return nullptr;
}

// ---------------------------------------------------------------------------
// Flexbox 布局属性
// ---------------------------------------------------------------------------

void Box::setAxis(Axis axis)
{
    this->axis = axis;
    YGNodeStyleSetFlexDirection(this->ygNode, getYGFlexDirection(axis));
    this->invalidate();
}

void Box::setDirection(Direction direction)
{
    switch (direction)
    {
        case Direction::INHERIT:
            YGNodeStyleSetDirection(this->ygNode, YGDirectionInherit);
            break;
        case Direction::LEFT_TO_RIGHT:
            YGNodeStyleSetDirection(this->ygNode, YGDirectionLTR);
            break;
        case Direction::RIGHT_TO_LEFT:
            YGNodeStyleSetDirection(this->ygNode, YGDirectionRTL);
            break;
    }
    this->invalidate();
}

void Box::setJustifyContent(JustifyContent justify)
{
    switch (justify)
    {
        case JustifyContent::DEFAULT:
        case JustifyContent::FLEX_START:
            YGNodeStyleSetJustifyContent(this->ygNode, YGJustifyFlexStart);
            break;
        case JustifyContent::CENTER:
            YGNodeStyleSetJustifyContent(this->ygNode, YGJustifyCenter);
            break;
        case JustifyContent::FLEX_END:
            YGNodeStyleSetJustifyContent(this->ygNode, YGJustifyFlexEnd);
            break;
        case JustifyContent::SPACE_BETWEEN:
            YGNodeStyleSetJustifyContent(this->ygNode, YGJustifySpaceBetween);
            break;
        case JustifyContent::SPACE_AROUND:
            YGNodeStyleSetJustifyContent(this->ygNode, YGJustifySpaceAround);
            break;
        case JustifyContent::SPACE_EVENLY:
            YGNodeStyleSetJustifyContent(this->ygNode, YGJustifySpaceEvenly);
            break;
    }
    this->invalidate();
}

void Box::setAlignItems(AlignItems alignment)
{
    switch (alignment)
    {
        case AlignItems::DEFAULT:
        case AlignItems::STRETCH:
            YGNodeStyleSetAlignItems(this->ygNode, YGAlignStretch);
            break;
        case AlignItems::AUTO:
            YGNodeStyleSetAlignItems(this->ygNode, YGAlignAuto);
            break;
        case AlignItems::FLEX_START:
            YGNodeStyleSetAlignItems(this->ygNode, YGAlignFlexStart);
            break;
        case AlignItems::CENTER:
            YGNodeStyleSetAlignItems(this->ygNode, YGAlignCenter);
            break;
        case AlignItems::FLEX_END:
            YGNodeStyleSetAlignItems(this->ygNode, YGAlignFlexEnd);
            break;
        case AlignItems::BASELINE:
            YGNodeStyleSetAlignItems(this->ygNode, YGAlignBaseline);
            break;
    }
    this->invalidate();
}

// ---------------------------------------------------------------------------
// 内边距
// ---------------------------------------------------------------------------

void Box::setPadding(float padding)
{
    this->setPadding(padding, padding, padding, padding);
}

void Box::setPadding(float top, float right, float bottom, float left)
{
    YGNodeStyleSetPadding(this->ygNode, YGEdgeTop, top);
    YGNodeStyleSetPadding(this->ygNode, YGEdgeRight, right);
    YGNodeStyleSetPadding(this->ygNode, YGEdgeBottom, bottom);
    YGNodeStyleSetPadding(this->ygNode, YGEdgeLeft, left);
    this->invalidate();
}

void Box::setPaddingTop(float top)
{
    YGNodeStyleSetPadding(this->ygNode, YGEdgeTop, top);
    this->invalidate();
}

void Box::setPaddingRight(float right)
{
    YGNodeStyleSetPadding(this->ygNode, YGEdgeRight, right);
    this->invalidate();
}

void Box::setPaddingBottom(float bottom)
{
    YGNodeStyleSetPadding(this->ygNode, YGEdgeBottom, bottom);
    this->invalidate();
}

void Box::setPaddingLeft(float left)
{
    YGNodeStyleSetPadding(this->ygNode, YGEdgeLeft, left);
    this->invalidate();
}

static float toFloat(YGValue value)
{
    return value.unit == YGUnitUndefined ? 0.0f : value.value;
}

float Box::getPaddingTop() const
{
    return toFloat(YGNodeStyleGetPadding(this->ygNode, YGEdgeTop));
}

float Box::getPaddingRight() const
{
    return toFloat(YGNodeStyleGetPadding(this->ygNode, YGEdgeRight));
}

float Box::getPaddingBottom() const
{
    return toFloat(YGNodeStyleGetPadding(this->ygNode, YGEdgeBottom));
}

float Box::getPaddingLeft() const
{
    return toFloat(YGNodeStyleGetPadding(this->ygNode, YGEdgeLeft));
}

// ---------------------------------------------------------------------------
// 间距与换行
// ---------------------------------------------------------------------------

void Box::setGap(float gap)
{
    if (this->axis == Axis::ROW)
        YGNodeStyleSetGap(this->ygNode, YGGutterColumn, gap);
    else
        YGNodeStyleSetGap(this->ygNode, YGGutterRow, gap);
    this->invalidate();
}

void Box::setGapColumn(float gap)
{
    YGNodeStyleSetGap(this->ygNode, YGGutterColumn, gap);
    this->invalidate();
}

void Box::setGapRow(float gap)
{
    YGNodeStyleSetGap(this->ygNode, YGGutterRow, gap);
    this->invalidate();
}

void Box::setFlexWrap(bool wrap)
{
    YGNodeStyleSetFlexWrap(this->ygNode, wrap ? YGWrapWrap : YGWrapNoWrap);
    this->invalidate();
}

// ---------------------------------------------------------------------------
// 每帧逻辑
// ---------------------------------------------------------------------------

void Box::update()
{
    for (View* child : this->children)
    {
        if (child->getVisibility() == Visibility::GONE)
            continue;
        child->update();
    }
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

void Box::drawChildren(ImDrawList*, float x, float y, float width, float height, FrameContext* ctx)
{
    for (View* child : this->children)
    {
        if (child->getVisibility() != Visibility::VISIBLE)
            continue;

        // 简单剔除：完全落在本容器之外且本容器裁剪时跳过。
        if (this->clipsToBounds)
        {
            const Rect bounds(x, y, width, height);
            if (!bounds.intersects(child->getFrame()))
                continue;
        }

        child->frame(ctx);
    }
}


namespace
{

// ---- Box 自己的属性：Flexbox 容器相关 ----

void setAxisAttr(View* v, const ResolvedValue& r)
{
    v->asBox()->setAxis(r.text == "row" ? Axis::ROW : Axis::COLUMN);
}

void setDirectionAttr(View* v, const ResolvedValue& r)
{
    Box* box          = v->asBox();
    const Direction d = r.text == "ltr" ? Direction::LEFT_TO_RIGHT
                                        : (r.text == "rtl" ? Direction::RIGHT_TO_LEFT : Direction::INHERIT);
    box->setDirection(d);
}

void setAlignItemsAttr(View* v, const ResolvedValue& r)
{
    Box* box = v->asBox();
    if (r.text == "flexStart")
        box->setAlignItems(AlignItems::FLEX_START);
    else if (r.text == "center")
        box->setAlignItems(AlignItems::CENTER);
    else if (r.text == "flexEnd")
        box->setAlignItems(AlignItems::FLEX_END);
    else if (r.text == "baseline")
        box->setAlignItems(AlignItems::BASELINE);
    else if (r.text == "auto")
        box->setAlignItems(AlignItems::AUTO);
    else
        box->setAlignItems(AlignItems::STRETCH);
}

void setJustifyContentAttr(View* v, const ResolvedValue& r)
{
    Box* box = v->asBox();
    if (r.text == "center")
        box->setJustifyContent(JustifyContent::CENTER);
    else if (r.text == "flexEnd")
        box->setJustifyContent(JustifyContent::FLEX_END);
    else if (r.text == "spaceBetween")
        box->setJustifyContent(JustifyContent::SPACE_BETWEEN);
    else if (r.text == "spaceAround")
        box->setJustifyContent(JustifyContent::SPACE_AROUND);
    else if (r.text == "spaceEvenly")
        box->setJustifyContent(JustifyContent::SPACE_EVENLY);
    else
        box->setJustifyContent(JustifyContent::FLEX_START);
}

void setFlexWrapAttr(View* v, const ResolvedValue& r)
{
    v->asBox()->setFlexWrap(r.boolean);
}

void setGapAttr(View* v, const ResolvedValue& r)
{
    v->asBox()->setGap(r.number);
}

void setGapRowAttr(View* v, const ResolvedValue& r)
{
    v->asBox()->setGapRow(r.number);
}

void setGapColumnAttr(View* v, const ResolvedValue& r)
{
    v->asBox()->setGapColumn(r.number);
}

void setPaddingAttr(View* v, const ResolvedValue& r)
{
    v->asBox()->setPadding(r.number, r.number, r.number, r.number);
}

void setPaddingTopAttr(View* v, const ResolvedValue& r)
{
    v->asBox()->setPaddingTop(r.number);
}

void setPaddingRightAttr(View* v, const ResolvedValue& r)
{
    v->asBox()->setPaddingRight(r.number);
}

void setPaddingBottomAttr(View* v, const ResolvedValue& r)
{
    v->asBox()->setPaddingBottom(r.number);
}

void setPaddingLeftAttr(View* v, const ResolvedValue& r)
{
    v->asBox()->setPaddingLeft(r.number);
}

} // namespace

const AttributeTable& Box::attributes() const
{
    static const AttributeTable table = {
        { "axis", AttributeKind::Enum, setAxisAttr, "row|column" },
        { "direction", AttributeKind::Enum, setDirectionAttr, "inherit|ltr|rtl" },
        { "alignItems", AttributeKind::Enum, setAlignItemsAttr,
            "stretch|flexStart|center|flexEnd|baseline|auto" },
        { "justifyContent", AttributeKind::Enum, setJustifyContentAttr,
            "flexStart|center|flexEnd|spaceBetween|spaceAround|spaceEvenly" },
        { "flexWrap", AttributeKind::Bool, setFlexWrapAttr, nullptr },
        { "gap", AttributeKind::Number, setGapAttr, nullptr },
        { "gapRow", AttributeKind::Number, setGapRowAttr, nullptr },
        { "gapColumn", AttributeKind::Number, setGapColumnAttr, nullptr },
        { "padding", AttributeKind::Number, setPaddingAttr, nullptr },
        { "paddingTop", AttributeKind::Number, setPaddingTopAttr, nullptr },
        { "paddingRight", AttributeKind::Number, setPaddingRightAttr, nullptr },
        { "paddingBottom", AttributeKind::Number, setPaddingBottomAttr, nullptr },
        { "paddingLeft", AttributeKind::Number, setPaddingLeftAttr, nullptr },
    };
    return table;
}

} // namespace bklv

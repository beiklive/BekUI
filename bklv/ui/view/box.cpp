#include "bklv/ui/view/box.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/foundation/core/logger.hpp"

namespace bklv
{

// Axis 到 Yoga flex-direction 的映射。
static YGFlexDirection getYGFlexDirection(Axis axis)
{
    return axis == Axis::ROW ? YGFlexDirectionRow : YGFlexDirectionColumn;
}

namespace
{

// node 是否在 root 这棵子树里（含 root 自己）。
// 自定义的 getDefaultFocus() 可能返回任意视图，用它把"绕回出发的子树"挡掉。
bool isInSubtree(const View* node, const View* root)
{
    for (const View* v = node; v != nullptr; v = v->getParent())
    {
        if (v == root)
            return true;
    }
    return false;
}

} // namespace

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

    // 上次焦点记忆指向被摘掉的子节点就清掉，否则"删掉聚焦项再进来"
    // 会拿到悬空指针（borealis 这里没清，是它的一个隐患）。
    if (this->lastFocusedView == view)
        this->lastFocusedView = nullptr;

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
    // ① 自己可聚焦（borealis 的第一级）
    if (getInputManager().canFocus(this))
        return this;

    // ② 上次焦点记忆：上次在子树里被聚焦过哪一支就回到哪一支
    //    （lastFocusedView 一直是自己的直接子节点，摘除子节点时会清掉）
    if (this->lastFocusedView != nullptr && this->lastFocusedView->getParent() == this)
    {
        if (View* remembered = this->lastFocusedView->getDefaultFocus())
        {
            if (getInputManager().canFocus(remembered))
                return remembered;
        }
    }

    // ③ 指定索引
    if (this->defaultFocusedIndex >= 0
        && (size_t)this->defaultFocusedIndex < this->children.size())
    {
        if (View* indexed = this->children[(size_t)this->defaultFocusedIndex]->getDefaultFocus())
        {
            if (getInputManager().canFocus(indexed))
                return indexed;
        }
    }

    // ④ 顺序扫第一个能聚焦的子节点
    for (View* child : this->children)
    {
        if (View* first = child->getDefaultFocus())
        {
            if (getInputManager().canFocus(first))
                return first;
        }
    }

    return nullptr;
}

void Box::onChildFocusGained(View* directChild, View* focusedView)
{
    // 只记"哪一支"：恢复这个 box 的默认焦点时就从这一支往下找。
    // 不再往上冒泡——InputManager::giveFocus 已经沿整条焦点路径逐级调用过了
    // （两边都冒泡会重复记同一支，也不利于 ScrollingFrame 之类的重写）。
    (void)focusedView;
    this->lastFocusedView = directChild;
}

View* Box::getParentNavigationDecision(View* /*from*/, View* newFocus,
    FocusDirection /*direction*/)
{
    // 默认没意见：谁找到的候选就是谁。
    // 滚动容器重写成"返回 this"表示焦点不动、自己滚动。
    return newFocus;
}

View* Box::focusCandidateAt(size_t index, View* from)
{
    if (index >= this->children.size())
        return nullptr;

    View* child = this->children[index];
    if (child == nullptr || child->getVisibility() != Visibility::VISIBLE)
        return nullptr;

    View* candidate = child->getDefaultFocus();

    // 不能绕回出发的那棵子树（步进器那种自定义 getDefaultFocus 可能指回自己）
    if (candidate == nullptr || isInSubtree(candidate, from))
        return nullptr;

    // 可聚焦 + 未禁用 + 沿父链可见 + 未被模态遮挡。
    // 被模态挡住的候选直接跳过、继续往后扫，而不是把整个导航判成"撞边"。
    if (!getInputManager().canFocus(candidate))
        return nullptr;

    return candidate;
}

View* Box::scanChildrenForFocus(FocusDirection direction, View* from)
{
    const ptrdiff_t count = (ptrdiff_t)this->children.size();
    if (count <= 0)
        return nullptr;

    // from 必须是自己的直接子节点；不是就说明调用姿势不对，本层不处理。
    ptrdiff_t index = -1;
    for (ptrdiff_t i = 0; i < count; i++)
    {
        if (this->children[(size_t)i] == from)
        {
            index = i;
            break;
        }
    }
    if (index < 0)
        return nullptr;

    const ptrdiff_t offset =
        (direction == FocusDirection::LEFT || direction == FocusDirection::UP) ? -1 : 1;

    if (!this->isNavigationWrap())
    {
        // 不循环：±1 顺序扫到越界为止（越界 = 交给父容器冒泡）
        for (ptrdiff_t i = index + offset; i >= 0 && i < count; i += offset)
        {
            if (View* candidate = this->focusCandidateAt((size_t)i, from))
                return candidate;
        }
        return nullptr;
    }

    // 循环：绕一圈回到 from 之前（只在**本 box 内**绕，不碰跨 box 策略）
    for (ptrdiff_t step = 1; step < count; step++)
    {
        const ptrdiff_t i = ((index + offset * step) % count + count) % count;
        if (View* candidate = this->focusCandidateAt((size_t)i, from))
            return candidate;
    }
    return nullptr;
}

View* Box::leaveThisBox(FocusDirection direction)
{
    switch (this->crossBoxPolicy)
    {
        case CrossBoxPolicy::NONE:
            // 主动拦住：静默，不算"撞边"（不触发反馈）
            getInputManager().markNavigationBlockedByPolicy();
            return nullptr;

        case CrossBoxPolicy::SAME_PARENT_ONLY:
        {
            Box* parent = this->parent;
            const bool horizontal =
                (direction == FocusDirection::LEFT || direction == FocusDirection::RIGHT);
            if (parent == nullptr || (parent->axis == Axis::ROW) != horizontal)
            {
                getInputManager().markNavigationBlockedByPolicy();
                return nullptr;
            }

            // 只让父容器在自己这一层里扫兄弟，不再往上冒
            if (View* sibling = parent->scanChildrenForFocus(direction, this))
                return sibling;

            getInputManager().markNavigationBlockedByPolicy();
            return nullptr;
        }

        case CrossBoxPolicy::ALLOW:
            break;
    }

    // 越界：交给父容器继续找（真冒泡）
    if (this->hasParent())
        return this->parent->getNextFocus(direction, this);

    return nullptr;
}

View* Box::getNextFocus(FocusDirection direction, View* currentView)
{
    if (direction == FocusDirection::NONE)
        return nullptr;

    // 轴向不符（row 按 UP/DOWN、column 按 LEFT/RIGHT）→ 立刻上交父容器，
    // 不在本层做跨轴扫描（borealis 同义：这种按法不属于这个容器管）。
    const bool horizontal  = (direction == FocusDirection::LEFT || direction == FocusDirection::RIGHT);
    const bool axisMatches = (this->axis == Axis::ROW) == horizontal;

    View* candidate = axisMatches ? this->scanChildrenForFocus(direction, currentView) : nullptr;

    // 容器的最后决定权：滚动容器在这里返回 this（焦点不动、我来把它滚进视野）
    if (View* decision = this->getParentNavigationDecision(currentView, candidate, direction))
        return decision;

    if (candidate != nullptr)
        return candidate;

    return this->leaveThisBox(direction);
}

void Box::setNavigationWrap(bool wrap)
{
    this->navigationWrap    = wrap;
    this->navigationWrapSet = true;
}

bool Box::isNavigationWrap() const
{
    if (this->navigationWrapSet)
        return this->navigationWrap;

    // 没设过就跟着最近的祖先（"作用域是单个 box，影响其子树、内层可覆盖"）
    if (this->parent != nullptr)
        return this->parent->isNavigationWrap();

    return false;
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

    // 自己兜底：能点（默认就是可聚焦）且点在自己范围内。
    if (this->isPointerTarget() && bounds.contains(local))
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
    this->justifyContent = justify;
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
    this->alignItems = alignment;
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

void Box::setAlignContent(AlignContent content)
{
    this->alignContent = content;
    switch (content)
    {
        case AlignContent::FLEX_START: YGNodeStyleSetAlignContent(this->ygNode, YGAlignFlexStart); break;
        case AlignContent::CENTER: YGNodeStyleSetAlignContent(this->ygNode, YGAlignCenter); break;
        case AlignContent::FLEX_END: YGNodeStyleSetAlignContent(this->ygNode, YGAlignFlexEnd); break;
        case AlignContent::STRETCH: YGNodeStyleSetAlignContent(this->ygNode, YGAlignStretch); break;
        case AlignContent::SPACE_BETWEEN: YGNodeStyleSetAlignContent(this->ygNode, YGAlignSpaceBetween); break;
        case AlignContent::SPACE_AROUND: YGNodeStyleSetAlignContent(this->ygNode, YGAlignSpaceAround); break;
    }
    this->invalidate();
}

void Box::setFlexBasis(float basis)
{
    YGNodeStyleSetFlexBasis(this->ygNode, basis);
    this->invalidate();
}

void Box::setFlexBasisAuto()
{
    YGNodeStyleSetFlexBasisAuto(this->ygNode);
    this->invalidate();
}

float Box::getFlexBasis() const
{
    return YGNodeStyleGetFlexBasis(this->ygNode).value; // auto 时是 NaN
}

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

float Box::getGap() const
{
    // setGap 只设**当前轴**对应的那条 gutter（读 YGGutterAll 在 Yoga 里是不定的）
    return YGNodeStyleGetGap(this->ygNode, this->axis == Axis::ROW ? YGGutterColumn : YGGutterRow);
}

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

void Box::drawChildren(ImDrawList*, float, float, float, float, FrameContext* ctx)
{
    // 会裁剪的容器把"自己的矩形"下发给后代（由后代继续继承）。
    // 这样滚动器里的深层子树（比如一页 68 行图标）也能被剔除，而不是只剔直接子节点。
    const bool  savedHasCull = ctx->hasCull;
    const float savedX = ctx->cullX, savedY = ctx->cullY;
    const float savedW = ctx->cullW, savedH = ctx->cullH;
    if (this->clipsToBounds)
    {
        // 用**布局坐标**（self frame）而不是 draw 参数：后者是绘制空间，和 getFrame() 不同源
        const Rect self = this->getFrame();
        ctx->hasCull = true;
        ctx->cullX    = self.x;
        ctx->cullY    = self.y;
        ctx->cullW    = self.width;
        ctx->cullH    = self.height;
    }

    for (View* child : this->children)
    {
        if (child->getVisibility() != Visibility::VISIBLE)
            continue;

        // 剔除：完全落在当前裁剪窗口之外 → 整棵子树跳过（update + draw 都不跑）。
        // 动画在滚动出去期间会停住，滚回来从当前值继续（dt 有钳制，不会瞬移）。
        if (ctx->hasCull)
        {
            const float margin = ctx->cullMargin;
            const Rect  cull(ctx->cullX - margin, ctx->cullY - margin,
                 ctx->cullW + margin * 2.0f, ctx->cullH + margin * 2.0f);
            if (!cull.intersects(child->getFrame()))
                continue;
        }

        child->frame(ctx);
    }

    ctx->hasCull = savedHasCull;
    ctx->cullX = savedX; ctx->cullY = savedY; ctx->cullW = savedW; ctx->cullH = savedH;
}


namespace
{

// ---- Box 自己的属性：Flexbox 容器相关 ----

void setAxisAttr(View* v, const ResolvedValue& r)
{
    if (enumMatches(r.text, "row"))
        v->asBox()->setAxis(Axis::ROW);
    else if (enumMatches(r.text, "column"))
        v->asBox()->setAxis(Axis::COLUMN);
    else
        warnUnknownEnumValue("axis", r.text, "row|column");
}

void setDirectionAttr(View* v, const ResolvedValue& r)
{
    Box* box = v->asBox();
    if (enumMatches(r.text, "ltr"))
        box->setDirection(Direction::LEFT_TO_RIGHT);
    else if (enumMatches(r.text, "rtl"))
        box->setDirection(Direction::RIGHT_TO_LEFT);
    else if (enumMatches(r.text, "inherit"))
        box->setDirection(Direction::INHERIT);
    else
        warnUnknownEnumValue("direction", r.text, "inherit|ltr|rtl");
}

void setAlignItemsAttr(View* v, const ResolvedValue& r)
{
    Box* box = v->asBox();
    if (enumMatches(r.text, "flexStart"))
        box->setAlignItems(AlignItems::FLEX_START);
    else if (enumMatches(r.text, "center"))
        box->setAlignItems(AlignItems::CENTER);
    else if (enumMatches(r.text, "flexEnd"))
        box->setAlignItems(AlignItems::FLEX_END);
    else if (enumMatches(r.text, "baseline"))
        box->setAlignItems(AlignItems::BASELINE);
    else if (enumMatches(r.text, "auto"))
        box->setAlignItems(AlignItems::AUTO);
    else if (enumMatches(r.text, "stretch"))
        box->setAlignItems(AlignItems::STRETCH);
    else
        warnUnknownEnumValue("alignItems", r.text,
            "flexStart|center|flexEnd|stretch|baseline|auto");
}

void setJustifyContentAttr(View* v, const ResolvedValue& r)
{
    Box* box = v->asBox();
    if (enumMatches(r.text, "flexStart"))
        box->setJustifyContent(JustifyContent::FLEX_START);
    else if (enumMatches(r.text, "center"))
        box->setJustifyContent(JustifyContent::CENTER);
    else if (enumMatches(r.text, "flexEnd"))
        box->setJustifyContent(JustifyContent::FLEX_END);
    else if (enumMatches(r.text, "spaceBetween"))
        box->setJustifyContent(JustifyContent::SPACE_BETWEEN);
    else if (enumMatches(r.text, "spaceAround"))
        box->setJustifyContent(JustifyContent::SPACE_AROUND);
    else if (enumMatches(r.text, "spaceEvenly"))
        box->setJustifyContent(JustifyContent::SPACE_EVENLY);
    else
        warnUnknownEnumValue("justifyContent", r.text,
            "flexStart|center|flexEnd|spaceBetween|spaceAround|spaceEvenly");
}

void setFlexWrapAttr(View* v, const ResolvedValue& r)
{
    v->asBox()->setFlexWrap(r.boolean);
}

void setAlignContentAttr(View* v, const ResolvedValue& r)
{
    Box* box = v->asBox();
    if (enumMatches(r.text, "flexStart")) box->setAlignContent(AlignContent::FLEX_START);
    else if (enumMatches(r.text, "center")) box->setAlignContent(AlignContent::CENTER);
    else if (enumMatches(r.text, "flexEnd")) box->setAlignContent(AlignContent::FLEX_END);
    else if (enumMatches(r.text, "stretch")) box->setAlignContent(AlignContent::STRETCH);
    else if (enumMatches(r.text, "spaceBetween")) box->setAlignContent(AlignContent::SPACE_BETWEEN);
    else if (enumMatches(r.text, "spaceAround")) box->setAlignContent(AlignContent::SPACE_AROUND);
    else
        warnUnknownEnumValue("alignContent", r.text,
            "flexStart|center|flexEnd|stretch|spaceBetween|spaceAround");
}

void setFlexBasisAttr(View* v, const ResolvedValue& r)
{
    Box* box = v->asBox();
    if (std::isnan(r.number))
    {
        box->setFlexBasisAuto();
        return;
    }
    if (r.percent)
    {
        // 百分比 basis 在 Yoga 里要另外设（YGNodeStyleSetFlexBasisPercent）。这里不静默丢掉：
        // 说清楚目前只支持像素与 auto，避免"写了没反应"。
        BKLV_LOG_WARNING("flexBasis 暂不支持百分比（%g%%），按 auto 处理；要百分比请用 width/height",
            r.number);
        box->setFlexBasisAuto();
        return;
    }
    box->setFlexBasis(r.number);
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
        { "axis", AttributeKind::ENUM, setAxisAttr, "row|column" },
        { "direction", AttributeKind::ENUM, setDirectionAttr, "inherit|ltr|rtl" },
        { "alignItems", AttributeKind::ENUM, setAlignItemsAttr,
            "stretch|flexStart|center|flexEnd|baseline|auto" },
        { "justifyContent", AttributeKind::ENUM, setJustifyContentAttr,
            "flexStart|center|flexEnd|spaceBetween|spaceAround|spaceEvenly" },
        { "flexWrap", AttributeKind::BOOL, setFlexWrapAttr, nullptr },
        { "alignContent", AttributeKind::ENUM, setAlignContentAttr,
            "flexStart|center|flexEnd|stretch|spaceBetween|spaceAround" },
        { "flexBasis", AttributeKind::SIZE, setFlexBasisAttr, nullptr, true },
        { "gap", AttributeKind::NUMBER, setGapAttr, nullptr },
        { "gapRow", AttributeKind::NUMBER, setGapRowAttr, nullptr },
        { "gapColumn", AttributeKind::NUMBER, setGapColumnAttr, nullptr },
        { "padding", AttributeKind::NUMBER, setPaddingAttr, nullptr },
        { "paddingTop", AttributeKind::NUMBER, setPaddingTopAttr, nullptr },
        { "paddingRight", AttributeKind::NUMBER, setPaddingRightAttr, nullptr },
        { "paddingBottom", AttributeKind::NUMBER, setPaddingBottomAttr, nullptr },
        { "paddingLeft", AttributeKind::NUMBER, setPaddingLeftAttr, nullptr },
    };
    return table;
}

} // namespace bklv

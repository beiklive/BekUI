#include "bklv/core/event.hpp"
#include "bklv/core/view.hpp"

#include <algorithm>
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

void View::setFlexBasis(float basis)
{
    YGNodeStyleSetFlexBasis(this->ygNode, basis);
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

void View::setCornerRadius(float radius, bool explicitValue)
{
    this->cornerRadius         = radius;
    this->cornerRadiusExplicit = explicitValue;
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
    //
    // 注意这里是**向下递归**：只把根节点标脏是不够的 —— Yoga 会复用子节点缓存的
    // 测量结果，Label 不重新测量的话文字宽度就永远是旧值。以前这个函数只向上冒泡，
    // 调在根上等于只标了根自己（根通常没有测量函数，于是什么都没做）；
    // 只是因为"字体就绪早于首次布局"才没暴露出来。
    if (this->fontGeneration != getFontGeneration())
    {
        this->fontGeneration = getFontGeneration();
        this->markMeasureDirty();
    }

    if (Box* box = this->asBox())
        for (View* child : box->getChildren())
            child->refreshFontDependentLayout();

    // 整棵树标完脏，交给 App::frame 统一重算一次。
    //
    // 不在这里直接算：这个函数只从 App::frame 调用，紧接着就是布局块，
    // 直接算等于一帧算两次。也**不要**用 YGUndefined 硬算 —— 那样百分比宽高
    // 会全部失去参照，整棵树塌成内容尺寸（这里踩过一次）。
    App::requestLayout();
}

void View::invalidate()
{
    this->markMeasureDirty();

    // 冒泡到根，由根标记"需要重新布局"，真正的那一次由 App::frame 统一做。
    //
    // 为什么不在这里立刻算：回收列表一帧里会新建/回收十几个 cell，每个都
    // addView -> invalidate()，立刻算的话一帧要全树重算十几次（实测 JSON 页
    // 10 次/帧）。改成标记后一帧只算一次。
    //
    // borealis 是在这里直接算的；bklv 延迟一帧，代价是"本帧内改的内容下一帧生效"，
    // 收益是回收列表的布局开销降一个数量级。回收器不受影响：cell 的位置是用
    // translation 摆的（见 RecyclerFrame::layoutCell），不依赖本帧的 Yoga 结果。
    if (this->hasParent() && !this->detached)
    {
        this->parent->invalidate();
        return;
    }

    App::requestLayout();
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

    // 记录本视图（含子树）会往绘制列表里追加的顶点起点：
    // 画完之后整段按自己的变换矩阵走一遍。子节点先按自己的矩阵变过，
    // 父节点再整体变 → 等价于 M_parent * M_child，顺序天然正确。
    bool transformed   = !this->transform.isIdentity();
    const int vtxStart = transformed ? ctx->drawList->VtxBuffer.Size : 0;

    // 变换里混进 NaN/Inf 会让整段顶点变成 NaN，控件就此消失（极难查）。
    // 这里直接拦下来：报一次警告并退化成不做变换——宁可看着"没生效"，也不要看不见。
    if (transformed && !this->transform.isFinite())
    {
        static bool warned = false;
        if (!warned)
        {
            warned = true;
            BKLV_LOG_WARNING("绘制变换里有 NaN/Inf（%s），本次忽略；"
                             "检查 setTranslate/setScale/setSkew/setRotate 的输入",
                this->getClassString().c_str());
        }
        transformed = false;
    }


    this->drawBackground(ctx->drawList, x, y, width, height);

    if (this->borderThickness > 0.0f)
        this->drawBorder(ctx->drawList, x, y, width, height);

    // 裁剪：只有显式设置 clipsToBounds 的节点才限制子节点绘制范围。
    const bool clipped = this->clipsToBounds && width > 0.0f && height > 0.0f;
    if (clipped)
    {
        if (!transformed)
        {
            ctx->drawList->PushClipRect(ImVec2(x, y), ImVec2(x + width, y + height), true);
        }
        else
        {
            // 变换后的裁剪框只能是轴对齐包围盒（ImGui 的裁剪框不能旋转），
            // 按四个角变换后取 min/max 近似
            const Point center(x + width * 0.5f, y + height * 0.5f);
            const Point corners[4] = {
                this->transform.apply(x, y, center),
                this->transform.apply(x + width, y, center),
                this->transform.apply(x + width, y + height, center),
                this->transform.apply(x, y + height, center),
            };

            float minX = corners[0].x, maxX = corners[0].x;
            float minY = corners[0].y, maxY = corners[0].y;
            for (int i = 1; i < 4; i++)
            {
                minX = std::min(minX, corners[i].x);
                maxX = std::max(maxX, corners[i].x);
                minY = std::min(minY, corners[i].y);
                maxY = std::max(maxY, corners[i].y);
            }

            ctx->drawList->PushClipRect(ImVec2(minX, minY), ImVec2(maxX, maxY), true);
        }
    }

    this->draw(ctx->drawList, x, y, width, height, ctx);
    this->drawChildren(ctx->drawList, x, y, width, height, ctx);

    if (clipped)
        ctx->drawList->PopClipRect();

    if (transformed)
    {
        const Point center(x + width * 0.5f, y + height * 0.5f);
        ImDrawList* drawList = ctx->drawList;
        for (int i = vtxStart; i < drawList->VtxBuffer.Size; i++)
        {
            const ImVec2& pos = drawList->VtxBuffer[i].pos;
            const Point moved = this->transform.apply(pos.x, pos.y, center);
            drawList->VtxBuffer[i].pos = ImVec2(moved.x, moved.y);
        }
    }
}

// ---------------------------------------------------------------------------
// 位移
// ---------------------------------------------------------------------------

void View::addGesture(std::unique_ptr<GestureRecognizer> gesture)
{
    if (gesture == nullptr)
        return;

    this->gestures.push_back(std::move(gesture));
}

bool View::dispatchGesture(GestureState state, const Point& position, const Point& delta)
{
    // 自己先问一遍：识别器按添加顺序，谁先吃掉算谁的
    for (const std::unique_ptr<GestureRecognizer>& gesture : this->gestures)
    {
        if (gesture == nullptr || !gesture->isEnabled())
            continue;

        if (gesture->recognize(state, position, delta))
            return true;
    }

    // 自己没接住就交给父容器（borealis 的 firstResponder 也是沿父链找）
    if (this->hasParent())
        return this->parent->dispatchGesture(state, position, delta);

    return false;
}

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
    // 动作事件：全局 UI / 日志可以订阅（对标 borealis 把点击做成可订阅事件）
    {
        ActionTriggeredEvent event;
        event.view    = this;
        event.button  = static_cast<int>(button);
        event.pointer = pointer;
        getEventBus().publish(event);
    }

    // 指针：只认命中视图自己的动作。hitTest 只会返回可聚焦视图，
    // 所以不需要冒泡（点按钮的文字区域时命中的也是按钮本身）。
    if (pointer)
    {
        for (const InputAction& action : this->actions)
        {
            if (action.button != button || !action.listener)
                continue;

            // 鼠标点击默认只"选中"，不执行动作 —— 否则点一下列表项就会误触发。
            // 控件想支持点一下就执行，把 onPointerAction 覆写成 true 即可。
            if (!this->onPointerAction)
                return false;

            action.listener(this);
            return true;
        }
        return false;
    }

    // 手柄/键盘：自己没注册就**沿父链往上找**。
    // 容器级的动作必须能在焦点位于其子节点时生效（borealis 也是这么做的）：
    //   · AppletFrame 的 "B 返回上一层"
    //   · Dialog 的 "B 关闭"
    // 不冒泡的话，焦点在内容里的按钮上时按 B 完全没反应。
    for (View* owner = this; owner != nullptr; owner = owner->getParent())
    {
        for (const InputAction& action : owner->actions)
        {
            if (action.button != button || !action.listener)
                continue;

            action.listener(owner);
            return true;
        }
    }
    return false;
}


namespace
{

// ---- 公共属性：任何 View 都能用的那批（尺寸 / 间距 / 定位 / 外观 / 可见性）----
//
// 表是函数内 static：整个进程一份，不随实例分配内存
// （borealis 是每个 View 实例挂 7 个 unordered_map，回收列表里纯属浪费）。

void setWidthAttr(View* v, const ResolvedValue& r)
{
    if (r.percent)
        v->setWidthPercentage(r.number);
    else
        v->setWidth(r.number);
}

void setHeightAttr(View* v, const ResolvedValue& r)
{
    if (r.percent)
        v->setHeightPercentage(r.number);
    else
        v->setHeight(r.number);
}

void setMinWidthAttr(View* v, const ResolvedValue& r)
{
    if (r.percent)
        v->setMinWidthPercentage(r.number);
    else
        v->setMinWidth(r.number);
}

void setMinHeightAttr(View* v, const ResolvedValue& r)
{
    if (r.percent)
        v->setMinHeightPercentage(r.number);
    else
        v->setMinHeight(r.number);
}

void setMaxWidthAttr(View* v, const ResolvedValue& r)
{
    if (r.percent)
        v->setMaxWidthPercentage(r.number);
    else
        v->setMaxWidth(r.number);
}

void setMaxHeightAttr(View* v, const ResolvedValue& r)
{
    if (r.percent)
        v->setMaxHeightPercentage(r.number);
    else
        v->setMaxHeight(r.number);
}

void setPositionTopAttr(View* v, const ResolvedValue& r)
{
    if (r.percent)
        v->setPositionTopPercentage(r.number);
    else
        v->setPositionTop(r.number);
}

void setPositionLeftAttr(View* v, const ResolvedValue& r)
{
    if (r.percent)
        v->setPositionLeftPercentage(r.number);
    else
        v->setPositionLeft(r.number);
}

void setPositionRightAttr(View* v, const ResolvedValue& r)
{
    if (r.percent)
        v->setPositionRightPercentage(r.number);
    else
        v->setPositionRight(r.number);
}

void setPositionBottomAttr(View* v, const ResolvedValue& r)
{
    if (r.percent)
        v->setPositionBottomPercentage(r.number);
    else
        v->setPositionBottom(r.number);
}

void setVisibilityAttr(View* v, const ResolvedValue& r)
{
    if (enumMatches(r.text, "visible"))
        v->setVisibility(Visibility::VISIBLE);
    else if (enumMatches(r.text, "invisible"))
        v->setVisibility(Visibility::INVISIBLE);
    else if (enumMatches(r.text, "gone"))
        v->setVisibility(Visibility::GONE);
    else
        warnUnknownEnumValue("visibility", r.text, "visible|invisible|gone");
}

void setPositionTypeAttr(View* v, const ResolvedValue& r)
{
    if (enumMatches(r.text, "absolute"))
        v->setPositionType(PositionType::ABSOLUTE);
    else if (enumMatches(r.text, "relative"))
        v->setPositionType(PositionType::RELATIVE);
    else
        warnUnknownEnumValue("position", r.text, "relative|absolute");
}

void setAlignSelfAttr(View* v, const ResolvedValue& r)
{
    // 注意 setAlignSelf 收的是 AlignSelf，不是 AlignItems——两者值集不同，别混。
    if (enumMatches(r.text, "flexStart"))
        v->setAlignSelf(AlignSelf::FLEX_START);
    else if (enumMatches(r.text, "center"))
        v->setAlignSelf(AlignSelf::CENTER);
    else if (enumMatches(r.text, "flexEnd"))
        v->setAlignSelf(AlignSelf::FLEX_END);
    else if (enumMatches(r.text, "stretch"))
        v->setAlignSelf(AlignSelf::STRETCH);
    else if (enumMatches(r.text, "baseline"))
        v->setAlignSelf(AlignSelf::BASELINE);
    else if (enumMatches(r.text, "auto"))
        v->setAlignSelf(AlignSelf::AUTO);
    else
        warnUnknownEnumValue("alignSelf", r.text,
            "auto|flexStart|center|flexEnd|stretch|baseline");
}

void setMarginAttr(View* v, const ResolvedValue& r)
{
    v->setMargins(r.number, r.number, r.number, r.number);
}

void setBackgroundColorAttr(View* v, const ResolvedValue& r)
{
    v->setBackgroundColor(r.color);
}

void setBorderColorAttr(View* v, const ResolvedValue& r)
{
    v->setBorderColor(r.color);
}

void setBorderThicknessAttr(View* v, const ResolvedValue& r)
{
    v->setBorderThickness(r.number);
}

void setCornerRadiusAttr(View* v, const ResolvedValue& r)
{
    v->setCornerRadius(r.number);
}

void setAlphaAttr(View* v, const ResolvedValue& r)
{
    v->setAlpha(r.number);
}

void setClipsToBoundsAttr(View* v, const ResolvedValue& r)
{
    v->setClipsToBounds(r.boolean);
}

void setMarginTopAttr(View* v, const ResolvedValue& r)
{
    v->setMarginTop(r.number);
}

void setMarginRightAttr(View* v, const ResolvedValue& r)
{
    v->setMarginRight(r.number);
}

void setMarginBottomAttr(View* v, const ResolvedValue& r)
{
    v->setMarginBottom(r.number);
}

void setMarginLeftAttr(View* v, const ResolvedValue& r)
{
    v->setMarginLeft(r.number);
}

void setGrowAttr(View* v, const ResolvedValue& r)
{
    v->setGrow(r.number);
}

void setShrinkAttr(View* v, const ResolvedValue& r)
{
    v->setShrink(r.number);
}

const AttributeTable& commonAttributes()
{
    static const AttributeTable table = {
        { "width", AttributeKind::SIZE, setWidthAttr, nullptr },
        { "height", AttributeKind::SIZE, setHeightAttr, nullptr },
        { "minWidth", AttributeKind::SIZE, setMinWidthAttr, nullptr },
        { "minHeight", AttributeKind::SIZE, setMinHeightAttr, nullptr },
        { "maxWidth", AttributeKind::SIZE, setMaxWidthAttr, nullptr },
        { "maxHeight", AttributeKind::SIZE, setMaxHeightAttr, nullptr },
        { "margin", AttributeKind::NUMBER, setMarginAttr, nullptr },
        { "marginTop", AttributeKind::NUMBER, setMarginTopAttr, nullptr },
        { "marginRight", AttributeKind::NUMBER, setMarginRightAttr, nullptr },
        { "marginBottom", AttributeKind::NUMBER, setMarginBottomAttr, nullptr },
        { "marginLeft", AttributeKind::NUMBER, setMarginLeftAttr, nullptr },
        { "grow", AttributeKind::NUMBER, setGrowAttr, nullptr },
        { "shrink", AttributeKind::NUMBER, setShrinkAttr, nullptr },
        { "alignSelf", AttributeKind::ENUM, setAlignSelfAttr, "auto|flexStart|center|flexEnd|stretch|baseline" },
        // positionType 与 position 是同义键（C++ 侧方法名叫 setPositionType）；
        // 四边偏移同理：positionTop 与 top 等价。两套写法都登记，别只留一套。
        { "position", AttributeKind::ENUM, setPositionTypeAttr, "relative|absolute" },
        { "positionType", AttributeKind::ENUM, setPositionTypeAttr, "relative|absolute" },
        { "top", AttributeKind::SIZE, setPositionTopAttr, nullptr },
        { "left", AttributeKind::SIZE, setPositionLeftAttr, nullptr },
        { "right", AttributeKind::SIZE, setPositionRightAttr, nullptr },
        { "bottom", AttributeKind::SIZE, setPositionBottomAttr, nullptr },
        { "positionTop", AttributeKind::SIZE, setPositionTopAttr, nullptr },
        { "positionRight", AttributeKind::SIZE, setPositionRightAttr, nullptr },
        { "positionBottom", AttributeKind::SIZE, setPositionBottomAttr, nullptr },
        { "positionLeft", AttributeKind::SIZE, setPositionLeftAttr, nullptr },
        { "visibility", AttributeKind::ENUM, setVisibilityAttr, "visible|invisible|gone" },
        { "backgroundColor", AttributeKind::COLOR, setBackgroundColorAttr, nullptr },
        { "borderColor", AttributeKind::COLOR, setBorderColorAttr, nullptr },
        { "borderThickness", AttributeKind::NUMBER, setBorderThicknessAttr, nullptr },
        { "cornerRadius", AttributeKind::NUMBER, setCornerRadiusAttr, nullptr },
        { "alpha", AttributeKind::NUMBER, setAlphaAttr, nullptr },
        { "clipsToBounds", AttributeKind::BOOL, setClipsToBoundsAttr, nullptr },
    };
    return table;
}

} // namespace

void View::markMeasureDirtyRecursive()
{
    this->markMeasureDirty();

    if (Box* box = this->asBox())
        for (View* child : box->getChildren())
            child->markMeasureDirtyRecursive();
}

void View::applyThemeRecursive()
{
    this->applyTheme();

    if (Box* box = this->asBox())
        for (View* child : box->getChildren())
            child->applyThemeRecursive();
}

const AttributeTable& View::attributes() const
{
    static const AttributeTable empty = {};
    return empty;
}

ApplyResult View::applyAttribute(const std::string& name, const Value& value, std::string& error)
{
    // 子类表优先：子类可以覆盖公共属性的语义
    const ApplyResult own = applyFromTable(this, this->attributes(), name, value, error);
    if (own != ApplyResult::NOT_HANDLED)
        return own;

    return applyFromTable(this, commonAttributes(), name, value, error);
}

} // namespace bklv

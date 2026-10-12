#include "bklv/views/scrolling_frame.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"

namespace bklv
{

// 惯性衰减系数（每秒），越大停得越快。
static constexpr float SCROLL_FRICTION = 6.0f;
// 低于这个速度就停止惯性，避免无限小抖动。
static constexpr float MIN_VELOCITY = 20.0f;
// 自动滚动到焦点的速度（像素/秒）。
static constexpr float AUTO_SCROLL_SPEED = 1400.0f;
// 焦点距离可视区边缘小于这个值就开始滚动。
static constexpr float FOCUS_MARGIN = 12.0f;

ScrollingFrame::ScrollingFrame()
    : Box(Axis::COLUMN)
{
    // 这是滚动容器的核心：裁剪掉超出自身的部分。
    this->setClipsToBounds(true);
    // 滚动容器必须能被压缩：内容比可用空间高时，该由它吸收溢出并滚动，
    // 而不是把父容器/后面的兄弟顶开（Yoga 的 flexShrink 默认是 0，和 CSS 不同）。
    // 踩过：内容区里的滚动器随页面内容变高（499~519），把底栏区域顶出去。
    this->setShrink(1.0f);
    // 基准尺寸设 0：滚动器只按"剩余空间"定高，不拿内容高度当基准。
    // 否则内容一大，Yoga 会按比例压缩同一列里的其它节点（按钮行被压矮就是这么来的）。
    this->setFlexBasis(0.0f);
    // 内边距设置
    this->setPadding(5.0f, 15.0f, 5.0f, 15.0f);
}

ScrollingFrame::~ScrollingFrame()
{
    // 内容视图不是本类 new 出来的（由调用方创建），所以这里只清指针，
    // 真正的释放交给 Box 的析构（它会 clearViews）。
    this->contentView = nullptr;
}

View* ScrollingFrame::evictContentView()
{
    if (this->contentView == nullptr)
        return nullptr;

    View* view = this->contentView;
    this->contentView = nullptr;

    // 只从树上摘下来，不销毁。
    if (view->getParent() == this)
        this->removeView(view, false);
    else
        view->setParent(nullptr);

    this->contentOffsetY       = 0.0f;
    this->velocity             = 0.0f;
    this->lastAutoScrolledView = nullptr;

    return view;
}

void ScrollingFrame::setContentView(View* view)
{
    if (this->contentView == view)
        return;

    // 旧内容只"摘下"，不销毁 —— ScrollingFrame 不拥有内容视图的所有权。
    // 用 clearViews() 会 delete 旧内容，而调用方（比如切页逻辑）可能还留着它的指针，
    // 之后再用就是 use-after-free（这个坑实测踩过：切回旧页时崩在野指针上）。
    this->evictContentView();
    this->contentView = view;

    if (this->contentView == nullptr)
        return;

    // 内容不参与 Yoga 的高度分配：它自己有多高就画多高，超出部分靠 translation 移动。
    this->contentView->setWidthPercentage(100.0f);
    this->contentView->setShrink(0.0f);
    this->addView(this->contentView);

    this->contentOffsetY = 0.0f;
    this->velocity       = 0.0f;
    this->lastAutoScrolledView = nullptr;
}

// ---------------------------------------------------------------------------
// 内容尺寸
// ---------------------------------------------------------------------------

void ScrollingFrame::onContentSizeChanged()
{
    if (this->contentView == nullptr)
        return;

    // 如果调用方已经显式设过高度，就不覆盖它。
    const float explicitHeight = this->contentView->getHeight();
    if (explicitHeight > 0.0f)
        return;

    // 否则自己按子节点累加算一个高度：这对"少量子节点"的场景够用，
    // 大量数据的列表请显式设置高度（RecyclerFrame 就是这么做的）。
    float total = 0.0f;
    if (Box* content = this->contentView->asBox())
    {
        const size_t count = content->getChildrenCount();
        for (size_t i = 0; i < count; ++i)
        {
            View* child = content->getChildren()[i];
            total += child->getHeight();
            if (i + 1 < count)
            {
                // 这里拿不到 Yoga 算出的 gap，用 margin 近似
                total += child->getMarginBottom() + child->getMarginTop();
            }
        }
    }

    if (total > 0.0f)
        this->contentView->setHeight(total);
}

void ScrollingFrame::onLayout()
{
    Box::onLayout();
    this->onContentSizeChanged();
}

// ---------------------------------------------------------------------------
// 偏移
// ---------------------------------------------------------------------------

float ScrollingFrame::getScrollRange() const
{
    if (this->contentView == nullptr)
        return 0.0f;

    const float visible = this->getHeight();
    const float content = this->contentView->getHeight();

    return std::max(0.0f, content - visible);
}

void ScrollingFrame::setContentOffsetY(float offset, bool animated)
{
    const float range = this->getScrollRange();
    const float target = std::clamp(offset, 0.0f, range);

    if (!animated)
    {
        this->contentOffsetY = target;
        this->animating      = false;
        if (this->contentView != nullptr)
            this->contentView->setTranslationY(-target);
        return;
    }

    this->animating   = true;
    this->animTarget  = target;
}

// ---------------------------------------------------------------------------
// 每帧推进
// ---------------------------------------------------------------------------

void ScrollingFrame::update()
{
    if (this->contentView == nullptr)
    {
        Box::update();
        return;
    }

    // 先更新子节点（它们可能自己也有动画）。
    Box::update();

    const ImGuiIO& io = ImGui::GetIO();
    const float deltaTime = std::clamp(io.DeltaTime, 0.0001f, 0.1f);

    const float range = this->getScrollRange();

    // ---- 1. 缓动到目标（滚到焦点） ----
    if (this->animating)
    {
        const float diff = this->animTarget - this->contentOffsetY;
        if (std::fabs(diff) < 1.0f)
        {
            this->contentOffsetY = this->animTarget;
            this->animating      = false;
        }
        else
        {
            const float step = std::min(std::fabs(diff), AUTO_SCROLL_SPEED * deltaTime);
            this->contentOffsetY += (diff > 0.0f ? step : -step);
        }
    }

    // ---- 2. 鼠标滚轮 ----
    if (this->scrollingEnabled && std::fabs(io.MouseWheel) > 0.0f)
    {
        const Rect frame = this->getFrame();
        if (io.MousePos.x >= frame.x && io.MousePos.x <= frame.getMaxX() && io.MousePos.y >= frame.y
            && io.MousePos.y <= frame.getMaxY())
        {
            this->contentOffsetY -= io.MouseWheel * 60.0f;
            this->animating = false;
            this->velocity  = 0.0f;
        }
    }

    // ---- 3. 拖拽 ----
    if (this->scrollingEnabled)
    {
        const bool mouseDown = io.MouseDown[0];

        if (mouseDown && !this->dragging)
        {
            const Rect frame = this->getFrame();
            const bool inside = io.MousePos.x >= frame.x && io.MousePos.x <= frame.getMaxX()
                && io.MousePos.y >= frame.y && io.MousePos.y <= frame.getMaxY();

            if (inside && range > 0.0f)
            {
                this->dragging        = true;
                this->dragStartY      = io.MousePos.y;
                this->dragStartOffset = this->contentOffsetY;
                this->animating       = false;
                this->velocity        = 0.0f;
            }
        }
        else if (mouseDown && this->dragging)
        {
            // 只认纵向拖动，鼠标左右移动不算滚动。
            this->contentOffsetY = this->dragStartOffset - (io.MousePos.y - this->dragStartY);

            // 用帧间位移估算松手速度，供惯性使用。
            const float instantaneous = -io.MouseDelta.y / deltaTime;
            this->velocity = this->velocity * 0.7f + instantaneous * 0.3f;
        }
        else if (!mouseDown && this->dragging)
        {
            this->dragging = false;
            if (!this->inertiaEnabled)
                this->velocity = 0.0f;
        }
    }

    // ---- 4. 惯性 ----
    if (!this->dragging && !this->animating && std::fabs(this->velocity) > 0.0f)
    {
        this->contentOffsetY += this->velocity * deltaTime;

        if (!this->inertiaEnabled)
            this->velocity = 0.0f;
        else
        {
            // 指数衰减，跟帧率无关。
            this->velocity *= std::exp(-SCROLL_FRICTION * deltaTime);
            if (std::fabs(this->velocity) < MIN_VELOCITY)
                this->velocity = 0.0f;
        }
    }

    // ---- 5. 钳位；撞到边界时把速度清掉，避免贴边抖动 ----
    const float clamped = std::clamp(this->contentOffsetY, 0.0f, range);
    if (std::fabs(clamped - this->contentOffsetY) > 0.01f)
        this->velocity = 0.0f;
    this->contentOffsetY = clamped;

    // ---- 6. 焦点跟随：只在焦点真的换了的时候滚一次 ----
    View* focus = getInputManager().getCurrentFocus();
    if (focus != nullptr && focus != this->lastAutoScrolledView && !this->dragging)
    {
        // 只处理在自己子树里的焦点。
        bool inside = false;
        for (View* v = focus; v != nullptr; v = v->getParent())
        {
            if (v == this)
            {
                inside = true;
                break;
            }
        }

        if (inside && range > 0.0f)
        {
            this->lastAutoScrolledView = focus;
            this->scrollToView(focus, true);
        }
    }

    // ---- 7. 应用偏移 ----
    this->contentView->setTranslationY(-this->contentOffsetY);
}

// ---------------------------------------------------------------------------
// 滚到某个视图
// ---------------------------------------------------------------------------

void ScrollingFrame::scrollToView(View* view, bool animated)
{
    if (view == nullptr || this->contentView == nullptr)
        return;

    const float frameTop    = this->getY();
    const float frameHeight = this->getHeight();
    if (frameHeight <= 0.0f)
        return;

    // getY() 已经包含 contentView 的 translation，所以这里算的是"当前屏幕位置"。
    const float viewTop    = view->getY();
    const float viewBottom = viewTop + view->getHeight();

    float target = this->contentOffsetY;

    if (this->behavior == ScrollingBehavior::CENTER)
    {
        const float viewCenter    = (viewTop + viewBottom) * 0.5f;
        const float frameCenter   = frameTop + frameHeight * 0.5f;
        target += viewCenter - frameCenter;
    }
    else
    {
        // FOCUS：只在跑出可视区时才滚，滚到刚好贴边。
        if (viewTop < frameTop + FOCUS_MARGIN)
            target += viewTop - (frameTop + FOCUS_MARGIN);
        else if (viewBottom > frameTop + frameHeight - FOCUS_MARGIN)
            target += viewBottom - (frameTop + frameHeight - FOCUS_MARGIN);
    }

    this->setContentOffsetY(target, animated);
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

void ScrollingFrame::draw(ImDrawList* drawList, float x, float y, float width, float height, FrameContext*)
{
    if (!this->showIndicator || this->contentView == nullptr || width <= 0.0f || height <= 0.0f)
        return;

    const float range = this->getScrollRange();
    if (range <= 0.0f)
        return;

    const float contentHeight = this->contentView->getHeight();
    if (contentHeight <= 0.0f)
        return;

    // 滑块长度按可视区占内容高度的比例
    const float barWidth    = 4.0f;
    const float thumbHeight = std::max(24.0f, height * (height / contentHeight));
    const float trackHeight = height - thumbHeight;
    const float progress    = range > 0.0f ? (this->contentOffsetY / range) : 0.0f;
    const float thumbY      = y + trackHeight * progress;

    const float barX = x + width - barWidth - 4.0f;

    drawList->AddRectFilled(ImVec2(barX, y), ImVec2(barX + barWidth, y + height), IM_COL32(255, 255, 255, 18),
        barWidth * 0.5f);
    drawList->AddRectFilled(ImVec2(barX, thumbY), ImVec2(barX + barWidth, thumbY + thumbHeight),
        IM_COL32(200, 210, 225, 120), barWidth * 0.5f);
}

} // namespace bklv

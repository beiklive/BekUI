#include "bklv/views/h_scrolling_frame.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/core/input.hpp"

namespace bklv
{

static constexpr float SCROLL_FRICTION   = 6.0f;
static constexpr float MIN_VELOCITY      = 20.0f;
static constexpr float AUTO_SCROLL_SPEED = 1400.0f;
static constexpr float FOCUS_MARGIN      = 12.0f;

HScrollingFrame::HScrollingFrame()
    : Box(Axis::ROW)
{
    this->setClipsToBounds(true);
}

HScrollingFrame::~HScrollingFrame()
{
    this->contentView = nullptr;
}

void HScrollingFrame::setContentView(View* view)
{
    if (this->contentView == view)
        return;

    this->clearViews();
    this->contentView = view;

    if (this->contentView == nullptr)
        return;

    // 横向排列的内容：高度撑满容器，宽度由自己决定。
    this->contentView->setHeightPercentage(100.0f);
    this->contentView->setShrink(0.0f);
    this->addView(this->contentView);

    this->contentOffsetX       = 0.0f;
    this->velocity             = 0.0f;
    this->lastAutoScrolledView = nullptr;
}

float HScrollingFrame::getScrollRange() const
{
    if (this->contentView == nullptr)
        return 0.0f;

    return std::max(0.0f, this->contentView->getWidth() - this->getWidth());
}

void HScrollingFrame::setContentOffsetX(float offset, bool animated)
{
    const float target = std::clamp(offset, 0.0f, this->getScrollRange());

    if (!animated)
    {
        this->contentOffsetX = target;
        this->animating      = false;
        if (this->contentView != nullptr)
            this->contentView->setTranslationX(-target);
        return;
    }

    this->animating  = true;
    this->animTarget = target;
}

void HScrollingFrame::update()
{
    Box::update();

    if (this->contentView == nullptr)
        return;

    const ImGuiIO& io     = ImGui::GetIO();
    const float deltaTime = std::clamp(io.DeltaTime, 0.0001f, 0.1f);
    const float range     = this->getScrollRange();

    if (this->animating)
    {
        const float diff = this->animTarget - this->contentOffsetX;
        if (std::fabs(diff) < 1.0f)
        {
            this->contentOffsetX = this->animTarget;
            this->animating      = false;
        }
        else
        {
            const float step = std::min(std::fabs(diff), AUTO_SCROLL_SPEED * deltaTime);
            this->contentOffsetX += (diff > 0.0f ? step : -step);
        }
    }

    // 横向用 shift + 滚轮，避免和纵向滚动抢输入。
    if (this->scrollingEnabled && std::fabs(io.MouseWheel) > 0.0f && io.KeyShift)
    {
        const Rect frame = this->getFrame();
        if (io.MousePos.x >= frame.x && io.MousePos.x <= frame.getMaxX() && io.MousePos.y >= frame.y
            && io.MousePos.y <= frame.getMaxY())
        {
            this->contentOffsetX -= io.MouseWheel * 60.0f;
            this->animating = false;
            this->velocity  = 0.0f;
        }
    }

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
                this->dragStartX      = io.MousePos.x;
                this->dragStartOffset = this->contentOffsetX;
                this->animating       = false;
                this->velocity        = 0.0f;
            }
        }
        else if (mouseDown && this->dragging)
        {
            this->contentOffsetX = this->dragStartOffset - (io.MousePos.x - this->dragStartX);

            const float instantaneous = -io.MouseDelta.x / deltaTime;
            this->velocity            = this->velocity * 0.7f + instantaneous * 0.3f;
        }
        else if (!mouseDown && this->dragging)
        {
            this->dragging = false;
            if (!this->inertiaEnabled)
                this->velocity = 0.0f;
        }
    }

    if (!this->dragging && !this->animating && std::fabs(this->velocity) > 0.0f)
    {
        this->contentOffsetX += this->velocity * deltaTime;

        if (!this->inertiaEnabled)
            this->velocity = 0.0f;
        else
        {
            this->velocity *= std::exp(-SCROLL_FRICTION * deltaTime);
            if (std::fabs(this->velocity) < MIN_VELOCITY)
                this->velocity = 0.0f;
        }
    }

    const float clamped = std::clamp(this->contentOffsetX, 0.0f, range);
    if (std::fabs(clamped - this->contentOffsetX) > 0.01f)
        this->velocity = 0.0f;
    this->contentOffsetX = clamped;

    // 焦点跟随：只在焦点变化时滚一次
    View* focus = getInputManager().getCurrentFocus();
    if (focus != nullptr && focus != this->lastAutoScrolledView && !this->dragging)
    {
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

    this->contentView->setTranslationX(-this->contentOffsetX);
}

void HScrollingFrame::scrollToView(View* view, bool animated)
{
    if (view == nullptr || this->contentView == nullptr)
        return;

    const float frameLeft   = this->getX();
    const float frameWidth  = this->getWidth();
    if (frameWidth <= 0.0f)
        return;

    const float viewLeft  = view->getX();
    const float viewRight = viewLeft + view->getWidth();

    float target = this->contentOffsetX;

    if (viewLeft < frameLeft + FOCUS_MARGIN)
        target += viewLeft - (frameLeft + FOCUS_MARGIN);
    else if (viewRight > frameLeft + frameWidth - FOCUS_MARGIN)
        target += viewRight - (frameLeft + frameWidth - FOCUS_MARGIN);

    this->setContentOffsetX(target, animated);
}

void HScrollingFrame::draw(ImDrawList* drawList, float x, float y, float width, float height, Style,
    FrameContext*)
{
    if (!this->showIndicator || this->contentView == nullptr)
        return;

    const float range = this->getScrollRange();
    const float contentWidth = this->contentView->getWidth();
    if (range <= 0.0f || contentWidth <= 0.0f || width <= 0.0f)
        return;

    const float barHeight   = 4.0f;
    const float thumbWidth  = std::max(24.0f, width * (width / contentWidth));
    const float trackWidth  = width - thumbWidth;
    const float progress    = range > 0.0f ? (this->contentOffsetX / range) : 0.0f;
    const float thumbX      = x + trackWidth * progress;
    const float barY        = y + height - barHeight - 4.0f;

    drawList->AddRectFilled(ImVec2(x, barY), ImVec2(x + width, barY + barHeight), IM_COL32(255, 255, 255, 18),
        barHeight * 0.5f);
    drawList->AddRectFilled(ImVec2(thumbX, barY), ImVec2(thumbX + thumbWidth, barY + barHeight),
        IM_COL32(200, 210, 225, 120), barHeight * 0.5f);
}

} // namespace bklv

#include "bklv/views/scroll_box.hpp"

#include "bklv/views/h_scrolling_frame.hpp"
#include "bklv/views/scrolling_frame.hpp"

using namespace bklv;

ScrollBox::ScrollBox(Axis direction)
    : direction(direction)
{
    this->content = new Box(direction);
    if (direction == Axis::COLUMN)
        this->content->setWidthPercentage(100.0f); // 纵向滚动：内容跟着视口宽
    else
        this->content->setHeightPercentage(100.0f); // 横向滚动：内容跟着视口高

    if (direction == Axis::COLUMN)
    {
        ScrollingFrame* scroller = new ScrollingFrame();
        scroller->setContentView(this->content);
        this->frame = scroller;
    }
    else
    {
        HScrollingFrame* scroller = new HScrollingFrame();
        scroller->setContentView(this->content);
        this->frame = scroller;
    }

    this->frame->setGrow(1.0f);
    this->frame->setFlexBasis(0.0f);
    this->frame->setShrink(1.0f);
    Box::addView(this->frame);
}

ScrollBox::~ScrollBox()
{
    // 滚动器由本类持有；content 的所有权在 setContentView 时交给滚动器
    if (this->frame != nullptr)
        Box::removeView(this->frame, true);
    this->frame   = nullptr;
    this->content = nullptr;
}

void ScrollBox::addView(View* view)
{
    if (this->content != nullptr)
        this->content->addView(view);
    else
        Box::addView(view);
}

float ScrollBox::getScrollOffset() const
{
    if (this->direction == Axis::COLUMN)
        return static_cast<ScrollingFrame*>(this->frame)->getContentOffsetY();
    return static_cast<HScrollingFrame*>(this->frame)->getContentOffsetX();
}

float ScrollBox::getScrollRange() const
{
    if (this->direction == Axis::COLUMN)
        return static_cast<ScrollingFrame*>(this->frame)->getScrollRange();
    return static_cast<HScrollingFrame*>(this->frame)->getScrollRange();
}

void ScrollBox::setScrollOffset(float offset, bool animated)
{
    if (this->direction == Axis::COLUMN)
        static_cast<ScrollingFrame*>(this->frame)->setContentOffsetY(offset, animated);
    else
        static_cast<HScrollingFrame*>(this->frame)->setContentOffsetX(offset, animated);
}

void ScrollBox::setShowScrollingIndicator(bool show)
{
    if (this->direction == Axis::COLUMN)
        static_cast<ScrollingFrame*>(this->frame)->setShowScrollingIndicator(show);
    else
        static_cast<HScrollingFrame*>(this->frame)->setShowScrollingIndicator(show);
}

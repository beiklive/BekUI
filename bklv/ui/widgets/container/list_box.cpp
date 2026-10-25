#include "bklv/ui/widgets/container/list_box.hpp"

#include "bklv/ui/theme/theme_data.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/container/scroll_box.hpp"

using namespace bklv;

ListBox::ListBox(const std::string& variant)
    : AppearanceView(variant.empty() ? "base/box/list" : "base/box/list." + variant, false)
{
    this->scroller = new ScrollBox(Axis::COLUMN);
    this->scroller->setGrow(1.0f);
    this->scroller->setFlexBasis(0.0f);
    this->scroller->setShrink(1.0f);
    this->scroller->setShowScrollingIndicator(true);
    // 宽度跟着 ListBox 走：滚动器如果宽度是 auto，内容盒会按"最宽的一项"撑开，
    // 列表就会横向溢出容器（踩过）。这里把它钉成 100%，内容盒再跟 100%。
    this->scroller->setWidthPercentage(100.0f);
    this->addView(this->scroller);
    this->content = this->scroller->getContentBox();
    if (this->content != nullptr)
        this->content->setWidthPercentage(100.0f);

    // 空态提示：不进滚动内容（否则它自己也能被滚走），直接叠在列表上。
    this->empty = new Label();
    this->empty->setSingleLine(false);
    this->empty->setVisibility(Visibility::GONE);
    this->addView(this->empty);

    this->applyTheme();
    this->refreshEmptyVisibility();
}

ListBox::~ListBox() = default;

void ListBox::applyTheme()
{
    AppearanceView::applyTheme();

    const ThemeData& theme = getTheme();

    if (this->content != nullptr)
        this->content->setGap(theme.number("metric.list.gap", 10.0f));

    if (this->empty != nullptr)
    {
        this->empty->setFontSize(theme.number("metric.list.emptySize", 18.0f));
        this->empty->setTextColor(theme.color("color.text.dim", IM_COL32(140, 145, 152, 255)));
        this->empty->setHorizontalAlign(HorizontalAlign::CENTER);
    }
}

void ListBox::addItem(View* view)
{
    if (view == nullptr)
        return;
    this->content->addView(view);
    this->refreshEmptyVisibility();
    this->invalidate();
}

Label* ListBox::addItemText(const std::string& text)
{
    auto* label = new Label();
    label->setText(text);
    label->setSingleLine(false);
    this->content->addView(label);
    this->refreshEmptyVisibility();
    this->invalidate();
    return label;
}

void ListBox::clearItems()
{
    if (this->content != nullptr)
        this->content->clearViews();
    this->refreshEmptyVisibility();
    this->invalidate();
}

size_t ListBox::getItemCount() const
{
    if (this->content == nullptr)
        return 0;
    return this->content->getChildren().size();
}

void ListBox::setEmptyText(const std::string& text)
{
    this->emptyText = text;
    if (this->empty != nullptr)
        this->empty->setText(text);
    this->refreshEmptyVisibility();
    this->invalidate();
}

void ListBox::refreshEmptyVisibility()
{
    if (this->empty == nullptr)
        return;

    const bool show = this->getItemCount() == 0 && !this->emptyText.empty();
    this->empty->setVisibility(show ? Visibility::VISIBLE : Visibility::GONE);
}

void ListBox::scrollToTop(bool animated)
{
    if (this->scroller != nullptr)
        this->scroller->setScrollOffset(0.0f, animated);
}

void ListBox::scrollToItem(View* item, bool animated)
{
    if (this->scroller != nullptr && item != nullptr)
        this->scroller->scrollToView(item, animated);
}

void ListBox::setShowScrollbar(bool show)
{
    if (this->scroller != nullptr)
        this->scroller->setShowScrollingIndicator(show);
}

float ListBox::getScrollOffset() const
{
    return this->scroller != nullptr ? this->scroller->getScrollOffset() : 0.0f;
}

float ListBox::getScrollRange() const
{
    return this->scroller != nullptr ? this->scroller->getScrollRange() : 0.0f;
}

void ListBox::update()
{
    AppearanceView::update();
}

void ListBox::onChildFocusGained(View* directChild, View* focusedView)
{
    AppearanceView::onChildFocusGained(directChild, focusedView);
    if (focusedView != nullptr)
        this->scrollToItem(focusedView, true);
}

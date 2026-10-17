#include "bklv/widgets/tab_bar.hpp"

#include <utility>

#include "bklv/core/attributes.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

TabBar::TabBar()
    : AppearanceView("widget/tab_bar", false)
{
    // 条里的条目横排、横向可滚动：页数多的时候（demo 有 15 个）也能滚到
    this->itemScroller = new HScrollingFrame();
    this->itemScroller->setWidthPercentage(100.0f);
    this->itemScroller->setGrow(1.0f);
    this->itemScroller->setShowScrollingIndicator(false);
    this->addView(this->itemScroller);

    this->itemBox = new Box(Axis::ROW);
    this->itemBox->setHeightPercentage(100.0f);
    this->itemBox->setAlignItems(AlignItems::CENTER);
    this->itemBox->setGap(getTheme().number("metric.spacing.xs", 4.0f));
    this->itemScroller->setContentView(this->itemBox);

    this->applyTheme();
}

void TabBar::addItem(const std::string& itemTitle, std::function<void()> onSelect,
    std::function<void()> onFocus)
{
    const size_t index = this->cells.size();

    TabCell* cell = new TabCell();
    cell->setTitle(itemTitle);

    // 焦点只是移过去（iOS 的底部 tab 是"点一下才切"）
    cell->setOnFocus([onFocus] {
        if (onFocus)
            onFocus();
    });

    // A / 点击才切页
    cell->setOnSelect([this, index, onSelect] {
        this->setSelectedIndex(index);
        if (onSelect)
            onSelect();
    });

    this->itemBox->addView(cell);
    this->cells.push_back(cell);

    if (index == this->selected)
        cell->setSelected(true);
}

void TabBar::clearItems()
{
    for (TabCell* cell : this->cells)
        this->itemBox->removeView(cell, true);
    this->cells.clear();
    this->selected = 0;
}

void TabBar::setSelectedIndex(size_t index)
{
    if (index >= this->cells.size())
        return;

    this->selected = index;
    for (size_t i = 0; i < this->cells.size(); i++)
        this->cells[i]->setSelected(i == index);
}

View* TabBar::getItemView(size_t index) const
{
    return index < this->cells.size() ? this->cells[index] : nullptr;
}

void TabBar::applyTheme()
{
    AppearanceView::applyTheme();

    if (this->itemBox != nullptr)
        this->itemBox->setGap(getTheme().number("metric.spacing.xs", 4.0f));
}

} // namespace bklv

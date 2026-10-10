#include "app/tab_bar.hpp"

#include "app/page_helpers.hpp"
#include "bklv/core/input.hpp"

using namespace bklv; // 实现文件里用一次

namespace demo
{

// ---------------------------------------------------------------------------
// TabCell
// ---------------------------------------------------------------------------

TabCell::TabCell()
    : RecyclerCell(false) // false：走普通流式布局，不做绝对定位
{
    this->setHeight(38.0f);
    this->setAlignItems(AlignItems::CENTER);
    this->setJustifyContent(JustifyContent::CENTER);
    this->setCornerRadius(8.0f);
    this->setPadding(0.0f, 18.0f, 0.0f, 18.0f);

    this->label = makeText("", 15.0f, rgb(170, 180, 195), true);
    this->label->setHorizontalAlign(HorizontalAlign::CENTER);
    this->addView(this->label);

    // tab 也是"点一下就切"更自然
    this->onPointerAction = true;
}

void TabCell::setTab(const std::string& name, bool selected)
{
    this->label->setText(name);
    this->setSelected(selected);
}

void TabCell::setSelected(bool selected)
{
    this->selected = selected;
    this->setBackgroundColor(selected ? rgb(48, 86, 140) : rgb(45, 49, 56));
    this->label->setTextColor(selected ? rgb(255, 255, 255) : rgb(170, 180, 195));
}

void TabCell::onFocusGained()
{
    this->setBorderColor(rgb(120, 190, 255));
    this->setBorderThickness(2.0f);
}

void TabCell::onFocusLost()
{
    this->setBorderThickness(0.0f);
}

// ---------------------------------------------------------------------------
// TabBar
// ---------------------------------------------------------------------------

TabBar::TabBar()
{
    this->setHeight(46.0f);
    this->setWidthPercentage(100.0f);
    this->setShowScrollingIndicator(false);

    this->content = new Box(Axis::ROW);
    this->content->setHeight(46.0f);
    this->content->setGap(8.0f);
    this->setContentView(this->content);
}

void TabBar::addTab(const std::string& name, const std::function<void()>& onSelect,
    const std::function<void()>& onFocus)
{
    TabCell* cell = new TabCell();
    cell->setTab(name, false);

    // 确认键与返回键都当作"切到这一页"
    cell->registerAction(ControllerButton::A, "切换", [onSelect](View*) { onSelect(); });
    cell->registerAction(ControllerButton::B, "选中", [onSelect](View*) { onSelect(); });

    this->cells.push_back(cell);
    this->focusHooks.push_back(onFocus);
    this->content->addView(cell);
}

void TabBar::setSelectedIndex(size_t index)
{
    for (size_t i = 0; i < this->cells.size(); ++i)
        this->cells[i]->setSelected(i == index);
}

void TabBar::onLayout()
{
    HScrollingFrame::onLayout();

    float total = 0.0f;
    for (TabCell* cell : this->cells)
        total += cell->getWidth();

    if (this->cells.size() > 1)
        total += 8.0f * static_cast<float>(this->cells.size() - 1);

    if (total > 0.0f)
        this->content->setWidth(total);
}

void TabBar::update()
{
    HScrollingFrame::update();

    // 焦点落到某个 tab 上就选中它
    View* focus = getInputManager().getCurrentFocus();
    for (size_t i = 0; i < this->cells.size(); ++i)
    {
        if (this->cells[i] == focus && i != this->lastFocusedIndex)
        {
            this->lastFocusedIndex = i;
            this->focusHooks[i]();
        }
    }
}

} // namespace demo

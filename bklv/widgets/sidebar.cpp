#include "bklv/widgets/sidebar.hpp"

#include <utility>

#include "bklv/core/attributes.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

Sidebar::Sidebar(const std::string& title)
    : Box(Axis::COLUMN)
    , title(title)
{
    // 面板样式（宽/不压缩/内边距/间距/底色/圆角）全在 widgets/sidebar.json 的 box 段里。
    // 高度写的是 auto：父容器会 stretch 它，写 100% 而父高度又由内容决定时会形成死循环
    // （踩过：底栏被挤出可视区）。

    this->headerBox = new Box(Axis::ROW);
    this->headerBox->setWidthPercentage(100.0f);
    this->headerBox->setHeight(AUTO);
    this->headerBox->setPadding(
        getTheme().number("metric.spacing.sm", 8.0f), getTheme().number("metric.spacing.sm", 8.0f),
        getTheme().number("metric.spacing.sm", 8.0f), getTheme().number("metric.spacing.sm", 8.0f));
    this->addView(this->headerBox);

    // 标题用 Label（语义控件），颜色跟随主题
    this->titleLabel = new Label();
    this->titleLabel->setText(title);
    this->titleLabel->setFontSizeExplicit(false);
    this->titleLabel->setTextColorExplicit(false);
    this->headerBox->addView(this->titleLabel);

    // 条目放进可滚动容器：条目高度是 borealis 的 70，页数多了会超出可视区，
    // 滚动条会跟着焦点走（ScrollingFrame 自带"焦点必须在可视区内"）
    this->itemScroller = new ScrollingFrame();
    this->itemScroller->setWidthPercentage(100.0f);
    this->itemScroller->setGrow(1.0f);
    this->itemScroller->setShowScrollingIndicator(true);
    this->itemScroller->setScrollingBehavior(ScrollingBehavior::FOCUS);
    this->addView(this->itemScroller);

    this->itemBox = new Box(Axis::COLUMN);
    this->itemBox->setWidthPercentage(100.0f);
    this->itemBox->setHeight(AUTO);
    this->itemBox->setGap(getTheme().number("metric.spacing.xs", 4.0f));
    this->itemScroller->setContentView(this->itemBox);

    this->applyPanelLook();
}

void Sidebar::applyPanelLook()
{
    // 换肤/改缩放时重新套用（值都在 widgets/sidebar.json 的 box 段里）
    applyWidgetBox(this, "sidebar");
}

void Sidebar::setPanelWidth(float width)
{
    this->setWidth(width);
}

void Sidebar::addItem(const std::string& itemTitle, std::function<void()> onSelect)
{
    const size_t index = this->cells.size();

    TabCell* cell = new TabCell();
    cell->setTitle(itemTitle);
    // 侧栏条目比 tab 格子高（borealis 是 70）：走 tab_cell 的 "sidebar" 变体
    cell->setAppearanceVariant("sidebar");
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

void Sidebar::clearItems()
{
    for (TabCell* cell : this->cells)
        this->itemBox->removeView(cell, true);
    this->cells.clear();
    this->selected = 0;
}

void Sidebar::setTitle(const std::string& value)
{
    this->title = value;
    if (this->titleLabel != nullptr)
        this->titleLabel->setText(value);
}

void Sidebar::setItemTitle(size_t index, const std::string& itemTitle)
{
    if (index >= this->cells.size())
        return;
    this->cells[index]->setTitle(itemTitle);
}

std::string Sidebar::getItemTitle(size_t index) const
{
    if (index >= this->cells.size())
        return {};
    return this->cells[index]->getText();
}

void Sidebar::setSelectedIndex(size_t index)
{
    if (index >= this->cells.size())
        return;

    this->selected = index;
    for (size_t i = 0; i < this->cells.size(); i++)
        this->cells[i]->setSelected(i == index);
}

const AttributeTable& Sidebar::attributes() const
{
    static const AttributeTable table = {
        { "title", AttributeKind::TEXT, [](View* v, const ResolvedValue& r) {
             static_cast<Sidebar*>(v)->setTitle(r.text);
         }, nullptr },
        { "panelWidth", AttributeKind::NUMBER, [](View* v, const ResolvedValue& r) {
             static_cast<Sidebar*>(v)->setPanelWidth(r.number);
         }, nullptr },
    };
    return table;
}

} // namespace bklv

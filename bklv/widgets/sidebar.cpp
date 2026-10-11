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
    this->setWidth(getTheme().number("metric.sidebar.width", 200.0f));
    this->setHeightPercentage(100.0f);
    this->setShrink(0.0f);
    this->setGap(getTheme().number("metric.spacing.xs", 4.0f));

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

    this->itemBox = new Box(Axis::COLUMN);
    this->itemBox->setWidthPercentage(100.0f);
    this->itemBox->setGrow(1.0f);
    this->itemBox->setGap(getTheme().number("metric.spacing.xs", 4.0f));
    this->addView(this->itemBox);

    this->applyPanelLook();
}

void Sidebar::applyPanelLook()
{
    this->setBackgroundColor(getTheme().color("color.surface.card"));
    this->setCornerRadius(getTheme().number("metric.radius.lg", 10.0f));
    this->setPadding(getTheme().number("metric.spacing.sm", 8.0f));
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
        { "title", AttributeKind::Text, [](View* v, const ResolvedValue& r) {
             static_cast<Sidebar*>(v)->setTitle(r.text);
         }, nullptr },
        { "panelWidth", AttributeKind::Number, [](View* v, const ResolvedValue& r) {
             static_cast<Sidebar*>(v)->setPanelWidth(r.number);
         }, nullptr },
    };
    return table;
}

} // namespace bklv

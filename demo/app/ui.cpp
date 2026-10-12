#include "app/ui.hpp"

#include "bklv/core/theme_data.hpp"

using namespace bklv;

namespace demo
{

Label* makeText(const std::string& text, float size, ImU32 color, bool singleLine)
{
    Label* label = new Label();
    label->setText(text);
    label->setFontSize(size);
    label->setTextColor(color);
    label->setSingleLine(singleLine);
    return label;
}

Label* makeHint(const std::string& text)
{
    Label* label = new Label();
    label->setText(text);
    label->setFontSizeExplicit(false);
    label->setTextColorExplicit(false);
    label->setSingleLine(false);
    label->setLineHeight(getTheme().number("metric.font.lineHeight", 1.4f));
    return label;
}

Rectangle* makeCard()
{
    Rectangle* card = new Rectangle("card");
    card->setWidthPercentage(100.0f);
    card->setHeight(AUTO);
    card->setPadding(getTheme().number("metric.card.padding", 16.0f));
    card->setGap(getTheme().number("metric.spacing.sm", 8.0f));
    return card;
}

Box* makeRow(float gap, AlignItems align)
{
    Box* row = new Box(Axis::ROW);
    row->setWidthPercentage(100.0f);
    row->setHeight(AUTO);
    row->setGap(gap);
    row->setAlignItems(align);
    return row;
}

Box* makeColumn(float gap)
{
    Box* column = new Box(Axis::COLUMN);
    column->setWidthPercentage(100.0f);
    column->setHeight(AUTO);
    column->setGap(gap);
    return column;
}

} // namespace demo

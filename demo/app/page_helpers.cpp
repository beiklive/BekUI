#include "app/page_helpers.hpp"

using namespace bklv; // 实现文件里用一次，省掉到处写 bklv::

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

Label* makeTitle(const std::string& text)
{
    Label* label = makeText(text, 22.0f, rgb(255, 255, 255));
    label->setMarginBottom(6.0f);
    return label;
}

Label* makeHint(const std::string& text)
{
    Label* label = makeText(text, 14.0f, rgb(150, 160, 175));
    label->setLineHeight(1.5f);
    label->setMarginBottom(14.0f);
    return label;
}

Box* makeCard(float padding)
{
    Box* card = new Box(Axis::COLUMN);
    card->setWidthPercentage(100.0f);
    card->setHeight(AUTO);
    card->setBackgroundColor(rgb(38, 42, 50));
    card->setCornerRadius(10.0f);
    card->setPadding(padding);
    card->setMarginBottom(14.0f);
    return card;
}

Box* makeRow(float gap, AlignItems align)
{
    Box* row = new Box(Axis::ROW);
    row->setWidthPercentage(100.0f);
    row->setAlignItems(align);
    row->setGap(gap);
    return row;
}

} // namespace demo

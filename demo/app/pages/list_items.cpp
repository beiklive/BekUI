#include "app/pages/list_items.hpp"

#include "app/page_helpers.hpp"

using namespace bklv;

namespace demo
{

// ---------------------------------------------------------------------------
// ListCell：图片 + 两行文字
// ---------------------------------------------------------------------------

ListCell::ListCell()
{
    this->setHeight(64.0f);
    this->setAlignItems(AlignItems::CENTER);
    this->setPadding(10.0f);
    this->setGap(12.0f);
    this->setCornerRadius(8.0f);
    this->setBackgroundColor(rgb(38, 42, 50));

    this->thumbnail = new Image();
    this->thumbnail->setDimensions(76.0f, 44.0f);
    this->thumbnail->setCornerRadius(6.0f);
    this->thumbnail->setScalingType(ImageScalingType::FILL);
    this->thumbnail->setImageFromFile("resources/testcard.png");
    this->addView(this->thumbnail);

    Box* textBox = new Box(Axis::COLUMN);
    textBox->setGrow(1.0f);
    textBox->setShrink(1.0f);

    this->title = makeText("", 15.0f, rgb(235, 238, 242), true);
    textBox->addView(this->title);

    this->subtitle = makeText("", 12.0f, rgb(150, 160, 175), true);
    this->subtitle->setMarginTop(3.0f);
    textBox->addView(this->subtitle);

    this->addView(textBox);
}

void ListCell::prepareForReuse()
{
    // 复用时必须清掉上一条数据的痕迹
    this->activated = false;
}

void ListCell::setIndex(size_t value)
{
    RecyclerCell::setIndex(value);
    this->title->setText("列表项 #" + std::to_string(value));
    this->subtitle->setText(this->activated ? "已触发" : "固定 64 高的行，滚出视口后会被回收复用");
}

void ListCell::onFocusGained()
{
    this->setBackgroundColor(rgb(52, 74, 110));
    this->setBorderColor(rgb(90, 170, 245));
    this->setBorderThickness(2.0f);
}

void ListCell::onFocusLost()
{
    this->setBackgroundColor(rgb(38, 42, 50));
    this->setBorderThickness(0.0f);
}

// ---------------------------------------------------------------------------
// SimpleCell：单行
// ---------------------------------------------------------------------------

SimpleCell::SimpleCell()
{
    this->setAlignItems(AlignItems::CENTER);
    this->setPadding(0.0f, 12.0f, 0.0f, 12.0f);
    this->setBackgroundColor(rgb(38, 42, 50));
    this->setCornerRadius(6.0f);

    this->label = makeText("", 14.0f, rgb(200, 208, 220), true);
    this->label->setGrow(1.0f);
    this->addView(this->label);
}

void SimpleCell::setIndex(size_t value)
{
    RecyclerCell::setIndex(value);
    this->label->setText("内嵌行 #" + std::to_string(value));
}

void SimpleCell::onFocusGained()
{
    this->setBackgroundColor(rgb(52, 74, 110));
    this->setBorderColor(rgb(90, 170, 245));
    this->setBorderThickness(2.0f);
}

void SimpleCell::onFocusLost()
{
    this->setBackgroundColor(rgb(38, 42, 50));
    this->setBorderThickness(0.0f);
}

} // namespace demo

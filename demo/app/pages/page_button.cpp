// 按钮展示页（骨架）。
//
// 现在只有**一个基础按钮**（外观 `base/button/button`），后面由你来加内容。
// 想继续加就在 page 上 addView —— 不需要动外层结构。

#include "app/pages.hpp"
#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

Box* buildButtonShowcasePage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setShrink(0.0f);
    page->setGap(getTheme().number("metric.spacing.md", 12.0f));
    // 按钮保持自然宽度（列方向默认 stretch 会把它拉满整行）
    page->setAlignItems(AlignItems::FLEX_START);

    Button* button = new Button();
    button->setAppearanceName("base/button/button");
    button->setText("基础按钮");
    page->addView(button);

    return page;
}

} // namespace demo

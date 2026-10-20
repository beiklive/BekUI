// 按钮展示页（骨架）。
//
// 现在只有**一个基础按钮**（外观 `base/button/button`），后面由你来加内容。
// 想继续加就在 page 上 addView —— 不需要动外层结构。

#include "app/pages.hpp"
#include "bklv/bklv.hpp"
#include "bklv/core/icon_font.hpp"

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
    page->addView(bklv::makeSectionHeader("基础按钮"));

    Box* button_box = new Box(Axis::ROW);
    button_box->setWidthPercentage(100.0f); // 关键：行自身要有确定宽度，grow 才有"剩余空间"可分
    button_box->setGap(getTheme().number("metric.spacing.md", 12.0f));


    Button* button = new Button();
    button->setAppearanceName("base/button/button");
    button->setText("基础按钮");
    button_box->addView(button);

    Button* border_button = new Button();
    border_button->setAppearanceName("base/button/button.bordered");
    border_button->setText("边框按钮");
    button_box->addView(border_button);

    Button* tab_button = new Button();
    tab_button->setAppearanceName("base/button/tab_button");
    tab_button->setText("列表按钮");
    button_box->addView(tab_button);

    page->addView(button_box);
    for (View* child : button_box->getChildren())
    {
        child->setGrow(1.0f);
        child->setFlexBasis(0.0f);  // 从 0 开始分（否则按文字宽度打底）
        child->setShrink(0.0f);
        child->setMinWidth(0.0f); // 外观里的 minWidth（120）会卡住 1 份的宽度
    }

    page->addView(bklv::makeSectionHeader("图标按钮"));

    Box* icon_row = new Box(Axis::ROW);
    icon_row->setWidthPercentage(100.0f);
    icon_row->setGap(getTheme().number("metric.spacing.md", 12.0f));
    icon_row->setAlignItems(AlignItems::CENTER);

    // 1) 纯图标正方形
    Button* square = new Button();
    square->setAppearanceName("base/button/button.icon");
    square->setIcon(bklv::icon::glyphByName("star"));
    icon_row->addView(square);

    // 2) 纯图标圆形
    Button* circle = new Button();
    circle->setAppearanceName("base/button/button.icon_circle");
    circle->setIcon(bklv::icon::glyphByName("favorite"));
    icon_row->addView(circle);

    // 3) 图标 + 文字：点一下换图标和文字，验证接口生效
    Button* icon_text = new Button();
    icon_text->setAppearanceName("base/button/button.icon_text");
    icon_text->setIcon(bklv::icon::glyphByName("settings"));
    icon_text->setText("图标 + 文字");
    icon_text->setWidth(220.0f);
    icon_text->setOnClick([icon_text] {
        static bool alt = false;
        alt = !alt;
        icon_text->setIcon(bklv::icon::glyphByName(alt ? "check" : "settings"));
        icon_text->setText(alt ? "已切换" : "图标 + 文字");
    });
    icon_row->addView(icon_text);

    page->addView(icon_row);

    return page;
}

} // namespace demo

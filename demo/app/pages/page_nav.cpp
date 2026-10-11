#include "app/pages/pages.hpp"

#include <string>

#include "app/page_helpers.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/sidebar.hpp"
#include "bklv/widgets/tab_frame.hpp"

using namespace bklv;

namespace demo
{

namespace
{

// 侧栏每一项对应的内容页（就是简单的几行文字，重点在导航结构）
Box* makeNavPage(const std::string& title, const std::string& body)
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);
    page->addView(makeText(title, 18.0f, getTheme().color("color.text.primary")));

    Label* text = makeText(body, 14.0f, getTheme().color("color.text.dim"));
    text->setLineHeight(1.5f);
    text->setTextColorExplicit(false);
    text->setMarginTop(8.0f);
    page->addView(text);
    return page;
}

} // namespace

Box* buildNavPage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);

    page->addView(makeTitle(getStr("tab/nav")));
    page->addView(makeHint(getStr("nav/hint")));

    // 左：侧栏；右：TabFrame（两者都是库级控件，外观来自主题）
    Box* row = new Box(Axis::ROW);
    row->setWidthPercentage(100.0f);
    row->setHeight(360.0f);
    row->setGap(getTheme().number("metric.spacing.md", 12.0f));

    Box* right = new Box(Axis::COLUMN);
    right->setGrow(1.0f);
    right->setShrink(1.0f);

    TabFrame* tabs = new TabFrame();
    tabs->addTab(getStr("nav/tab_a"), makeNavPage(getStr("nav/page_a"), getStr("nav/body_a")));
    tabs->addTab(getStr("nav/tab_b"), makeNavPage(getStr("nav/page_b"), getStr("nav/body_b")));
    tabs->addTab(getStr("nav/tab_c"), makeNavPage(getStr("nav/page_c"), getStr("nav/body_c")));
    right->addView(tabs);

    Sidebar* sidebar = new Sidebar(getStr("nav/sidebar_title"));
    sidebar->addItem(getStr("nav/item_1"), [tabs] { tabs->setSelectedIndex(0); });
    sidebar->addItem(getStr("nav/item_2"), [tabs] { tabs->setSelectedIndex(1); });
    sidebar->addItem(getStr("nav/item_3"), [tabs] { tabs->setSelectedIndex(2); });

    row->addView(sidebar);
    row->addView(right);
    page->addView(row);

    return page;
}

} // namespace demo

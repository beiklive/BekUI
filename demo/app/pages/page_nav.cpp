#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/tab_frame.hpp"

using namespace bklv;

namespace demo
{

namespace
{

Box* makeTabContent(const std::string& title, const std::string& body)
{
    Box* content = makeColumn(getTheme().number("metric.spacing.sm", 8.0f));
    content->addView(makeText(title, getTheme().number("metric.font.size.lg", 22.0f),
        getTheme().color("color.text.primary")));
    content->addView(makeHint(body));
    return content;
}

} // namespace

Box* buildNavPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeTitle(getStr("tab/nav")));
    page->addView(makeHint(getStr("nav/hint")));

    // TabFrame 自己撑满一块区域：切页时旧页面只脱离不销毁
    Box* host = new Box(Axis::COLUMN);
    host->setWidthPercentage(100.0f);
    host->setHeight(320.0f);

    TabFrame* tabs = new TabFrame();
    tabs->addTab(getStr("nav/tab_a"), makeTabContent(getStr("nav/page_a"), getStr("nav/body_a")));
    tabs->addTab(getStr("nav/tab_b"), makeTabContent(getStr("nav/page_b"), getStr("nav/body_b")));
    tabs->addTab(getStr("nav/tab_c"), makeTabContent(getStr("nav/page_c"), getStr("nav/body_c")));

    host->addView(tabs);
    page->addView(host);

    return page;
}

} // namespace demo

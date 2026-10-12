#include "app/pages.hpp"

#include <string>

#include "app/ui.hpp"
#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

Box* buildScrollPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("scroll/hint")));

    // ---- 独立滚动容器（对标 borealis 的 Scroll test：一段长文本放进 ScrollingFrame）----
    Rectangle* textCard = makeCard();
    textCard->addView(makeText(getStr("scroll/inside"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    ScrollingFrame* scroller = new ScrollingFrame();
    scroller->setHeight(getTheme().number("metric.scroll.demoHeight", 220.0f));
    scroller->setShowScrollingIndicator(true);

    Box* longContent = makeColumn(getTheme().number("metric.spacing.sm", 8.0f));
    for (int i = 0; i < 12; i++)
    {
        longContent->addView(makeText(getStr("scroll/paragraph", i + 1),
            getTheme().number("metric.font.size.sm", 14.0f),
            getTheme().color("color.text.dim")));
    }
    scroller->setContentView(longContent);
    textCard->addView(scroller);
    page->addView(textCard);

    // ---- 焦点跟随滚动（在外层滚动区里放足够多可聚焦行）----
    Rectangle* followCard = makeCard();
    followCard->addView(makeText(getStr("scroll/follow"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    Label* status = makeHint(getStr("scroll/status_none"));
    followCard->addView(status);

    Box* list = makeColumn(getTheme().number("metric.spacing.sm", 8.0f));
    for (int i = 0; i < 20; i++)
    {
        Button* row = new Button(getStr("scroll/row", i + 1));
        row->setWidthPercentage(100.0f);
        row->setOnClick([status, i] { status->setText(getStr("scroll/status", i + 1)); });
        list->addView(row);
    }
    followCard->addView(list);
    page->addView(followCard);

    return page;
}

} // namespace demo

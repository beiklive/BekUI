#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/bklv.hpp"

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

    // ---- 事件总线：焦点/动作事件实时显示（进程级总线，RAII 句柄活到进程结束）----
    Rectangle* eventCard = makeCard();
    eventCard->addView(makeText(getStr("nav/event_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    Label* lastEvent = makeHint(getStr("nav/event_none"));
    eventCard->addView(lastEvent);

    // static：订阅句柄要活过页面构建函数；进程结束自动退订
    static Subscription focusSub = getEventBus().subscribe<FocusChangedEvent>(
        [lastEvent](const FocusChangedEvent& event) {
            if (lastEvent == nullptr || event.current == nullptr)
                return;
            lastEvent->setText(getStr("nav/event_focus", event.current->getClassString()));
        });

    static Subscription actionSub = getEventBus().subscribe<ActionTriggeredEvent>(
        [lastEvent](const ActionTriggeredEvent& event) {
            if (lastEvent == nullptr || event.view == nullptr)
                return;
            lastEvent->setText(getStr("nav/event_action", event.view->getClassString(),
                event.pointer ? getStr("nav/event_pointer") : getStr("nav/event_key")));
        });

    eventCard->addView(makeHint(getStr("nav/event_hint")));
    page->addView(eventCard);

    return page;
}

} // namespace demo

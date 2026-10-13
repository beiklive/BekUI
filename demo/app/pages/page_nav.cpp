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

// 导航页的根节点：事件订阅句柄 + 延迟任务句柄都挂在它身上。
//
// 为什么必须这样：切页会 `delete` 旧页（Pager 现建现删）。订阅与延迟回调用的是
// 页面内部的 Label，页面一删它们就是野指针 —— 实测踩过（`static Subscription`
// 活到进程结束，切页回来时 Label 已析构，ASAN 直接报 heap-use-after-free）。
//
// 成员析构顺序（派生类成员 → 基类）正好帮上忙：订阅先 RAII 退订、AliveFlag 先失效，
// 之后 `Box::~Box` 删子节点时触发的焦点事件就不会再打回半死的页面。
class NavPage : public Box
{
  public:
    NavPage()
        : Box(Axis::COLUMN)
    {
    }

    // 事件行 3 秒后自动复位；每次来事件都取消上一个延迟再重排（delay + cancel 的真实用法）
    void scheduleReset()
    {
        this->pendingReset.cancel();

        auto guard = this->alive.handle();
        this->pendingReset = task::delay(3000, [this, guard] {
            if (guard.expired() || !guard.lock()->isAlive())
                return; // 页面已经被删了：延迟回调什么都不能碰
            if (this->lastEvent != nullptr)
                this->lastEvent->setText(getStr("nav/event_none"));
        });
    }

    Subscription focusSub;
    Subscription actionSub;
    task::DelayHandle pendingReset;
    AliveFlag alive;
    Label* lastEvent = nullptr;
};

} // namespace

Box* buildNavPage()
{
    NavPage* page = new NavPage();
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);
    page->setGap(getTheme().number("metric.spacing.md", 12.0f));
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

    // ---- 事件总线：焦点/动作事件实时显示 ----
    Rectangle* eventCard = makeCard();
    eventCard->addView(makeText(getStr("nav/event_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    Label* lastEvent   = makeHint(getStr("nav/event_none"));
    page->lastEvent    = lastEvent;
    eventCard->addView(lastEvent);

    // 订阅句柄是页面成员：页面被 delete 时自动退订（见 NavPage 的注释）
    page->focusSub = getEventBus().subscribe<FocusChangedEvent>(
        [page](const FocusChangedEvent& event) {
            if (page->lastEvent == nullptr || event.current == nullptr)
                return;
            page->lastEvent->setText(getStr("nav/event_focus", event.current->getClassString()));
            page->scheduleReset();
        });

    page->actionSub = getEventBus().subscribe<ActionTriggeredEvent>(
        [page](const ActionTriggeredEvent& event) {
            if (page->lastEvent == nullptr || event.view == nullptr)
                return;
            page->lastEvent->setText(getStr("nav/event_action", event.view->getClassString(),
                event.pointer ? getStr("nav/event_pointer") : getStr("nav/event_key")));
            page->scheduleReset();
        });

    eventCard->addView(makeHint(getStr("nav/event_hint")));
    // 提示里说明"3 秒后自动复位"，让 delay 的行为对使用者可见
    page->addView(eventCard);

    return page;
}

} // namespace demo

// 无窗口测试：事件总线（按类型订阅、RAII 退订、派发期间增删安全）。
//
// 注意 Switch 构建带 -fno-rtti，所以总线内部用"静态 token 取地址"当 key，
// 这里不依赖任何 RTTI 行为。

#include <cstdio>
#include <string>
#include <vector>

#include "bklv/core/event.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

struct PingEvent
{
    int value = 0;
};

struct OtherEvent
{
    std::string text;
};

} // namespace

int main()
{
    std::printf("== 1. 订阅与派发 ==\n");
    {
        EventBus bus;
        int received = 0;

        Subscription sub = bus.subscribe<PingEvent>([&](const PingEvent& event) {
            received += event.value;
        });

        checkEqualSize(bus.getSubscriberCount<PingEvent>(), 1, "有一个订阅者");
        check(sub.isActive(), "句柄是有效的");

        PingEvent ping;
        ping.value = 7;
        bus.publish(ping);
        checkEqualInt(received, 7, "收到事件");

        ping.value = 5;
        bus.publish(ping);
        checkEqualInt(received, 12, "累加第二次数值");

        // 没有订阅者的事件类型：什么都不发生
        bus.publish(OtherEvent{ "没人听" });
        checkEqualInt(received, 12, "无关事件不影响");
    }

    std::printf("\n== 2. 类型隔离 ==\n");
    {
        EventBus bus;
        int pings = 0, others = 0;

        Subscription a = bus.subscribe<PingEvent>([&](const PingEvent&) { pings++; });
        Subscription b = bus.subscribe<OtherEvent>([&](const OtherEvent&) { others++; });

        bus.publish(PingEvent{ 1 });
        checkEqualInt(pings, 1, "PingEvent 只给 PingEvent 订阅者");
        checkEqualInt(others, 0, "另一种事件不受影响");

        bus.publish(OtherEvent{ "x" });
        checkEqualInt(pings, 1, "反过来也一样");
        checkEqualInt(others, 1, "OtherEvent 收到");
        checkEqualSize(bus.getTotalSubscriberCount(), 2, "总订阅数 2");
    }

    std::printf("\n== 3. RAII 退订 ==\n");
    {
        EventBus bus;
        int count = 0;

        {
            Subscription sub = bus.subscribe<PingEvent>([&](const PingEvent&) { count++; });
            bus.publish(PingEvent{});
            checkEqualInt(count, 1, "作用域内收到");
        } // 句柄析构 → 退订

        bus.publish(PingEvent{});
        checkEqualInt(count, 1, "句柄析构后不再收到");
        checkEqualSize(bus.getSubscriberCount<PingEvent>(), 0, "订阅表已清空");

        // 手动退订
        int manual = 0;
        Subscription sub = bus.subscribe<PingEvent>([&](const PingEvent&) { manual++; });
        bus.publish(PingEvent{});
        sub.unsubscribe();
        check(!sub.isActive(), "手动退订后句柄失效");
        bus.publish(PingEvent{});
        checkEqualInt(manual, 1, "手动退订生效");
    }

    std::printf("\n== 4. 多个订阅者与移动语义 ==\n");
    {
        EventBus bus;
        int a = 0, b = 0;

        Subscription first = bus.subscribe<PingEvent>([&](const PingEvent&) { a++; });
        Subscription second = bus.subscribe<PingEvent>([&](const PingEvent&) { b++; });
        checkEqualSize(bus.getSubscriberCount<PingEvent>(), 2, "两个订阅者");

        bus.publish(PingEvent{});
        checkEqualInt(a, 1, "第一个收到");
        checkEqualInt(b, 1, "第二个收到");

        // 移动句柄：原句柄失效，新句柄管退订
        Subscription moved = std::move(first);
        check(!first.isActive(), "被移动的句柄失效");
        check(moved.isActive(), "新句柄有效");
        moved.unsubscribe();
        bus.publish(PingEvent{});
        checkEqualInt(a, 1, "移动后退订的是新句柄");
        checkEqualInt(b, 2, "另一个订阅者不受影响");
    }

    std::printf("\n== 5. 派发期间增删订阅 ==\n");
    {
        EventBus bus;
        int outer = 0, inner = 0;
        Subscription innerSub;

        Subscription outerSub = bus.subscribe<PingEvent>([&](const PingEvent&) {
            outer++;
            if (!innerSub.isActive())
                innerSub = bus.subscribe<PingEvent>([&](const PingEvent&) { inner++; });
        });

        bus.publish(PingEvent{});
        checkEqualInt(outer, 1, "派发中新增订阅不会破坏遍历");
        checkEqualInt(inner, 0, "新订阅者本轮不参与（快照语义）");

        bus.publish(PingEvent{});
        checkEqualInt(outer, 2, "第二轮");
        checkEqualInt(inner, 1, "新订阅者下一轮生效");
    }

    std::printf("\n== 6. clear 与内置事件 ==\n");
    {
        EventBus bus;
        int hits = 0;
        Subscription sub = bus.subscribe<PingEvent>([&](const PingEvent&) { hits++; });
        bus.clear();
        checkEqualSize(bus.getTotalSubscriberCount(), 0, "clear 清空全部订阅");
        bus.publish(PingEvent{});
        checkEqualInt(hits, 0, "清空后不再派发");

        // 进程级总线：内置事件类型能订阅/派发
        int focusEvents = 0;
        Subscription focusSub = getEventBus().subscribe<FocusChangedEvent>(
            [&](const FocusChangedEvent& event) { focusEvents += event.current != nullptr ? 1 : 0; });

        FocusChangedEvent event;
        event.previous = nullptr;
        event.current  = nullptr;
        getEventBus().publish(event);
        checkEqualInt(focusEvents, 0, "空焦点不计");

        getEventBus().clear();
        checkEqualSize(getEventBus().getSubscriberCount<FocusChangedEvent>(), 0, "进程级总线可清理");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

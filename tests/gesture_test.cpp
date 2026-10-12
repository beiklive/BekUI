// 无窗口测试：指针手势（轻触 / 拖动 / 滚轮 + 沿父链分发）。

#include <cstdio>
#include <memory>
#include <string>

#include "bklv/core/box.hpp"
#include "bklv/core/gesture.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

Box* makeBox(float width, float height)
{
    Box* box = new Box(Axis::COLUMN);
    box->setWidth(width);
    box->setHeight(height);
    return box;
}

} // namespace

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");

    const Point origin(0.0f, 0.0f);

    std::printf("== 1. 轻触：位移/时长在阈值内才触发 ==\n");
    {
        int taps = 0;
        TapGestureRecognizer tap([&] { taps++; });

        check(tap.recognize(GestureState::START, Point(100.0f, 100.0f), origin), "START 被接管");
        check(!tap.recognize(GestureState::END, Point(102.0f, 100.0f), origin) == false,
            "小幅移动后抬起算轻触");
        checkEqualInt(taps, 1, "轻触回调触发一次");

        // 移动超过阈值 → 不算轻触
        tap.recognize(GestureState::START, Point(100.0f, 100.0f), origin);
        tap.recognize(GestureState::CANCEL, Point(100.0f, 100.0f), origin);
        tap.recognize(GestureState::START, Point(100.0f, 100.0f), origin);
        tap.recognize(GestureState::STAY, Point(140.0f, 100.0f), origin); // 移动 40 > 8
        tap.recognize(GestureState::END, Point(140.0f, 100.0f), origin);
        checkEqualInt(taps, 1, "拖动过的不算轻触");

        // 滚轮不干扰轻触
        tap.recognize(GestureState::START, Point(10.0f, 10.0f), origin);
        check(!tap.recognize(GestureState::SCROLL, Point(10.0f, 10.0f), Point(0.0f, 40.0f)),
            "轻触不处理滚轮");
    }

    std::printf("\n== 2. 拖动：START / 增量 / END ==\n");
    {
        int starts = 0, ends = 0;
        float accumulated = 0.0f;

        PanGestureRecognizer pan({ [&](const Point&) { starts++; },
            [&](const Point& delta) { accumulated += delta.x; }, [&] { ends++; } });

        pan.recognize(GestureState::START, Point(50.0f, 50.0f), origin);
        pan.recognize(GestureState::STAY, Point(60.0f, 50.0f), Point(10.0f, 0.0f));
        pan.recognize(GestureState::STAY, Point(75.0f, 50.0f), Point(15.0f, 0.0f));
        pan.recognize(GestureState::END, Point(75.0f, 50.0f), origin);

        checkEqualInt(starts, 1, "开始回调一次");
        checkEqualInt(ends, 1, "结束回调一次");
        checkFloat(accumulated, 25.0f, "增量累加 10 + 15");

        check(!pan.isActive(), "结束后不再处于手势中");
        check(!pan.recognize(GestureState::STAY, Point(80.0f, 50.0f), Point(5.0f, 0.0f)),
            "没 START 就不响应移动");
    }

    std::printf("\n== 3. 滚轮：只认 SCROLL 状态 ==\n");
    {
        Point last{ 0.0f, 0.0f };
        int scrolls = 0;
        ScrollGestureRecognizer scroll([&](const Point& delta) { last = delta; scrolls++; });

        check(scroll.recognize(GestureState::SCROLL, origin, Point(0.0f, 40.0f)), "SCROLL 被接管");
        checkFloat(last.y, 40.0f, "拿到纵向增量");
        checkEqualInt(scrolls, 1, "回调一次");

        check(!scroll.recognize(GestureState::STAY, origin, Point(0.0f, 40.0f)), "STAY 不管");
        check(!scroll.recognize(GestureState::SCROLL, origin, Point(0.0f, 0.0f)), "零增量不管");
    }

    std::printf("\n== 4. 沿父链分发 ==\n");
    {
        // 父容器拖，子节点不接：子节点收到的事件要冒泡给父容器
        Box* parent = makeBox(200.0f, 200.0f);
        Box* child  = makeBox(100.0f, 100.0f);
        parent->addView(child);

        int pans = 0;
        parent->addGesture(std::make_unique<PanGestureRecognizer>(PanGestureRecognizer::Callbacks{
            nullptr, [&](const Point&) { pans++; }, nullptr }));

        check(child->dispatchGesture(GestureState::START, origin, origin), "子节点把 START 冒泡上去了");
        child->dispatchGesture(GestureState::STAY, origin, Point(5.0f, 0.0f));
        checkEqualInt(pans, 1, "父容器收到了拖动增量");

        // 子节点自己接住之后，父容器不再收到
        int childPans = 0, parentPans = 0;
        Box* p2 = makeBox(200.0f, 200.0f);
        Box* c2 = makeBox(100.0f, 100.0f);
        p2->addView(c2);
        c2->addGesture(std::make_unique<PanGestureRecognizer>(PanGestureRecognizer::Callbacks{
            nullptr, [&](const Point&) { childPans++; }, nullptr }));
        p2->addGesture(std::make_unique<PanGestureRecognizer>(PanGestureRecognizer::Callbacks{
            nullptr, [&](const Point&) { parentPans++; }, nullptr }));

        c2->dispatchGesture(GestureState::START, origin, origin);
        c2->dispatchGesture(GestureState::STAY, origin, Point(3.0f, 0.0f));
        checkEqualInt(childPans, 1, "子节点自己吃掉");
        checkEqualInt(parentPans, 0, "父容器不再收到（不重复响应）");

        // 禁用的识别器不参与
        auto disabled = std::make_unique<PanGestureRecognizer>(PanGestureRecognizer::Callbacks{
            nullptr, [&](const Point&) { parentPans++; }, nullptr });
        disabled->setEnabled(false);
        Box* p3 = makeBox(100.0f, 100.0f);
        p3->addGesture(std::move(disabled));
        check(!p3->dispatchGesture(GestureState::START, origin, origin), "禁用的识别器不吃事件");

        delete parent;
        delete p2;
        delete p3;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

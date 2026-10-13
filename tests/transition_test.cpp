// 无窗口测试：Shell 内容栈（层级下钻）的转场动画。
//
// 转场靠绘制期变换（setTranslateX/setAlpha），不参与布局，所以能按固定 dt 步进、
// 断言中间态与落位后的状态。

#include <cstdio>
#include <string>

#include "bklv/core/app.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/shell.hpp"
#include "bklv/widgets/button.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

Box* makePage(const char* text)
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeightPercentage(100.0f);
    page->addView(new Button(text));
    return page;
}

} // namespace

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");
    setTheme("default");

    std::printf("== 1. 转场：滑动 + 淡入，落位后清干净 ==\n");
    {
        Box* first = makePage("第一页");
        Shell* frame = new Shell();
        frame->pushContentView(first);
        frame->setWidth(800.0f);
        frame->setHeight(600.0f);
        frame->setContentTransition(ContentTransition::SLIDE_FADE);
        frame->setContentTransitionDuration(0.2f);
        YGNodeCalculateLayout(frame->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);

        check(!frame->isContentTransitioning(), "初始没有转场");

        Box* detail = makePage("详情");
        frame->pushContentView(detail);

        check(frame->isContentTransitioning(), "push 之后进入转场");
        checkEqualSize(frame->getContentStackSize(), 2, "内容栈有两层");
        check(frame->getOutgoingContentView() == first, "旧页是转场中的 out 视图");

        const float startX = detail->getTransform().translateX;
        const float startAlpha = detail->getAlpha();
        check(startX > 0.0f, "新页从右侧开始（位移为正）");
        check(startAlpha < 0.2f, "新页起始接近透明");

        // 推进一半
        frame->stepContentTransition(0.1f);
        check(frame->isContentTransitioning(), "半程仍在转场");
        check(detail->getTransform().translateX < startX, "新页在向左走");
        check(detail->getAlpha() > startAlpha, "新页在变清晰");
        check(first->getAlpha() < 1.0f, "旧页在淡出");

        // 推到底
        frame->stepContentTransition(0.5f);
        check(!frame->isContentTransitioning(), "转场结束");
        checkFloat(detail->getTransform().translateX, 0.0f, "新页落位（位移归零）");
        checkFloat(detail->getAlpha(), 1.0f, "新页完全不透明");
        checkFloat(first->getAlpha(), 1.0f, "旧页透明度已复位");
        check(frame->getOutgoingContentView() == nullptr, "out 视图已清空");
        check(first->getParent() != frame->getContentLayer(), "旧页已从内容区摘掉（但仍在栈里）");
        checkEqualSize(frame->getContentStackSize(), 2, "旧页留在栈里，返回时状态还在");

        std::printf("\n== 2. 返回：旧页滑出并被释放 ==\n");
        {
            bool callbackRan = false;
            frame->popContentView([&] { callbackRan = true; });

            check(frame->isContentTransitioning(), "pop 也走转场");
            checkEqualSize(frame->getContentStackSize(), 1, "栈回到一层");
            check(frame->getOutgoingContentView() == detail, "被弹出的页是 out 视图");
            check(!callbackRan, "回调要等转场结束才跑");

            frame->stepContentTransition(0.5f);
            check(callbackRan, "转场结束后回调执行");
            check(frame->getOutgoingContentView() == nullptr, "out 视图清空");
            check(frame->getContentView() == first, "当前页回到第一页");
            checkFloat(first->getTransform().translateX, 0.0f, "回来的页面位移归零");
        }

        delete frame;
    }

    std::printf("\n== 3. 关掉转场：直接落位 ==\n");
    {
        Box* first = makePage("A");
        Shell* frame = new Shell();
        frame->pushContentView(first);
        frame->setWidth(800.0f);
        frame->setHeight(600.0f);
        frame->setContentTransition(ContentTransition::NONE);
        YGNodeCalculateLayout(frame->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);

        Box* second = makePage("B");
        frame->pushContentView(second);
        check(!frame->isContentTransitioning(), "NONE 不产生转场");
        check(frame->getContentView() == second, "新页直接生效");
        checkFloat(second->getAlpha(), 1.0f, "不透明");
        check(first->getParent() != frame->getContentLayer(), "旧页已经摘掉");

        bool ran = false;
        frame->popContentView([&] { ran = true; });
        check(ran, "NONE 时回调立即执行");

        delete frame;
    }

    std::printf("\n== 4. 只有淡入淡出（不滑动）==\n");
    {
        Box* first = makePage("A");
        Shell* frame = new Shell();
        frame->pushContentView(first);
        frame->setWidth(800.0f);
        frame->setHeight(600.0f);
        frame->setContentTransition(ContentTransition::FADE);
        frame->setContentTransitionDuration(0.2f);
        YGNodeCalculateLayout(frame->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);

        Box* second = makePage("B");
        frame->pushContentView(second);
        checkFloat(second->getTransform().translateX, 0.0f, "FADE 不位移");
        check(second->getAlpha() < 0.2f, "但有淡入");

        frame->stepContentTransition(0.5f);
        checkFloat(second->getAlpha(), 1.0f, "淡入完成");

        delete frame;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

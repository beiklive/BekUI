// 无窗口测试：Shell 内容栈（层级下钻）的**入场 / 出场**转场。
//
// 语义（v23 起改成两段）：
//   push：旧层先**出场**（它还挂在树上，所以看得到）→ 出场播完摘它 → 挂新层 → 新层**入场**
//   pop ：被弹出的临时页先**出场** → 出场播完释放它 → 挂回上一层 → 上一层**入场**
// 两段都是**绘制期变换**（setTranslateX/setAlpha），不参与布局，所以能按固定 dt 步进。

#include <cstdio>
#include <string>

#include "bklv/core/app.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/transition.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/shell.hpp"

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

    std::printf("== 1. push：旧层先出场，新层再入场 ==\n");
    {
        Box* first = makePage("第一页");
        Shell* shell = new Shell();
        shell->pushContentView(first);
        shell->setWidth(800.0f);
        shell->setHeight(600.0f);
        shell->setEnterTransition(ContentTransition::SLIDE_FADE);
        shell->setExitTransition(ContentTransition::SLIDE_FADE);
        shell->setTransitionDuration(0.2f);
        YGNodeCalculateLayout(shell->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);

        check(!shell->isEntering() && !shell->isExiting(), "首屏没有转场");

        Box* detail = makePage("详情");
        shell->pushContentView(detail);

        // ---- 第一段：旧层出场 ----
        check(shell->isExiting(), "push 先让旧层出场");
        check(!shell->isEntering(), "出场期间不是入场");
        check(shell->getOutgoingContentView() == first, "出场的就是旧层");
        check(first->getParent() == shell->getContentLayer(), "出场期间旧层还挂在内容层（所以看得到）");
        check(detail->getParent() == nullptr, "新层这时还没挂上去");
        checkEqualSize(shell->getContentStackSize(), 2, "逻辑上已经是两层");
        check(shell->getContentView() == detail, "逻辑上的当前层已经是新层");
        checkFloat(first->getAlpha(), 1.0f, "出场起点：旧层不透明");
        checkFloat(first->getTransform().translateX, 0.0f, "出场起点：位移为 0");

        shell->stepContentTransition(0.1f);
        check(shell->isExiting(), "半程仍在出场");
        check(first->getTransform().translateX < 0.0f, "旧层在往左走");
        check(first->getAlpha() < 1.0f, "旧层在淡出");

        shell->stepContentTransition(0.5f);
        check(!shell->isExiting(), "出场结束");

        // ---- 第二段：换层 + 新层入场 ----
        check(first->getParent() == nullptr, "出场播完旧层才脱离树（但留在栈里）");
        check(detail->getParent() == shell->getContentLayer(), "出场播完新层才挂上去");
        check(shell->isEntering(), "接着是新层入场");
        checkEqualSize(shell->getContentStackSize(), 2, "旧层留在栈里，返回时状态还在");
        checkFloat(first->getAlpha(), 1.0f, "旧层出场后透明度复位（下次还要用）");

        const float startX = detail->getTransform().translateX;
        check(startX > 0.0f, "新层从右侧开始入场");
        check(detail->getAlpha() < 0.2f, "新层起始接近透明");

        shell->stepContentTransition(0.1f);
        check(detail->getTransform().translateX < startX, "新层在向左走");
        check(detail->getAlpha() > 0.0f, "新层在变清晰");

        shell->stepContentTransition(0.5f);
        check(!shell->isEntering() && !shell->isExiting(), "两段都播完");
        checkFloat(detail->getTransform().translateX, 0.0f, "新层落位（位移归零）");
        checkFloat(detail->getAlpha(), 1.0f, "新层完全不透明");
        check(shell->getOutgoingContentView() == nullptr, "没有正在出场的层");

        std::printf("\n== 2. pop：被弹出的页先出场，再释放，上一层入场 ==\n");
        {
            bool callbackRan = false;
            shell->popContentView([&] { callbackRan = true; });

            check(shell->isExiting(), "pop 先让被弹出的页出场");
            checkEqualSize(shell->getContentStackSize(), 1, "逻辑上栈已经回到一层");
            check(shell->getOutgoingContentView() == detail, "出场的是被弹出的那一页");
            check(!callbackRan, "回调要等出场播完才跑");
            checkFloat(detail->getAlpha(), 1.0f, "出场起点：不透明");

            shell->stepContentTransition(0.5f);
            check(callbackRan, "出场播完回调执行");
            check(shell->isEntering(), "接着上一层入场");
            check(shell->getContentView() == first, "当前页回到第一页");
            check(first->getParent() == shell->getContentLayer(), "上一页已经挂回内容层");

            shell->stepContentTransition(0.5f);
            check(!shell->isEntering() && !shell->isExiting(), "入场也播完");
            checkFloat(first->getTransform().translateX, 0.0f, "回来的页面位移归零");
            checkFloat(first->getAlpha(), 1.0f, "回来的页面不透明");
        }

        delete shell;
    }

    std::printf("\n== 3. NONE：入场/出场都直接落位 ==\n");
    {
        Box* first = makePage("A");
        Shell* shell = new Shell();
        shell->pushContentView(first);
        shell->setWidth(800.0f);
        shell->setHeight(600.0f);
        shell->setEnterTransition(ContentTransition::NONE);
        shell->setExitTransition(ContentTransition::NONE);
        YGNodeCalculateLayout(shell->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);

        Box* second = makePage("B");
        shell->pushContentView(second);
        check(!shell->isEntering() && !shell->isExiting(), "NONE 不产生转场");
        check(shell->getContentView() == second, "新页直接生效");
        check(second->getParent() == shell->getContentLayer(), "新页直接挂上");
        check(first->getParent() == nullptr, "旧页直接摘掉");
        checkFloat(second->getAlpha(), 1.0f, "不透明");
        checkFloat(second->getTransform().translateX, 0.0f, "不位移");

        bool ran = false;
        shell->popContentView([&] { ran = true; });
        check(ran, "NONE 时 pop 回调同步执行");
        check(shell->getContentView() == first, "NONE 时直接回到上一层");
        check(!shell->isEntering() && !shell->isExiting(), "NONE 时 pop 也不留转场");

        delete shell;
    }

    std::printf("\n== 4. 入场/出场可以分开配（FADE 只淡不滑）==\n");
    {
        Box* first = makePage("A");
        Shell* shell = new Shell();
        shell->pushContentView(first);
        shell->setWidth(800.0f);
        shell->setHeight(600.0f);
        shell->setEnterTransition(ContentTransition::FADE);
        shell->setExitTransition(ContentTransition::FADE);
        shell->setTransitionDuration(0.2f);
        YGNodeCalculateLayout(shell->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);

        checkEqualInt((int)shell->getEnterTransition(), (int)ContentTransition::FADE,
            "入场方式读得回来");
        checkEqualInt((int)shell->getExitTransition(), (int)ContentTransition::FADE,
            "出场方式读得回来");

        Box* second = makePage("B");
        shell->pushContentView(second);
        checkFloat(first->getTransform().translateX, 0.0f, "FADE 出场不位移");
        shell->stepContentTransition(0.5f);

        check(shell->isEntering(), "出场完接着入场");
        checkFloat(second->getTransform().translateX, 0.0f, "FADE 入场不位移");
        check(second->getAlpha() < 0.2f, "但入场有淡入");

        shell->stepContentTransition(0.5f);
        checkFloat(second->getAlpha(), 1.0f, "淡入完成");
        check(!shell->isEntering(), "入场结束");

        // 入场/出场分开设：入场 NONE、出场 SLIDE
        shell->setEnterTransition(ContentTransition::NONE);
        shell->setExitTransition(ContentTransition::SLIDE);
        Box* third = makePage("C");
        shell->pushContentView(third);
        check(shell->isExiting(), "出场照旧播");
        shell->stepContentTransition(0.5f);
        check(!shell->isEntering(), "入场设成 NONE 就不播入场");
        checkFloat(third->getAlpha(), 1.0f, "NONE 入场直接落位");

        delete shell;
    }

    std::printf("\n== 5. finishContentTransition：立刻落位 ==\n");
    {
        Box* first = makePage("A");
        Shell* shell = new Shell();
        shell->pushContentView(first);
        shell->setWidth(800.0f);
        shell->setHeight(600.0f);
        shell->setTransitionDuration(0.5f);
        YGNodeCalculateLayout(shell->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);

        Box* second = makePage("B");
        shell->pushContentView(second);
        check(shell->isExiting(), "出场在跑");
        shell->finishContentTransition();
        check(!shell->isEntering() && !shell->isExiting(), "finishContentTransition 之后彻底空闲");
        check(shell->getContentView() == second, "并且画面上已经是新层");
        check(second->getParent() == shell->getContentLayer(), "新层挂上了");
        checkFloat(second->getAlpha(), 1.0f, "新层不透明");

        // 出场 + 入场两段能被同一次 finish 收掉
        Box* third = makePage("C");
        shell->pushContentView(third);
        shell->finishContentTransition();
        check(!shell->isEntering() && !shell->isExiting(), "连续两段也能一次落位");

        delete shell;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

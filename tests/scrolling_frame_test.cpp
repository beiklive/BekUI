// 无窗口测试：滚动容器在**没有 ImGui 上下文**时的安全性。
//
// 这条路径踩过一次真 bug：`ScrollingFrame::update()` 的拖拽分支判了 `io != nullptr`，
// 而 `HScrollingFrame` 那份是复制过去改 x↔y 的，复制时把判断丢了 ——
// 于是"无窗口 + scrollingEnabled"直接空指针解引用；而且 HScrollingFrame 当时
// **没有任何测试**，所以谁都没发现（见 docs/architecture-review.html §4.2 P1）。
//
// 这里两个类各跑一遍 update()：不建 ImGui 上下文（GetCurrentContext() == nullptr）、
// 有内容、开着滚动，就必须安安静静地跑完。测试进程不建上下文，所以这条
// 断言本身就是"无窗口安全"的回归测试。
//
// 顺带钉住两个类共有的几何接口（getScrollRange / setContentOffset 的夹取），
// 让"横向那份和纵向那份行为一致"这件事有最基础的保障。

#include <cstdio>

#include "test_util.hpp"

#include "bklv/core/box.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/h_scrolling_frame.hpp"
#include "bklv/views/scrolling_frame.hpp"

using namespace bklv;

int main()
{
    initResources();

    // 前置条件：整个测试进程里没有 ImGui 上下文（否则这条测试就失去意义了）
    check(ImGui::GetCurrentContext() == nullptr, "测试进程没有 ImGui 上下文（无窗口）");

    std::printf("\n== 1. ScrollingFrame：无上下文时 update() 不崩 ==\n");
    {
        ScrollingFrame frame;
        Box* content = new Box(Axis::COLUMN);
        content->setHeight(2000.0f); // 比视口高得多 → 有滚动范围
        frame.setContentView(content);

        check(frame.isScrollable(), "滚动器声明自己是可滚动容器");
        check(frame.getContentView() == content, "内容视图挂上了");

        // 刻意开着滚动（默认就是 true），走拖拽/滚轮那几条分支
        frame.setScrollingEnabled(true);
        frame.update();
        check(true, "无上下文时 update() 跑完没崩（滚轮/拖拽分支）");

        // 惯性动画那条路也不能碰 ImGui
        frame.setContentOffsetY(100.0f, true);
        frame.update();
        check(true, "无上下文时带动画的 update() 跑完没崩");
    }

    std::printf("\n== 2. HScrollingFrame：同一套保证（这里曾经漏判 io）==\n");
    {
        HScrollingFrame frame;
        Box* content = new Box(Axis::ROW);
        content->setWidth(4000.0f);
        frame.setContentView(content);

        check(frame.isScrollable(), "横向滚动器声明自己是可滚动容器");
        check(frame.getContentView() == content, "内容视图挂上了");

        frame.setScrollingEnabled(true);
        frame.update();
        check(true, "无上下文时 update() 跑完没崩（这里原来会空指针解引用）");

        frame.setContentOffsetX(100.0f, true);
        frame.update();
        check(true, "无上下文时带动画的 update() 跑完没崩");
    }

    std::printf("\n== 3. 滚动范围与偏移夹取（两份行为一致）==\n");
    {
        // 画布尺寸没算过布局时，滚动范围必须是 0 而不是负数/垃圾值
        ScrollingFrame v;
        Box* vc = new Box(Axis::COLUMN);
        vc->setHeight(2000.0f);
        v.setContentView(vc);
        checkFloat(v.getScrollRange(), 0.0f, "没布局时纵向滚动范围为 0");

        HScrollingFrame h;
        Box* hc = new Box(Axis::ROW);
        hc->setWidth(4000.0f);
        h.setContentView(hc);
        checkFloat(h.getScrollRange(), 0.0f, "没布局时横向滚动范围为 0");

        // 超范围的偏移要被夹回区间内（不能把内容拖出可视区）
        v.setContentOffsetY(99999.0f);
        check(v.getContentOffsetY() <= 1.0f, "纵向偏移被夹进滚动范围");
        h.setContentOffsetX(99999.0f);
        check(h.getContentOffsetX() <= 1.0f, "横向偏移被夹进滚动范围");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

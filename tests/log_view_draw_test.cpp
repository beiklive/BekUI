// 绘制开销测试：日志面板的**每帧代价必须与日志条数无关**（用户硬要求：不能影响性能）。
//
// 做法（全部确定性、不依赖计时）：
//   建一个真 ImGui 上下文 + 内置字体 + 自建 draw list，面板尺寸固定，然后
//   ① 30 条日志 与 1000 条日志（面板只画得下十几行）**顶点数/绘制命令数必须一样**；
//   ② 滚到中间 / 底部同样不变（可见区间是"一次除法"算出来的，不是从头遍历）；
//   ③ 反复 frame() 不再重建行缓存（除第一次），顶点数也不变（稳态零分配、零重算）。
//
// 另外把 1000 行时的一帧耗时打出来（参考值，不做断言：CI 上计时不稳）。

#include <chrono>
#include <cstdio>

#include <yoga/Yoga.h>

#include "bklv/core/appearance.hpp"
#include "bklv/core/font.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/debug_log_view.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

struct DrawStats
{
    int vertices = 0;
    int commands = 0;
};

// 画一帧，返回顶点数与绘制命令数
DrawStats drawOnce(DebugLogView* view, ImDrawList* list)
{
    list->_ResetForNewFrame();
    list->_FringeScale    = 1.0f;
    list->_InvFringeScale = 1.0f;

    FrameContext ctx;
    ctx.drawList   = list;
    ctx.pixelRatio = 1.0f;

    view->frame(&ctx);

    DrawStats stats;
    stats.vertices = list->VtxBuffer.Size;
    stats.commands = list->CmdBuffer.Size;
    return stats;
}

void layoutPanel(DebugLogView* view)
{
    view->setEnabled(true);
    view->setWidth(600.0f);
    view->setHeight(400.0f);
    YGNodeCalculateLayout(view->getYGNode(), 600.0f, 400.0f, YGDirectionLTR);
    view->notifyLayout();
}

void fillLogs(int count)
{
    Logger::clear();
    for (int i = 0; i < count; i++)
        Logger::info("日志第 " + std::to_string(i) + " 条：这一行有点长，用来保证裁剪路径被走到");
}

} // namespace

int main()
{
    bklv::initResources();
    setTheme("default");

    // 日志面板要画文字就得有字体：建上下文 + 内置字体（和 shadow_test 同一套路）
    ImGui::CreateContext();
    ImGui::GetIO().Fonts->AddFontDefault();
    ImGui::GetIO().Fonts->Build();
    bklv::setDefaultFont(ImGui::GetIO().Fonts->Fonts[0]);

    ImDrawList* list = new ImDrawList(ImGui::GetDrawListSharedData());

    std::printf("== 1. 绘制量只跟「看得见的行数」有关，与日志总量无关 ==\n");
    {
        Logger::setMaxEntries(1000);

        DebugLogView* view = new DebugLogView();
        layoutPanel(view);

        fillLogs(30);
        view->update();
        const DrawStats small = drawOnce(view, list);
        check(small.vertices > 0, "画出了东西（字体生效、有 rect + 文字顶点）");
        checkEqualSize(view->getCachedRowCount(), 30, "30 条日志 → 30 行缓存");

        fillLogs(1000);
        view->update();
        const DrawStats big = drawOnce(view, list);
        checkEqualSize(view->getCachedRowCount(), 1000, "1000 条日志 → 1000 行缓存");

        checkEqualInt(big.vertices, small.vertices,
            "1000 条 vs 30 条：顶点数完全相同（只画面板里那十几行）");
        checkEqualInt(big.commands, small.commands, "绘制命令数也完全相同");

        std::printf("  [数据] 30 条 %d 顶点 / %d 命令；1000 条 %d 顶点 / %d 命令\n",
            small.vertices, small.commands, big.vertices, big.commands);

        std::printf("\n== 2. 滚动到任意位置，绘制量不变（可见区间是 O(1) 算出来的）==\n");
        const float maxScroll = view->getMaxScroll();
        check(maxScroll > 0.0f, "1000 行足够长，能滚");

        view->setScrollOffset(maxScroll * 0.5f);
        view->update();
        const DrawStats middle = drawOnce(view, list);
        checkEqualInt(middle.vertices, big.vertices, "滚到中间：顶点数不变");

        view->scrollToBottom();
        view->update();
        const DrawStats bottom = drawOnce(view, list);
        checkEqualInt(bottom.vertices, big.vertices, "滚到底：顶点数不变");

        std::printf("\n== 3. 反复画同一帧：不再重建缓存、顶点数稳定 ==\n");
        const uint64_t rebuilds = view->getRebuildCount();
        for (int i = 0; i < 20; i++)
        {
            view->update();
            const DrawStats again = drawOnce(view, list);
            checkEqualInt(again.vertices, bottom.vertices, "重复绘制顶点数不变");
        }
        check(view->getRebuildCount() == rebuilds, "20 帧没有新日志：行缓存一次都没重建");

        // 只改滚动位置也不重建（缓存只看日志版本/过滤/字号/字体）
        view->scrollBy(-3);
        view->update();
        drawOnce(view, list);
        check(view->getRebuildCount() == rebuilds, "只滚动不重建缓存");

        std::printf("\n== 4. 一帧耗时参考值（1000 行、面板里画十几行）==\n");
        {
            const auto start = std::chrono::steady_clock::now();
            const int frames = 500;
            for (int i = 0; i < frames; i++)
            {
                view->update();
                drawOnce(view, list);
            }
            const auto end = std::chrono::steady_clock::now();
            const double us =
                std::chrono::duration<double, std::micro>(end - start).count() / frames;
            std::printf("  [数据] 平均 %.1f 微秒/帧（含 update + 绘制）\n", us);

            // 极宽松的上限：只用来抓"每帧遍历上千行 + 拼上千个字符串"这类回归
            check(us < 5000.0, "一帧远低于 5 毫秒（不会拖慢 60fps 主循环）");
        }

        delete view;
    }

    // 注意：**不要** ImGui::DestroyContext()（没接平台后端时它会在 Shutdown 里踩空）
    delete list;

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

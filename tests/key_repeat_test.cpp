// 无窗口测试：长按重复（导航/动作的响应节奏）。
//
// 这条是被真问题逼出来的：原来两个键共用一个"边沿状态"，
// 按住方向键时每帧都被判成"刚按下"，导航直接飙到帧率（60 次/秒）。
// 现在按 borealis 的做法：首次按下立刻响应，按住则 250ms 后每 100ms 一次。

#include <cstdio>
#include <string>

#include "bklv/core/input.hpp"

using namespace bklv;

static int g_failed = 0;
static int g_checks = 0;

static void check(bool ok, const std::string& what)
{
    g_checks++;
    if (ok)
        std::printf("  [通过] %s\n", what.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s\n", what.c_str());
    }
}

static void checkEqualInt(int got, int want, const std::string& what)
{
    g_checks++;
    if (got == want)
        std::printf("  [通过] %s  =>  %d\n", what.c_str(), got);
    else
    {
        g_failed++;
        std::printf("  [失败] %s  期望 %d 实际 %d\n", what.c_str(), want, got);
    }
}

int main()
{
    std::printf("\n== 1. 按下立刻响应一次 ==\n");
    {
        KeyRepeater repeater;
        check(repeater.step(true, 0.0), "刚按下立刻响应");
        check(!repeater.step(true, 0.01), "下一帧不响应");
        check(!repeater.step(true, 0.24), "250ms 之前不重复");
        check(repeater.step(true, 0.26), "过了 250ms 开始重复");
    }

    std::printf("\n== 2. 重复间隔是 100ms ==\n");
    {
        KeyRepeater repeater;
        repeater.step(true, 0.0);          // 首次
        check(repeater.step(true, 0.26), "第一个重复点");
        check(!repeater.step(true, 0.30), "间隔内不响应");
        check(repeater.step(true, 0.37), "第二个重复点");
        check(!repeater.step(true, 0.40), "间隔内不响应");
        check(repeater.step(true, 0.48), "第三个重复点");
    }

    std::printf("\n== 3. 按住 1 秒的触发次数（回归：不能是帧率）==\n");
    {
        KeyRepeater repeater;
        int fires = 0;
        // 按住不放，按 60fps 推进 1 秒
        for (int frame = 0; frame < 60; frame++)
        {
            const double now = (double)frame / 60.0;
            if (repeater.step(true, now))
                fires++;
        }

        // 首次 1 次 + 250ms 之后每 100ms 一次（到 0.983s 为止）= 9 次
        checkEqualInt(fires, 9, "按住 1 秒触发 9 次（而不是 60 次）");
        check(fires < 15, "远低于帧率");
    }

    std::printf("\n== 4. 松开再按会重新立刻响应 ==\n");
    {
        KeyRepeater repeater;
        repeater.step(true, 0.0);
        check(!repeater.step(false, 0.05), "松开当帧不响应");
        check(repeater.step(true, 0.06), "重新按下立刻响应");
        check(!repeater.step(true, 0.10), "重新按下后进入 delay 等待");
        check(repeater.step(true, 0.32), "重新按下后 250ms 再重复");
    }

    std::printf("\n== 5. 方向切换互不影响 ==\n");
    {
        KeyRepeater up, down;
        check(up.step(true, 0.0), "按上");
        check(down.step(true, 0.0), "同帧按下也要立刻响应");
        check(!up.step(true, 0.1), "上的重复计时没被下打断");
        check(!down.step(true, 0.1), "下的重复计时独立");
        check(up.step(true, 0.26), "上到点重复");
        check(down.step(true, 0.26), "下到点重复");
    }

    std::printf("\n== 6. 节奏可配置 ==\n");
    {
        KeyRepeater fast(50, 20);
        fast.step(true, 0.0);
        check(fast.step(true, 0.06), "50ms 后就开始重复");
        check(fast.step(true, 0.09), "20ms 间隔");

        KeyRepeater slow(500, 300);
        slow.step(true, 0.0);
        check(!slow.step(true, 0.4), "500ms 之前不重复");
        check(slow.step(true, 0.55), "过了 500ms 才重复");

        slow.reset();
        check(slow.step(true, 1.0), "reset 之后重新立刻响应");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

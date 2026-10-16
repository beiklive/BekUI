// 无窗口测试：时间源（core/time.hpp）。
//
// 这一层也以前没有测试（评审里点名过）。它有三条容易踩的约定，都在这里钉住：
//   ① `getFrameDeltaSeconds()` **没有 ImGui 上下文时要给一个安全值**（按 60fps 估），
//      不能去读 GetIO() —— 否则无窗口测试直接空指针（Animatable / ScrollingFrame /
//      ProgressSpinner 上先后踩过三次）。
//   ② `getCPUTimeUsec()` / `getSecondsNow()` 必须是**单调**的、并且不依赖 ImGui 上下文
//      （Button 的按下反馈原来把 ImGui::GetTime() 和自累计时钟混用，见评审 §4.2）。
//   ③ 两者都是"自进程启动"：同一帧里多次调用只增不减。

#include <cmath>
#include <cstdio>

#include "test_util.hpp"

#include "bklv/core/time.hpp"

using namespace bklv;

int main()
{
    // 前置条件：整条测试都不建 ImGui 上下文
    check(ImGui::GetCurrentContext() == nullptr, "测试进程没有 ImGui 上下文（无窗口）");

    std::printf("\n== 1. 帧间隔在没有上下文时也要安全 ==\n");
    {
        const float dt = getFrameDeltaSeconds();
        check(dt > 0.0f, "帧间隔是正数（不会把动画算成 0 步长）");
        checkFloat(dt, 1.0f / 60.0f, "无上下文时按 60fps 估算");
        check(!(dt != dt), "帧间隔不是 NaN");
    }

    std::printf("\n== 2. 单调时钟：只增不减、不依赖 ImGui ==\n");
    {
        const uint64_t t0 = getCPUTimeUsec();
        const uint64_t t1 = getCPUTimeUsec();
        check(t1 >= t0, "getCPUTimeUsec 单调不减");

        const double s0 = getSecondsNow();
        const double s1 = getSecondsNow();
        check(s1 >= s0, "getSecondsNow 单调不减");

        // 两个 API 必须同源：秒 × 1e6 与微秒的差在一个微秒级抖动范围内
        const double diff = std::fabs(s1 * 1000000.0 - (double)getCPUTimeUsec());
        check(diff < 1000.0, "getSecondsNow 与 getCPUTimeUsec 同源（差 < 1ms）");
    }

    std::printf("\n== 3. 能真的量出时间（不是永远返回 0）==\n");
    {
        // 忙等一小会儿：确认时钟在走。用 clock() 做忙等以免依赖 sleep 的精度。
        const uint64_t start = getCPUTimeUsec();
        volatile double sink = 0.0;
        while (getCPUTimeUsec() - start < 2000) // 2ms
            sink += 1.0;
        (void)sink;

        const uint64_t elapsed = getCPUTimeUsec() - start;
        check(elapsed >= 2000, "忙等 2ms 后测得的间隔 >= 2ms（时钟在走）");
    }

    std::printf("\n== 4. Ticking 注册表：注册/注销成对，不留悬挂指针 ==\n");
    {
        // 没有注册对象时也要能安全跑一遍（内部就是遍历注册表）
        Ticking::updateTickings();
        check(true, "没有注册对象时 updateTickings() 安全");

        // Animatable 是 Ticking 的实现：update(dt) 显式步进，不依赖全局帧时间
        Animatable anim(0.0f);
        anim.addStep(10.0f, 1.0f, EasingFunction::LINEAR);
        anim.start();
        check(anim.isTicking(), "补间开始后处于 ticking");

        anim.update(0.5f);
        checkFloat((float)anim, 5.0f, "线性补间走到一半");
        anim.update(0.5f);
        checkFloat((float)anim, 10.0f, "线性补间走到终点");
        check(anim.isStopped(), "跑完之后自己停掉（不占每帧开销）");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

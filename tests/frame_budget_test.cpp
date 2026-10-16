// 无窗口测试：帧预算（core/frame_budget.hpp）与帧时间统计（core/frame_stats.hpp）。
//
// 目标（用户拍板）：**界面永不卡顿，任务可以晚**。
//   · 帧预算是"可延后工作"的时间闸门：回调、上传、动图换帧、SVG 栅格化、缓存淘汰
//     用完就停手，剩下的下一帧继续；
//   · 帧统计把"卡不卡"变成数字（p50 / p95 / p99 / 最坏帧 / 掉帧数）。
//
// 这里钉住三件事：
//   ① 统计的数学（分位数、掉帧计数、环形缓冲只留最近的样本）
//   ② 预算状态机（没开帧 / 不限制 / 用尽 / endFrame 复位 / resetGlobals 也能复位）
//   ③ **推迟不能丢活**：预算用尽时主线程回调与延迟任务一个都不跑，下一帧按原顺序补跑

#include <cstdio>
#include <vector>

#include "bklv/core/frame_budget.hpp"
#include "bklv/core/frame_stats.hpp"
#include "bklv/core/globals.hpp"
#include "bklv/core/task.hpp"
#include "bklv/core/time.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 把预算耗光：开一个极小的预算，自旋到它用尽（时间源是单调时钟，分辨率足够）
void exhaustBudget()
{
    budget::beginFrame(0.0005); // 0.5 微秒
    for (int i = 0; i < 500000 && !budget::isExhausted(); i++)
    {
    }
    check(budget::isExhausted(), "自旋之后预算确实用尽了");
}

} // namespace

int main()
{
    std::printf("\n== 1. 帧统计：分位数、掉帧计数、环形缓冲 ==\n");
    {
        FrameStats stats;
        checkEqualInt((int)stats.getCount(), 0, "新对象没有样本");
        const FrameTimeSummary empty = stats.getSummary();
        checkEqualInt((int)empty.count, 0, "空统计 count = 0");
        checkFloat((float)empty.worstMs, 0.0f, "空统计最坏帧 = 0（不 NaN）");

        // 100 个样本：第 i 个是 i 毫秒
        for (int i = 1; i <= 100; i++)
            stats.record((double)i / 1000.0);

        const FrameTimeSummary summary = stats.getSummary();
        checkEqualInt((int)summary.count, 100, "样本数 100");
        checkFloat((float)summary.averageMs, 50.5f, "平均值 = 50.5ms");
        checkFloat((float)summary.p50Ms, 50.0f, "p50 = 50ms（中位数，取下侧秩序）");
        checkFloat((float)summary.p95Ms, 95.0f, "p95 = 95ms");
        checkFloat((float)summary.p99Ms, 99.0f, "p99 = 99ms");
        checkFloat((float)summary.worstMs, 100.0f, "最坏帧 = 100ms");
        checkEqualInt((int)summary.droppedCount, 84, "超过 16.67ms 的有 84 帧（17..100）");
        checkEqualInt((int)stats.getCountAbove(50.0), 50, "超过 50ms 的有 50 帧");

        // 环形缓冲：灌满之后旧的被挤出去，只留最近 kCapacity 帧
        for (size_t i = 0; i < FrameStats::getCapacity(); i++)
            stats.record(0.001);
        checkEqualInt((int)stats.getCount(), (int)FrameStats::getCapacity(), "样本数被容量夹住");
        checkFloat((float)stats.getSummary().worstMs, 1.0f, "那批大帧已经被挤出去了");
        checkEqualInt((int)stats.getSummary().droppedCount, 0, "最近这些帧都没掉");

        stats.reset();
        checkEqualInt((int)stats.getCount(), 0, "reset 清空样本");
    }

    std::printf("\n== 2. 帧预算：状态机 ==\n");
    {
        budget::resetFrameBudget();
        check(!budget::isFrameActive(), "没开帧时 isFrameActive() = false");
        check(!budget::isExhausted(), "没开帧时不算用尽（不设限 = 老行为）");
        check(budget::getRemainingMs() > 1000.0, "没开帧时剩余量很大（调用方不用特判）");

        budget::beginFrame(0.0); // 0 = 不限制
        check(budget::isFrameActive(), "开了帧");
        check(!budget::isExhausted(), "预算 0 = 不限制，永不算用尽");
        budget::endFrame();
        check(!budget::isFrameActive(), "endFrame 之后帧结束");
        check(!budget::isExhausted(), "帧结束后也不再算用尽");

        budget::beginFrame(8.0);
        check(!budget::isExhausted(), "8ms 的预算刚开时还有余量");
        const double remaining = budget::getRemainingMs();
        check(remaining > 0.0 && remaining <= 8.0, "剩余量在 (0, 8] 之间");

        budget::beginFrame(0.0005); // 极小预算
        for (int i = 0; i < 500000 && !budget::isExhausted(); i++)
        {
        }
        check(budget::isExhausted(), "极小预算自旋之后一定用尽");
        checkEqualInt((int)budget::getDeferredCount(), 0, "没人上报时推迟计数为 0");
        budget::noteDeferred();
        budget::noteDeferred();
        checkEqualInt((int)budget::getDeferredCount(), 2, "noteDeferred() 累加");

        budget::endFrame();
        checkEqualInt((int)budget::getDeferredCount(), 2, "endFrame 之后还能读上一帧的推迟计数");
        check(!budget::isExhausted(), "endFrame 之后不再算用尽");
    }

    std::printf("\n== 3. 预算用尽时：主线程回调与延迟任务都推迟，且一个都不丢 ==\n");
    {
        std::vector<int> order;
        task::postToMain([&order] { order.push_back(1); });
        task::postToMain([&order] { order.push_back(2); });
        task::postToMain([&order] { order.push_back(3); });

        exhaustBudget();
        task::pumpMainThread();
        checkEqualInt((int)order.size(), 0, "预算用尽：三个回调一个都没跑（推到下一帧）");
        budget::endFrame();

        budget::beginFrame(-1.0); // 不限制
        task::pumpMainThread();
        checkEqualInt((int)order.size(), 3, "下一帧按原顺序全跑完（没丢）");
        check(order.size() == 3 && order[0] == 1 && order[1] == 2 && order[2] == 3,
            "顺序保持：先排的先跑");
        budget::endFrame();

        // 延迟任务同理
        int fired = 0;
        task::delay(0, [&fired] { fired++; }); // 立刻到期
        exhaustBudget();
        task::pumpMainThread();
        checkEqualInt(fired, 0, "预算用尽：到期了的延迟任务也推迟");
        budget::endFrame();

        budget::beginFrame(-1.0);
        task::pumpMainThread();
        checkEqualInt(fired, 1, "下一帧补跑（没有丢）");
        budget::endFrame();
    }

    std::printf("\n== 4. 全局重置：resetGlobals() 也把预算状态清掉 ==\n");
    {
        budget::beginFrame(8.0);
        check(budget::isFrameActive(), "先开一个帧");
        globals::resetGlobals();
        check(!budget::isFrameActive(), "resetGlobals() 之后帧预算不再是活动状态");
        check(!budget::isExhausted(), "也不再算用尽");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

// 无窗口测试：后台任务 + 主线程回调。
//
// 关键约定：work 在后台线程跑，done 一定在主线程（pumpMainThread）执行。

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "bklv/core/task.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 等条件成立（最多 timeoutMs），返回是否等到
template <typename Predicate>
bool waitFor(Predicate predicate, int timeoutMs = 2000)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

} // namespace

int main()
{
    std::printf("== 1. 后台执行 + 主线程回调 ==\n");
    {
        task::init(2);
        check(task::isMainThread(), "init 之后当前线程被认作主线程");

        std::atomic<bool> ranOnWorker{ false };
        std::atomic<bool> sameThreadAsMain{ false };
        std::atomic<int> callbacks{ 0 };

        const std::thread::id mainId = std::this_thread::get_id();

        task::run(
            [&] {
                // 后台线程：这里不该是主线程
                ranOnWorker = !task::isMainThread();
            },
            [&] {
                // 主线程回调：线程 id 必须和测试线程一致
                callbacks++;
                sameThreadAsMain = std::this_thread::get_id() == mainId;
            });

        check(waitFor([&] { return task::pendingCount() == 0; }), "任务跑完（pendingCount 归零）");
        check(ranOnWorker.load(), "work 在后台线程执行");
        checkEqualInt(callbacks.load(), 0, "回调还没 pump（必须由主线程驱动）");

        task::pumpMainThread();
        checkEqualInt(callbacks.load(), 1, "pump 之后回调执行一次");
        check(sameThreadAsMain.load(), "回调在主线程执行");
    }

    std::printf("\n== 2. 带返回值 ==\n");
    {
        std::string received;
        task::runResult(
            [] { return std::string("解码结果"); },
            [&](std::string value) { received = std::move(value); });

        check(waitFor([&] { return task::pendingCount() == 0; }), "任务跑完");
        task::pumpMainThread();
        checkEqual(received, "解码结果", "返回值被带到主线程回调里");
    }

    std::printf("\n== 3. 多个任务与顺序 ==\n");
    {
        std::atomic<int> counter{ 0 };
        constexpr int N = 8;

        task::init(0); // 0 会被夹到 1 个 worker，验证极端参数不炸
        for (int i = 0; i < N; i++)
            task::run([&] { counter++; }, nullptr);

        check(waitFor([&] { return task::pendingCount() == 0; }), "8 个任务都跑完");
        checkEqualInt(counter.load(), N, "计数正确");
    }

    std::printf("\n== 4. 工作线程里往回投 ==\n");
    {
        std::atomic<int> applied{ 0 };
        task::run([&] { task::postToMain([&] { applied++; }); }, nullptr);

        check(waitFor([&] { return task::pendingCount() == 0; }), "任务跑完");
        checkEqualInt(applied.load(), 0, "还没 pump");
        task::pumpMainThread();
        checkEqualInt(applied.load(), 1, "pump 后执行");
    }

    std::printf("\n== 5. 延迟任务（delay / cancel）==\n");
    {
        task::init(2);

        // 没到点不执行
        int fired = 0;
        task::DelayHandle handle = task::delay(50, [&fired] { fired++; });
        check(handle.isPending(), "刚创建时是挂起的");
        checkEqualSize(task::pendingDelayCount(), 1, "有一个挂起延迟");

        task::pumpMainThread();
        checkEqualInt(fired, 0, "没到时间不执行");

        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        task::pumpMainThread();
        checkEqualInt(fired, 1, "到时间后由 pump 执行");
        check(!handle.isPending(), "执行完不再挂起");

        // 0 毫秒：下一次 pump 就跑
        int immediate = 0;
        task::delay(0, [&immediate] { immediate++; });
        task::pumpMainThread();
        checkEqualInt(immediate, 1, "0ms 下一次 pump 执行");

        // fire-and-forget：句柄是临时对象也必须照常执行（析构不取消）
        int detached = 0;
        task::delay(0, [&detached] { detached++; });
        task::pumpMainThread();
        checkEqualInt(detached, 1, "不留句柄也会执行（析构不取消）");

        // 显式取消 + 移动语义
        int explicitCancel = 0;
        task::DelayHandle manual = task::delay(30, [&explicitCancel] { explicitCancel++; });
        manual.cancel();
        check(!manual.isPending(), "显式取消后不再挂起");
        task::pumpMainThread();
        checkEqualInt(explicitCancel, 0, "显式取消生效");

        int moved = 0;
        task::DelayHandle source = task::delay(30, [&moved] { moved++; });
        task::DelayHandle target = std::move(source);
        check(!source.isPending(), "被移动的句柄失效");
        check(target.isPending(), "新句柄接管");
        target.cancel();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        task::pumpMainThread();
        checkEqualInt(moved, 0, "移动后取消仍然有效");

        // 工作线程里发起延迟
        int fromWorker = 0;
        task::run([&fromWorker] { task::delay(0, [&fromWorker] { fromWorker++; }); }, nullptr);
        check(waitFor([&] { return task::pendingCount() == 0; }), "工作线程跑完");
        task::pumpMainThread();
        checkEqualInt(fromWorker, 1, "工作线程里发起的延迟也能在主线程执行");

        // 空回调不炸
        task::DelayHandle none = task::delay(10, nullptr);
        check(!none.isPending(), "空回调直接返回无效句柄");
    }

    std::printf("\n== 6. 关停安全 ==\n");
    {
        // 还有任务在排队时直接关停：不能崩，也不能再执行回调
        task::run([] { std::this_thread::sleep_for(std::chrono::milliseconds(20)); }, nullptr);
        // 还有挂起的延迟时关停：必须一起清掉
        int afterShutdown = 0;
        task::delay(20, [&afterShutdown] { afterShutdown++; });
        task::shutdown();
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        task::pumpMainThread();
        checkEqualInt(afterShutdown, 0, "关停后挂起的延迟不会执行");
        checkEqualSize(task::pendingDelayCount(), 0, "关停清空延迟队列");
        checkEqualSize(task::pendingCount(), 0, "关停后没有待执行任务");
        check(!task::isMainThread() == false, "主线程判定仍然可用");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

#include "bklv/foundation/core/task.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "bklv/foundation/core/frame_budget.hpp"
#include "bklv/foundation/core/logger.hpp"

namespace bklv
{

namespace task
{

namespace
{

struct Pool
{
    std::mutex mutex;
    std::condition_variable hasWork;
    std::deque<std::function<void()>> jobs;      // 待执行的后台任务
    std::vector<std::thread> workers;
    bool stopping = false;
    std::size_t running = 0;                     // 正在执行的任务数
};

// 已完成的回调（主线程消费）
std::mutex g_mainMutex;
std::deque<std::function<void()>> g_mainQueue;

// 延迟任务（主线程消费；到期时间按 steady_clock）
struct Delay
{
    size_t id = 0;
    std::chrono::steady_clock::time_point due;
    std::function<void()> callback;
};

std::mutex g_delayMutex;
std::vector<Delay> g_delays;
size_t g_nextDelayId = 1;

std::shared_ptr<Pool> g_pool;
std::thread::id g_mainThread;
std::atomic<bool> g_inited{ false };

void workerLoop(const std::shared_ptr<Pool>& pool)
{
    for (;;)
    {
        std::function<void()> job;

        {
            std::unique_lock<std::mutex> lock(pool->mutex);
            pool->hasWork.wait(lock, [&pool] { return pool->stopping || !pool->jobs.empty(); });

            if (pool->stopping && pool->jobs.empty())
                return;

            job = std::move(pool->jobs.front());
            pool->jobs.pop_front();
            pool->running++;
        }

        // 任务本身不该抛出去（抛了也只是这个 worker 少一次回调），兜一下
        try
        {
            if (job)
                job();
        }
        catch (const std::exception& e)
        {
            BKLV_LOG_ERROR("后台任务抛出异常：%s", e.what());
        }
        catch (...)
        {
            BKLV_LOG_ERROR("后台任务抛出未知异常");
        }

        {
            std::lock_guard<std::mutex> lock(pool->mutex);
            pool->running--;
        }
    }
}

} // namespace

void init(std::size_t threads)
{
    if (g_inited.exchange(true))
        return;

    g_mainThread = std::this_thread::get_id();

    g_pool        = std::make_shared<Pool>();
    const size_t count = threads > 0 ? threads : 1;
    g_pool->workers.reserve(count);
    for (size_t i = 0; i < count; i++)
    {
        std::shared_ptr<Pool> pool = g_pool;
        g_pool->workers.emplace_back([pool] { workerLoop(pool); });
    }
}

void shutdown()
{
    if (!g_inited.exchange(false))
        return;

    std::shared_ptr<Pool> pool = g_pool;
    g_pool.reset();

    if (pool != nullptr)
    {
        {
            std::lock_guard<std::mutex> lock(pool->mutex);
            pool->stopping = true;
        }
        pool->hasWork.notify_all();

        for (std::thread& worker : pool->workers)
            if (worker.joinable())
                worker.join();
    }

    std::lock_guard<std::mutex> lock(g_mainMutex);
    g_mainQueue.clear();

    std::lock_guard<std::mutex> delayLock(g_delayMutex);
    g_delays.clear();
}

void run(std::function<void()> work, std::function<void()> done)
{
    if (work == nullptr)
        return;

    init();

    std::function<void()> job = std::move(work);
    if (done != nullptr)
    {
        std::function<void()> finish = std::move(done);
        job = [job = std::move(job), finish = std::move(finish)]() mutable {
            job();
            postToMain(std::move(finish));
        };
    }

    std::shared_ptr<Pool> pool = g_pool;
    if (pool == nullptr)
        return;

    {
        std::lock_guard<std::mutex> lock(pool->mutex);
        if (pool->stopping)
            return;
        pool->jobs.push_back(std::move(job));
    }
    pool->hasWork.notify_one();
}

void postToMain(std::function<void()> callback)
{
    if (callback == nullptr)
        return;

    std::lock_guard<std::mutex> lock(g_mainMutex);
    g_mainQueue.push_back(std::move(callback));
}

void pumpMainThread()
{
    // 1. 先跑到期的延迟任务（对标 borealis 在 performSyncTasks 里处理 delay：
    //    但这里不复制整个队列、也不每帧回填，只把到期的摘出来）
    {
        const auto now = std::chrono::steady_clock::now();
        std::vector<Delay> due;

        {
            std::lock_guard<std::mutex> lock(g_delayMutex);
            for (auto it = g_delays.begin(); it != g_delays.end();)
            {
                if (it->due <= now)
                {
                    due.push_back(std::move(*it));
                    it = g_delays.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        size_t executed = 0;
        for (Delay& item : due)
        {
            if (item.callback == nullptr)
            {
                executed++;
                continue;
            }

            // 帧预算用完了就停手：剩下的下一帧继续（它们已经到期，下一帧立刻会被捡起来）
            if (budget::isExhausted())
                break;

            try
            {
                item.callback();
            }
            catch (const std::exception& e)
            {
                BKLV_LOG_ERROR("延迟任务抛出异常：%s", e.what());
            }
            catch (...)
            {
                BKLV_LOG_ERROR("延迟任务抛出未知异常");
            }
            executed++;
        }

        // 没轮到的放回延迟表里，别丢（到期时间已经过了，下一帧一定会再被摘出来）
        if (executed < due.size())
        {
            budget::noteDeferred();
            std::lock_guard<std::mutex> lock(g_delayMutex);
            for (size_t i = executed; i < due.size(); i++)
                g_delays.push_back(std::move(due[i]));
        }
    }

    // 2. 再跑投递到主线程的回调（一次把这批取出来执行，执行期间新投进来的留到下一帧；
    //    预算用完就把没跑的放回队列**头部** —— 它们本来就是更早排进来的）
    std::deque<std::function<void()>> batch;
    {
        std::lock_guard<std::mutex> lock(g_mainMutex);
        batch.swap(g_mainQueue);
    }

    for (size_t i = 0; i < batch.size(); i++)
    {
        std::function<void()>& callback = batch[i];
        if (callback == nullptr)
            continue;

        if (budget::isExhausted())
        {
            // 剩下的（含当前这个）放回队首，下一帧先跑它们
            budget::noteDeferred();
            std::lock_guard<std::mutex> lock(g_mainMutex);
            g_mainQueue.insert(g_mainQueue.begin(),
                std::make_move_iterator(batch.begin() + (long)i),
                std::make_move_iterator(batch.end()));
            break;
        }

        try
        {
            callback();
        }
        catch (const std::exception& e)
        {
            BKLV_LOG_ERROR("主线程回调抛出异常：%s", e.what());
        }
        catch (...)
        {
            BKLV_LOG_ERROR("主线程回调抛出未知异常");
        }
    }
}

DelayHandle delay(int delayMs, std::function<void()> callback)
{
    if (callback == nullptr)
        return DelayHandle();

    Delay item;
    item.due      = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(delayMs < 0 ? 0 : delayMs);
    item.callback = std::move(callback);

    std::lock_guard<std::mutex> lock(g_delayMutex);
    item.id = g_nextDelayId++;
    const size_t id = item.id;
    g_delays.push_back(std::move(item));
    return DelayHandle(id);
}

void DelayHandle::cancel()
{
    if (this->id == 0)
        return;

    const size_t target = this->id;
    this->id = 0;

    std::lock_guard<std::mutex> lock(g_delayMutex);
    for (auto it = g_delays.begin(); it != g_delays.end(); ++it)
    {
        if (it->id == target)
        {
            g_delays.erase(it);
            break;
        }
    }
}

bool DelayHandle::isPending() const
{
    if (this->id == 0)
        return false;

    std::lock_guard<std::mutex> lock(g_delayMutex);
    for (const Delay& item : g_delays)
        if (item.id == this->id)
            return true;

    return false;
}

std::size_t pendingDelayCount()
{
    std::lock_guard<std::mutex> lock(g_delayMutex);
    return g_delays.size();
}

std::size_t pendingCount()
{
    std::shared_ptr<Pool> pool = g_pool;
    if (pool == nullptr)
        return 0;

    std::lock_guard<std::mutex> lock(pool->mutex);
    return pool->jobs.size() + pool->running;
}

bool isMainThread()
{
    return std::this_thread::get_id() == g_mainThread;
}

} // namespace task

} // namespace bklv

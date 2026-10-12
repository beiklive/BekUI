#include "bklv/core/task.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "bklv/core/logger.hpp"

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
    // 一次把这批回调取出来执行（执行期间新投进来的留到下一帧）
    std::deque<std::function<void()>> batch;
    {
        std::lock_guard<std::mutex> lock(g_mainMutex);
        batch.swap(g_mainQueue);
    }

    for (std::function<void()>& callback : batch)
    {
        if (callback == nullptr)
            continue;

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

#pragma once

// 后台任务 + 主线程回调。
//
// 对应 borealis 的 core/task.hpp / core/thread.hpp（它用 brls::async + ASYNC_RETAIN 宏）。
// 差别：
// - borealis 的 async 靠宏手写引用计数、每次调用自己管线程；这里是**固定大小的线程池**
//   （默认 2 个 worker），任务与回调都是 std::function，没有宏。
// - **回调一定在主线程执行**（由 App::frame 每帧 pump 一次），所以回调里可以安全碰视图、
//   纹理与 ImGui；后台线程只做纯 CPU / IO 的活。
// - 关停是显式的（shutdown()），worker 持有共享状态，join 之后才退出，析构安全。
//
// 典型用法（异步加载图片：文件 IO + 解码在后台，纹理上传在主线程）：
//
//     task::runResult(
//         [path] { return decodeImageFile(path.c_str()); },      // 后台
//         [this, path](DecodedImage image) { this->upload(image); }); // 主线程

#include <cstddef>
#include <functional>
#include <utility>

namespace bklv
{

namespace task
{

// 启动线程池（默认 2 个 worker，重复调用只生效一次；首次 run 会自动调）
void init(std::size_t threads = 2);

// 停止并等待 worker 退出（App 析构时调）
void shutdown();

// work 在后台线程执行；done 非空时在**主线程**（下一帧 pump）执行
void run(std::function<void()> work, std::function<void()> done = nullptr);

// work 有返回值：返回值会被移动到主线程的 done 里
template <typename Work, typename Done>
void runResult(Work work, Done done)
{
    using Result = decltype(work());

    run(
        [work = std::move(work), done = std::move(done)]() mutable {
            Result value = work();
            postToMain([done = std::move(done), value = std::move(value)]() mutable {
                done(std::move(value));
            });
        },
        nullptr);
}

// 把一段代码投回主线程（工作线程里调用也安全）
void postToMain(std::function<void()> callback);

// 延迟句柄：显式 cancel() 用（对标 borealis 的 cancelDelay(iter)）。移动语义，不可拷贝。
//
// **析构不取消**：`task::delay(1000, [] { ... });` 这种 fire-and-forget 写法要能生效——
// 一开始做成"析构即取消"时，临时句柄在语句结束就被销毁，回调静默不触发（测试抓到的）。
// 如果要"对象死了就别回调"，在回调里捕获 AliveFlag 的句柄（见 core/alive.hpp），
// 和异步任务的回调用同一套护栏。
class DelayHandle
{
  public:
    DelayHandle() = default;
    DelayHandle(size_t id)
        : id(id)
    {
    }

    DelayHandle(const DelayHandle&) = delete;
    DelayHandle& operator=(const DelayHandle&) = delete;

    DelayHandle(DelayHandle&& other) noexcept
        : id(other.id)
    {
        other.id = 0;
    }

    DelayHandle& operator=(DelayHandle&& other) noexcept
    {
        if (this != &other)
        {
            this->id = other.id;
            other.id = 0;
        }
        return *this;
    }

    // 取消这次延迟（已经执行过或已取消则是空操作）
    void cancel();
    bool isPending() const;

  private:
    size_t id = 0;
};

// 延迟 delayMs 毫秒后在**主线程**执行 callback（对标 borealis 的 brls::delay + cancelDelay）。
//
// 与 borealis 的差别：不占线程、也不用每帧复制整个队列——到期时间排在一个表里，
// 由 pumpMainThread()（App::frame 每帧）检查一遍；取消用 RAII 句柄，不用记 iter 编号。
// 时间基准是 steady_clock（真实毫秒），和补间用的帧时间不是一回事。
DelayHandle delay(int delayMs, std::function<void()> callback);

// 由 App::frame 每帧调用一次：执行已排到主线程的回调
void pumpMainThread();

// 还没跑完的任务数（排队 + 正在执行）
std::size_t pendingCount();

// 还没到期的延迟任务数
std::size_t pendingDelayCount();

// 当前线程是不是主线程（init 时记录）
bool isMainThread();

} // namespace task

} // namespace bklv

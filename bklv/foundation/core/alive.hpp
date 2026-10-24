#pragma once

// "我还活着"标记：给异步回调用的生命周期护栏。
//
// 为什么需要：`task::run*` 的回调在**主线程**执行，但发起时捕获的 `this`
// 可能在这期间被销毁（换页/重建视图树/回收器释放）。直接捕获裸 this 就是
// use-after-free —— borealis 用 ASYNC_RETAIN / ASYNC_RELEASE 三个宏手写引用计数
// 来防这件事；这里换成一个小对象：持有者析构时 shared_ptr 释放，回调里的
// weak_ptr 立刻过期，直接 return。
//
// 用法：
//     class Foo {
//         AliveFlag alive;                       // 成员
//         void load() {
//             auto guard = this->alive.handle();  // weak_ptr
//             task::run([...]{ ... }, [this, guard]{ if (!guard->isAlive()) return; ... });
//         }
//     };

#include <memory>

namespace bklv
{

class AliveFlag
{
  public:
    struct Flag
    {
        bool alive = true;
        bool isAlive() const { return this->alive; }
    };

    AliveFlag()
        : flag(std::make_shared<Flag>())
    {
    }

    // 拷贝/移动都保持"共享同一个标记"的语义（默认行为即可）
    AliveFlag(const AliveFlag&) = default;
    AliveFlag& operator=(const AliveFlag&) = default;

    // 析构：标记失效（回调里 isAlive() 变 false）
    ~AliveFlag()
    {
        if (this->flag != nullptr)
            this->flag->alive = false;
    }

    bool isAlive() const { return this->flag != nullptr && this->flag->alive; }

    // 交给异步回调的句柄（弱引用，不会延长持有者寿命）
    std::weak_ptr<const Flag> handle() const { return this->flag; }

    // 显式作废（想在析构之前就停掉回调时用）
    void invalidate()
    {
        if (this->flag != nullptr)
            this->flag->alive = false;
    }

  private:
    std::shared_ptr<Flag> flag;
};

} // namespace bklv

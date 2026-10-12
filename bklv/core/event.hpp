#pragma once

// 事件总线。对标 borealis 的 core/event.hpp（它那边只有点击事件，且靠全局 Application 派发），
// 这里做成通用的小总线：按**事件类型**订阅，焦点/点击等内置事件由框架自己发。
//
// 两条设计取舍：
// - **不用 typeid / std::type_index**：Switch 构建带 `-fno-rtti`。改用"每个事件类型一个
//   静态 token 变量，取它的地址当 key"这个经典做法，效果一样且零 RTTI。
// - **订阅用 RAII 句柄**：`Subscription` 析构自动退订，不用记着手动 unsub；
//   视图/页面销毁时不会留下悬空回调（这块以前在别处踩过野指针）。

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace bklv
{

class EventBus;
class View;

// 事件类型的静态 token：同类型共用一个地址，拿地址当 key
template <typename Event>
struct EventToken
{
    static char id;
};

template <typename Event>
char EventToken<Event>::id = 0;

// 订阅句柄：析构即退订（可手动 unsubscribe）
class Subscription
{
  public:
    Subscription() = default;
    Subscription(EventBus* bus, const void* token, size_t id)
        : bus(bus)
        , token(token)
        , id(id)
    {
    }

    ~Subscription() { this->unsubscribe(); }

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept
        : bus(other.bus)
        , token(other.token)
        , id(other.id)
    {
        other.bus = nullptr;
    }

    Subscription& operator=(Subscription&& other) noexcept
    {
        if (this != &other)
        {
            this->unsubscribe();
            this->bus   = other.bus;
            this->token = other.token;
            this->id    = other.id;
            other.bus   = nullptr;
        }
        return *this;
    }

    void unsubscribe();
    bool isActive() const { return this->bus != nullptr; }

  private:
    EventBus* bus = nullptr;
    const void* token = nullptr;
    size_t id = 0;
};

class EventBus
{
  public:
    // 订阅某类事件；返回 RAII 句柄（可以不管它，析构自动退订）
    template <typename Event, typename Handler>
    Subscription subscribe(Handler handler)
    {
        const void* token = &EventToken<Event>::id;
        const size_t id   = this->nextId++;

        Entry entry;
        entry.id      = id;
        entry.handler = [handler = std::move(handler)](const void* payload) {
            handler(*static_cast<const Event*>(payload));
        };

        this->handlers[token].push_back(std::move(entry));
        return Subscription(this, token, id);
    }

    // 发事件。派发期间新增/退订都不会破坏遍历（内部对快照迭代）
    template <typename Event>
    void publish(const Event& event)
    {
        const void* token = &EventToken<Event>::id;
        auto it           = this->handlers.find(token);
        if (it == this->handlers.end())
            return;

        // 快照：回调里订阅/退订都安全
        const std::vector<Entry> snapshot = it->second;
        for (const Entry& entry : snapshot)
        {
            if (this->isStillSubscribed(token, entry.id) && entry.handler != nullptr)
                entry.handler(&event);
        }
    }

    template <typename Event>
    size_t getSubscriberCount() const
    {
        const void* token = &EventToken<Event>::id;
        auto it           = this->handlers.find(token);
        return it == this->handlers.end() ? 0 : it->second.size();
    }

    size_t getTotalSubscriberCount() const;

    // 退订全部（换根视图/关页面时用，避免留下悬空回调）
    void clear();

  private:
    struct Entry
    {
        size_t id = 0;
        std::function<void(const void*)> handler;
    };

    void remove(const void* token, size_t id);
    bool isStillSubscribed(const void* token, size_t id) const;

    std::map<const void*, std::vector<Entry>> handlers;
    size_t nextId = 1;

    friend class Subscription;
};

// 进程级总线（App 构造时清一次；也可以自己 new 一个隔离的）
EventBus& getEventBus();

// ---- 框架内置事件 ----

// 焦点变化（标题栏/状态栏这类全局 UI 用它做联动）
struct FocusChangedEvent
{
    View* previous = nullptr;
    View* current  = nullptr;
};

// 动作触发（A/B/点击都算，pointer 区分鼠标）
struct ActionTriggeredEvent
{
    View* view = nullptr;
    int button = 0;      // ControllerButton 的值；用 int 避免这里依赖 input.hpp
    bool pointer = false;
};

} // namespace bklv

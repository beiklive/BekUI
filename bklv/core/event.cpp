#include "bklv/core/event.hpp"

#include "bklv/core/globals.hpp"

namespace bklv
{

void Subscription::unsubscribe()
{
    if (this->bus == nullptr)
        return;

    this->bus->remove(this->token, this->id);
    this->bus = nullptr;
}

void EventBus::remove(const void* token, size_t id)
{
    auto it = this->handlers.find(token);
    if (it == this->handlers.end())
        return;

    std::vector<Entry>& list = it->second;
    for (auto entry = list.begin(); entry != list.end(); ++entry)
    {
        if (entry->id == id)
        {
            list.erase(entry);
            break;
        }
    }

    if (list.empty())
        this->handlers.erase(it);
}

bool EventBus::isStillSubscribed(const void* token, size_t id) const
{
    auto it = this->handlers.find(token);
    if (it == this->handlers.end())
        return false;

    for (const Entry& entry : it->second)
        if (entry.id == id)
            return true;

    return false;
}

size_t EventBus::getTotalSubscriberCount() const
{
    size_t total = 0;
    for (const auto& pair : this->handlers)
        total += pair.second.size();
    return total;
}

void EventBus::clear()
{
    this->handlers.clear();
}

EventBus& getEventBus()
{
    // 登记在全局表里（core/globals.hpp）：可列举、可随 resetGlobals() 一起重置。
    // 重置之后拿到的是**新对象**，旧订阅自然作废 —— 这是"换一个 App 从干净状态开始"的前提。
    return *globals::getGlobalStore().obtain<EventBus>("eventBus");
}

} // namespace bklv

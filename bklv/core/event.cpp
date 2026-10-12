#include "bklv/core/event.hpp"

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
    static EventBus bus;
    return bus;
}

} // namespace bklv

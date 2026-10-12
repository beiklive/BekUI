#pragma once

// LRU 缓存。对标 borealis 的 core/cache_helper.hpp（它是"引用计数 + 脏标记 + LRU"）。
//
// 与 borealis 的差别：
// - 接口收成三个动作：`get(key, loader)` / `invalidate(key)` / `Pin`（把条目钉住不被淘汰），
//   不暴露内部 Node，也不需要调用方自己数引用。
// - **纯数据、不碰 ImGui**，所以能无窗口测（命中/未命中计数、淘汰顺序、钉住语义都有测试）。

#include <cstddef>
#include <functional>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace bklv
{

template <typename Key, typename Value>
class LruCache
{
  public:
    explicit LruCache(size_t capacity = 64)
        : capacity(capacity == 0 ? 1 : capacity)
    {
    }

    // 取一个值：命中就直接返回，没命中就调 loader 生成并放进缓存。
    // loader 返回的值会被缓存；条目被标脏后下一次 get 会重新调 loader。
    Value get(const Key& key, const std::function<Value()>& loader)
    {
        auto it = this->index.find(key);
        if (it != this->index.end() && !this->entries[it->second].dirty)
        {
            this->hits++;
            // 命中：挪到最前（最近使用）
            this->order.splice(this->order.begin(), this->order, this->entries[it->second].orderIt);
            this->entries[it->second].orderIt = this->order.begin();
            return this->entries[it->second].value;
        }

        this->misses++;

        if (loader == nullptr)
            throw std::runtime_error("LruCache::get 需要 loader");

        Value value = loader();

        if (it != this->index.end())
        {
            // 脏条目：就地刷新
            this->entries[it->second].value = value;
            this->entries[it->second].dirty = false;
            return value;
        }

        this->insert(key, value);
        return value;
    }

    // 名字用 has 而不是 contains：本项目的查询式命名统一 is*/has*/can*
    // （contains 是 STL 的名字，只有标准库成员才照旧）
    bool has(const Key& key) const
    {
        auto it = this->entries.find(key);
        return it != this->entries.end() && !it->second.dirty;
    }

    // 标脏：条目还在，但下次 get 会重新生成（近似 borealis 的 dirty 语义）
    void invalidate(const Key& key)
    {
        auto it = this->index.find(key);
        if (it != this->index.end())
            this->entries[it->second].dirty = true;
    }

    void invalidateAll()
    {
        for (auto& entry : this->entries)
            entry.second.dirty = true;
    }

    // 直接塞一个值（不进 loader）
    void put(const Key& key, Value value)
    {
        auto it = this->index.find(key);
        if (it != this->index.end())
        {
            this->entries[it->second].value = std::move(value);
            this->entries[it->second].dirty = false;
            this->order.splice(this->order.begin(), this->order, this->entries[it->second].orderIt);
            this->entries[it->second].orderIt = this->order.begin();
            return;
        }

        this->insert(key, std::move(value));
    }

    // 把条目钉住：被钉住的条目不会被淘汰（对标 borealis 的引用计数 > 0 不过期）
    class Pin
    {
      public:
        Pin() = default;
        Pin(LruCache* cache, const Key& key)
            : cache(cache)
            , key(key)
        {
            if (this->cache != nullptr)
                this->cache->pinCount[key]++;
        }

        ~Pin() { this->release(); }

        Pin(const Pin&) = delete;
        Pin& operator=(const Pin&) = delete;

        Pin(Pin&& other) noexcept
            : cache(other.cache)
            , key(other.key)
        {
            other.cache = nullptr;
        }

        Pin& operator=(Pin&& other) noexcept
        {
            if (this != &other)
            {
                this->release();
                this->cache = other.cache;
                this->key   = other.key;
                other.cache = nullptr;
            }
            return *this;
        }

        bool isPinned() const { return this->cache != nullptr; }

      private:
        void release()
        {
            if (this->cache == nullptr)
                return;

            auto it = this->cache->pinCount.find(this->key);
            if (it != this->cache->pinCount.end())
            {
                if (--it->second == 0)
                    this->cache->pinCount.erase(it);
            }
            this->cache = nullptr;
        }

        LruCache* cache = nullptr;
        Key key{};
    };

    Pin pin(const Key& key) { return Pin(this, key); }

    void setCapacity(size_t value)
    {
        this->capacity = value == 0 ? 1 : value;
        this->evictIfNeeded();
    }

    size_t getCapacity() const { return this->capacity; }
    size_t size() const { return this->entries.size(); }
    size_t getHitCount() const { return this->hits; }
    size_t getMissCount() const { return this->misses; }

    void clear()
    {
        this->entries.clear();
        this->index.clear();
        this->order.clear();
        this->pinCount.clear();
        this->hits   = 0;
        this->misses = 0;
    }

  private:
    struct Entry
    {
        Value value;
        bool dirty = false;
        typename std::list<Key>::iterator orderIt;
    };

    void insert(const Key& key, Value value)
    {
        this->order.push_front(key);
        Entry entry;
        entry.value   = std::move(value);
        entry.orderIt = this->order.begin();
        this->entries.emplace(key, std::move(entry));
        this->index[key] = key;
        this->evictIfNeeded();
    }

    void evictIfNeeded()
    {
        // 从最久未用的一端淘汰；被钉住的跳过（挪到最近端，避免每次都从它开始扫）
        while (this->entries.size() > this->capacity)
        {
            bool evicted = false;

            for (auto it = this->order.rbegin(); it != this->order.rend(); ++it)
            {
                if (this->pinCount.count(*it) > 0)
                    continue;

                const Key key = *it;
                this->order.erase(std::next(it).base());
                this->entries.erase(key);
                this->index.erase(key);
                evicted = true;
                break;
            }

            if (!evicted)
                break; // 全被钉住，不能再淘汰
        }
    }

    size_t capacity;
    std::list<Key> order;                 // 最近使用在最前
    std::unordered_map<Key, Entry> entries;
    std::unordered_map<Key, Key> index;    // 只为 contains/get 少一次哈希查找
    std::unordered_map<Key, size_t> pinCount;
    size_t hits   = 0;
    size_t misses = 0;
};

} // namespace bklv

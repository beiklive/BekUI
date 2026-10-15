#include "bklv/core/decode_queue.hpp"

#include <algorithm>
#include <map>
#include <vector>

#include "bklv/core/graphics.hpp"

namespace bklv
{

namespace
{

struct Pending
{
    int priority = 0;
    std::function<void()> start;
};

// key -> 等待启动的解码
std::map<std::string, Pending> g_pending;
// 正在飞的 key
std::vector<std::string> g_inFlight;
// 已完成、待上传的 key -> 上传动作
std::map<std::string, std::function<void()>> g_uploads;

} // namespace

void enqueueDecode(const std::string& key, int priority, std::function<void()> start)
{
    if (key.empty() || !start)
        return;
    if (g_pending.count(key) > 0
        || std::find(g_inFlight.begin(), g_inFlight.end(), key) != g_inFlight.end())
        return; // 同一张图不重复解
    g_pending[key] = Pending { priority, std::move(start) };
}

void pumpDecodeQueue()
{
    const size_t limit = getDecodeConcurrency(); // 0 = 不限
    while (!g_pending.empty() && (limit == 0 || g_inFlight.size() < limit))
    {
        // 取优先级最高（数字最小）的一个
        auto best = g_pending.begin();
        for (auto it = g_pending.begin(); it != g_pending.end(); ++it)
            if (it->second.priority < best->second.priority)
                best = it;

        const std::string key = best->first;
        std::function<void()> start = std::move(best->second.start);
        g_pending.erase(best);
        g_inFlight.push_back(key);
        if (start)
            start();
    }
}

void notifyDecodeFinished(const std::string& key, std::function<void()> upload)
{
    const auto it = std::find(g_inFlight.begin(), g_inFlight.end(), key);
    if (it != g_inFlight.end())
        g_inFlight.erase(it);
    if (!key.empty() && upload)
        g_uploads[key] = std::move(upload);
}

void pumpUploads()
{
    const size_t quota = getMaxUploadsPerFrame(); // 0 = 不限
    size_t done = 0;
    while (!g_uploads.empty() && (quota == 0 || done < quota))
    {
        auto it = g_uploads.begin();
        std::function<void()> upload = std::move(it->second);
        g_uploads.erase(it);
        if (upload)
            upload();
        ++done;
    }
}

void cancelDecode(const std::string& key)
{
    g_pending.erase(key);
    g_uploads.erase(key);
    const auto it = std::find(g_inFlight.begin(), g_inFlight.end(), key);
    if (it != g_inFlight.end())
        g_inFlight.erase(it);
}

size_t getPendingDecodeCount() { return g_pending.size(); }
size_t getInFlightDecodeCount() { return g_inFlight.size(); }
size_t getPendingUploadCount() { return g_uploads.size(); }

void clearDecodeQueueForTest()
{
    g_pending.clear();
    g_inFlight.clear();
    g_uploads.clear();
}

} // namespace bklv

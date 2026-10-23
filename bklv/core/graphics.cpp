// 渲染后端注册表 + 纹理管理。
//
// 后端由编译期决定（见 core/platform.hpp 里各平台的 createBackend），
// 这里只负责把解码后的像素交给当前后端上传，并管理句柄生命周期。

#include "bklv/core/graphics.hpp"

#include <map>
#include <string>
#include <vector>

#include "bklv/core/image_decoder.hpp"

#include <map>
#include <string>
#include <vector>

#include "bklv/core/frame_budget.hpp"
#include "bklv/core/globals.hpp"
#include "bklv/core/logger.hpp"

namespace bklv
{

// ---------------------------------------------------------------------------
// 纹理管理
// ---------------------------------------------------------------------------

namespace
{

TextureBackend* g_backend = nullptr;

struct TextureEntry
{
    TextureHandle handle;
    bool alive = false;
    size_t refCount      = 0; // 还有几个使用者（小步2：refCount>0 的不许淘汰）
    uint64_t lastUseTick = 0; // LRU 记账
};

// 句柄表。存 TextureHandle 而不是裸 ImTextureID，
// 这样释放之后能把条目置为 dead，外部拿着的引用会失效而不是变成野指针。
std::vector<TextureEntry> g_textures;

// 纹理预算（0 = 不限）；只做账目，不淘汰
size_t g_textureBudget = 0;

// 路径 -> 索引，做加载缓存。
std::map<std::string, int> g_pathCache;

// LRU 记账（小步2 的前半）：路径 -> 最后一次使用的序号。只记录，不淘汰。
// 背压旋钮（0 = 不限）；下一步的 enqueueDecode 队列才会读它们
size_t g_decodeConcurrency  = 4;
size_t g_maxUploadsPerFrame = 2;

uint64_t g_useTick = 0;
std::map<std::string, uint64_t> g_pathLastUse;

[[maybe_unused]] int registerTexture(const TextureHandle& handle)
{
    // 复用已经释放的位置，避免长跑时数组无限增长。
    for (size_t i = 0; i < g_textures.size(); ++i)
    {
        if (!g_textures[i].alive)
        {
            g_textures[i].handle = handle;
            g_textures[i].alive  = true;
            return static_cast<int>(i);
        }
    }

    g_textures.push_back({ handle, true });
    return static_cast<int>(g_textures.size()) - 1;
}

[[maybe_unused]] TextureRef makeRef(int index)
{
    TextureRef ref;
    if (index < 0 || index >= static_cast<int>(g_textures.size()) || !g_textures[index].alive)
        return ref;

    const TextureHandle& handle = g_textures[index].handle;
    ref.index   = index;
    ref.texture = handle.texture;
    ref.width   = handle.width;
    ref.height  = handle.height;
    return ref;
}

} // namespace

namespace
{

// resetGlobals() 用的收尾钩子（定义在 shutdownTextureStore 之后）
void resetTextureStoreForGlobals();

} // namespace

bool isTextureStoreReady()
{
    return g_backend != nullptr;
}

namespace
{
int g_textureLeases = 0;
}

void acquireTextureStore(TextureBackend* backend)
{
    if (backend == nullptr)
        return;
    if (g_textureLeases <= 0)
        initTextureStore(backend);
    else if (g_backend != backend)
        BKLV_LOG_WARNING("纹理仓库已由另一个后端初始化；多 Context 需共用同一 GPU 设备");
    g_textureLeases++;
}

void releaseTextureStore(TextureBackend* backend)
{
    (void)backend;
    if (g_textureLeases <= 0)
        return;
    g_textureLeases--;
    if (g_textureLeases == 0)
        shutdownTextureStore();
}

void initTextureStore(TextureBackend* backend)
{
    // 换后端时缓存全部作废：旧句柄属于上一个后端，不能再拿来用；
    // 之前的失败记录也要清掉，否则新后端明明可用却会被旧的失败标记挡住。
    if (g_backend != backend)
        g_pathCache.clear();

    g_backend = backend;

    // 纹理仓库的状态是文件级 static（映射表 + 句柄表），搬不进全局表；
    // 按 core/globals.hpp 的约定登记一个重置钩子，resetGlobals() 时会被调到。
    // 注意语义：shutdownTextureStore() 要在后端销毁上下文**之前**跑（resetGlobals 由宿主决定时机）。
    globals::getGlobalStore().addResetHook("textureStore", &resetTextureStoreForGlobals);
}

void shutdownTextureStore()
{
    // 必须在后端销毁上下文之前调用。
    for (size_t i = 0; i < g_textures.size(); ++i)
    {
        if (!g_textures[i].alive)
            continue;

        if (g_backend != nullptr)
            g_backend->destroyTexture(g_textures[i].handle);
        g_textures[i].alive = false;
    }

    g_textures.clear();
    g_pathCache.clear();
}

void trimTextureCache(); // 前置声明（实现在下方）

namespace
{

// resetGlobals() 用的收尾钩子：除了清表，还把后端指针摘掉 ——
// 只清表的话 isTextureStoreReady() 在"后端已经销毁"之后仍然报 true（老问题，一起收掉）。
void resetTextureStoreForGlobals()
{
    shutdownTextureStore();
    g_backend = nullptr;
    g_textureLeases = 0;
}

} // namespace

void setTextureBudget(size_t maxTextures)
{
    g_textureBudget = maxTextures;
    trimTextureCache(); // 预算变小后立刻收一次（预算 0 = 不限，直接返回）
    if (maxTextures > 0 && getTextureCount() > maxTextures)
        BKLV_LOG_WARNING("纹理数 %zu 已超过预算 %zu（本步只记账不淘汰，见 docs §11.4）",
            getTextureCount(), maxTextures);
}

size_t getTextureBudget()
{
    return g_textureBudget;
}

size_t getTextureCount()
{
    size_t count = 0;
    for (const TextureEntry& entry : g_textures)
        if (entry.alive)
            ++count;
    return count;
}

bool isTextureBudgetExceeded()
{
    return g_textureBudget > 0 && getTextureCount() > g_textureBudget;
}

size_t getTextureCacheCount()
{
    return g_pathCache.size();
}

void retainTexture(size_t index)
{
    if (index >= g_textures.size() || !g_textures[index].alive)
        return; // 越界/已释放：安全 no-op
    ++g_textures[index].refCount;
    g_textures[index].lastUseTick = ++g_useTick;
}

void releaseTexture(size_t index)
{
    if (index >= g_textures.size() || !g_textures[index].alive)
        return;
    if (g_textures[index].refCount == 0)
    {
        BKLV_LOG_WARNING("纹理 %zu 的引用计数已经是 0，release 被忽略（计数不会下溢）", index);
        return;
    }
    --g_textures[index].refCount;
}

size_t getTextureRefCount(size_t index)
{
    if (index >= g_textures.size() || !g_textures[index].alive)
        return 0;
    return g_textures[index].refCount;
}

void setDecodeConcurrency(size_t maxConcurrentDecodes)
{
    g_decodeConcurrency = maxConcurrentDecodes;
}

size_t getDecodeConcurrency()
{
    return g_decodeConcurrency;
}

void setMaxUploadsPerFrame(size_t maxUploads)
{
    g_maxUploadsPerFrame = maxUploads;
}

size_t getMaxUploadsPerFrame()
{
    return g_maxUploadsPerFrame;
}

void trimTextureCache()
{
    if (g_textureBudget == 0 || getTextureCount() <= g_textureBudget)
        return; // 不限 / 未超预算：不遍历

    while (getTextureCount() > g_textureBudget)
    {
        // 帧预算用完就停手：剩下的下一帧继续（App::frame 每帧会再调一次）。
        // 每次淘汰都要调后端的 destroyTexture，是实打实的主线程开销。
        if (budget::isExhausted())
        {
            budget::noteDeferred();
            break;
        }

        // 找最旧的、且没人在用的那张
        size_t victim = (size_t)-1;
        uint64_t oldest = 0;
        for (size_t i = 0; i < g_textures.size(); ++i)
        {
            if (!g_textures[i].alive || g_textures[i].refCount > 0)
                continue;
            if (victim == (size_t)-1 || g_textures[i].lastUseTick < oldest)
            {
                victim = i;
                oldest = g_textures[i].lastUseTick;
            }
        }
        if (victim == (size_t)-1)
            break; // 全都在用：停，不硬淘汰

        if (g_backend != nullptr)
            g_backend->destroyTexture(g_textures[victim].handle);
        BKLV_LOG_INFO("纹理淘汰：释放下标 %zu（当前 %zu 张 / 预算 %zu）",
            victim, getTextureCount() - 1, g_textureBudget);
        g_textures[victim].alive    = false;
        g_textures[victim].refCount = 0;

        // 同步清掉指向它的路径记录（否则下次命中会拿到已释放的下标）
        for (auto it = g_pathCache.begin(); it != g_pathCache.end();)
        {
            if (it->second == (int)victim)
            {
                g_pathLastUse.erase(it->first);
                it = g_pathCache.erase(it);
            }
            else
                ++it;
        }
    }
}

const std::string& getLeastRecentlyUsedPath()
{
    static const std::string empty;
    const std::string* best = nullptr;
    uint64_t oldest = 0;
    for (const auto& entry : g_pathLastUse)
    {
        if (best == nullptr || entry.second < oldest)
        {
            best   = &entry.first;
            oldest = entry.second;
        }
    }
    return best != nullptr ? *best : empty;
}

const char* getTextureBackendName()
{
    return g_backend != nullptr ? g_backend->getName() : "none";
}

const char* getGraphicsBackendName()
{
    // 编译期表：图形上下文就绪之前（App 构造期就会打日志）也要能回答，所以不能转发给
    // 后端对象。三个分支必须同时满足两点：
    //   ① 和 platforms/*.cpp 的 createBackend() 分支一致（否则报的不是真正跑的那个后端）；
    //   ② 和对应后端 getName() 的字面量完全一致（否则一次运行里出现两个名字，
    //      历史上 GL 就是 "OpenGL 3.2 Core" vs "OpenGL"）。
    // ②由 tests/backend_name_test.cpp 钉住；改这里的字符串要同步改后端头文件。
#if defined(BKLV_SWITCH)
    return "OpenGL ES 3";
#elif defined(BKLV_VULKAN)
    return "Vulkan";
#else
    return "OpenGL 3.2 Core";
#endif
}

int getLiveTextureCount()
{
    int count = 0;
    for (const TextureEntry& entry : g_textures)
        if (entry.alive)
            count++;
    return count;
}

// 缓存里的失败标记。避免每建一个 cell 就重试一次并刷屏日志。
static constexpr int TEXTURE_CACHE_FAILED = -2;

TextureRef loadTexture(const std::string& path)
{
    g_pathLastUse[path] = ++g_useTick; // LRU 记账：这次命中也算"用过"
    auto cached = g_pathCache.find(path);
    if (cached != g_pathCache.end())
    {
        // 之前失败过就直接返回无效引用，不再重试、不再打日志。
        if (cached->second == TEXTURE_CACHE_FAILED)
            return TextureRef();
        return makeRef(cached->second);
    }

    // 上传需要图形后端。视图树常在 App::run() 之前就建好，那时仓库还没就绪——
    // 这种情况**不能**当成"解码失败"缓存起来，否则图永远加载不出来（踩过）。
    // 返回无效引用，交由调用方（Image）在每帧里重试。
    if (g_backend == nullptr)
        return TextureRef();

    // 解码交给平台相关的解码器（macOS ImageIO / 其他平台 stb_image）。
    const DecodedImage decoded = decodeImageFile(path.c_str());
    if (!decoded.isValid())
    {
        // 解码器一定会给原因（格式不支持 / 尺寸超限 / 文件读不到…），别只报个路径
        Logger::error("图片加载失败：" + path
            + (decoded.getError().empty() ? std::string() : "（" + decoded.getError() + "）"));
        g_pathCache[path] = TEXTURE_CACHE_FAILED;
        return TextureRef();
    }

    TextureHandle handle =
        g_backend->createTexture(decoded.pixels.data(), decoded.width, decoded.height);

    if (!handle.isValid())
    {
        g_pathCache[path] = TEXTURE_CACHE_FAILED;
        return TextureRef();
    }

    const int index = registerTexture(handle);
    g_pathCache[path] = index;
    return makeRef(index);
}

TextureRef loadTextureFromMemory(const void* data, size_t size)
{
    if (data == nullptr || size == 0 || g_backend == nullptr)
        return TextureRef();

    const DecodedImage decoded = decodeImageMemory(data, size);
    if (!decoded.isValid())
        return TextureRef();

    TextureHandle handle =
        g_backend->createTexture(decoded.pixels.data(), decoded.width, decoded.height);

    if (!handle.isValid())
        return TextureRef();

    // 内存加载不进路径缓存，但仍登记句柄，保证能被正确释放。
    return makeRef(registerTexture(handle));
}

TextureRef loadTextureFromPixels(const unsigned char* pixels, int width, int height)
{
    if (pixels == nullptr || width <= 0 || height <= 0)
        return TextureRef();

    if (g_backend == nullptr || !isTextureStoreReady())
    {
        BKLV_LOG_WARNING("纹理仓库还没就绪，无法上传纹理（%dx%d）", width, height);
        return TextureRef();
    }

    const TextureHandle handle = g_backend->createTexture(pixels, width, height);
    if (!handle.isValid())
        return TextureRef();

    // 不做路径缓存（像素是调用方给的），但仍登记句柄，保证能被正确释放
    return makeRef(registerTexture(handle));
}

bool updateTexture(const TextureRef& ref, const unsigned char* pixels, int width, int height)
{
    if (g_backend == nullptr || pixels == nullptr || width <= 0 || height <= 0)
        return false;

    if (ref.index < 0 || ref.index >= static_cast<int>(g_textures.size())
        || !g_textures[ref.index].alive)
        return false;

    TextureHandle& handle = g_textures[ref.index].handle;

    // 就地更新要求尺寸一致：动图的画布尺寸是固定的，不一致说明调用方搞错了
    if (handle.width != width || handle.height != height)
    {
        Logger::warning("更新纹理尺寸不一致：" + std::to_string(handle.width) + "x"
            + std::to_string(handle.height) + " vs " + std::to_string(width) + "x"
            + std::to_string(height));
        return false;
    }

    if (!g_backend->updateTexture(handle, pixels, width, height))
        return false;

    // 引用里缓存了尺寸，句柄没变就不用动
    return true;
}

void releaseTexture(TextureRef& ref)
{
    if (ref.index < 0 || ref.index >= static_cast<int>(g_textures.size()))
    {
        ref = TextureRef();
        return;
    }

    if (g_textures[ref.index].alive)
    {
        if (g_backend != nullptr)
            g_backend->destroyTexture(g_textures[ref.index].handle);
        g_textures[ref.index].alive = false;
    }

    // 缓存里指向这条的记录也要清掉，否则下次 loadTexture 会拿到失效引用。
    for (auto it = g_pathCache.begin(); it != g_pathCache.end();)
    {
        if (it->second == ref.index)
            it = g_pathCache.erase(it);
        else
            ++it;
    }

    ref = TextureRef();
}

void releaseTexture(TextureHandle& handle)
{
    if (g_backend != nullptr)
        g_backend->destroyTexture(handle);
    else
        handle = TextureHandle();
}

} // namespace bklv

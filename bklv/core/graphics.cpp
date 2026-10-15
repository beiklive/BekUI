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

#include "bklv/core/logger.hpp"

namespace bklv
{

// ---------------------------------------------------------------------------
// 纹理管理
// ---------------------------------------------------------------------------

namespace
{

RenderBackend* g_backend = nullptr;

struct TextureEntry
{
    TextureHandle handle;
    bool alive = false;
};

// 句柄表。存 TextureHandle 而不是裸 ImTextureID，
// 这样释放之后能把条目置为 dead，外部拿着的引用会失效而不是变成野指针。
std::vector<TextureEntry> g_textures;

// 纹理预算（0 = 不限）；只做账目，不淘汰
size_t g_textureBudget = 0;

// 路径 -> 索引，做加载缓存。
std::map<std::string, int> g_pathCache;

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

bool isTextureStoreReady()
{
    return g_backend != nullptr;
}

void initTextureStore(RenderBackend* backend)
{
    // 换后端时缓存全部作废：旧句柄属于上一个后端，不能再拿来用；
    // 之前的失败记录也要清掉，否则新后端明明可用却会被旧的失败标记挡住。
    if (g_backend != backend)
        g_pathCache.clear();

    g_backend = backend;
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

void setTextureBudget(size_t maxTextures)
{
    g_textureBudget = maxTextures;
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

const char* getTextureBackendName()
{
    return g_backend != nullptr ? g_backend->getName() : "none";
}

const char* getGraphicsBackendName()
{
    // 必须和 platforms/*.cpp 的 createBackend() 保持同样的分支，
    // 否则日志里报的后端名会和真正跑的那个对不上。
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

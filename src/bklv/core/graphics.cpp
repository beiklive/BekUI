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

const char* getTextureBackendName()
{
    return g_backend != nullptr ? g_backend->getName() : "none";
}

const char* getGraphicsBackendName()
{
    // 必须和 platforms/*.cpp 的 createBackend() 保持同样的分支，
    // 否则日志里报的后端名会和真正跑的那个对不上。
#if defined(BKLV_NULL_BACKEND)
    return "none";
#elif defined(BKLV_SWITCH)
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

    // 解码交给平台相关的解码器（macOS ImageIO / 其他平台 stb_image）。
    const DecodedImage decoded = decodeImageFile(path.c_str());
    if (!decoded.isValid() || g_backend == nullptr)
    {
        Logger::error("图片加载失败：" + path);
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

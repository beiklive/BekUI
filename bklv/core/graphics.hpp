#pragma once

// 渲染后端接口。
//
// 目的：让 View / Label / Image 这些上层代码完全不接触 OpenGL。
//
// 分工：
//   RenderBackend  —— 图形 API 相关：窗口标记、GL/VK 上下文、ImGui 渲染后端、
//                     "一段 RGBA 像素 -> 可绘制纹理"
//   Platform       —— 窗口、事件循环、系统能力，并决定用哪个后端（见 core/platform.hpp）
//   App            —— ImGui 上下文与帧调度
//   上层 View      —— 只认 ImTextureID / ImDrawList，不认具体图形 API
//
// 图形后端是**编译期**确定的，不做运行时选择：
// 后端代码、链接库、shader 版本都不同，运行时切换在一个构建里没有意义。
// 选择方式：
//   macOS / Linux / Windows —— 编入 backends/opengl_backend（OpenGL 3.2 Core）
//   Switch                  —— 编入 backends/gles3_backend（EGL + OpenGL ES 3）
// 要加后端，就是新写一个 RenderBackend 子类并在对应平台的 createBackend 里换掉，
// 链接库与 shader 版本随之改变，属于构建层面的选择。

#include <cstddef>
#include <string>
#include <vector>

#include "imgui.h"

namespace bklv
{

// ---------------------------------------------------------------------------
// 纹理句柄
// ---------------------------------------------------------------------------

// 后端创建出来的纹理。除了给 ImDrawList 用的 ImTextureID，
// 还给后端自己留了一个指针，比如 Vulkan 需要的 VkDescriptorSet。
struct TextureHandle
{
    ImTextureID texture = ImTextureID_Invalid; // 交给 ImDrawList::AddImage
    void* userData      = nullptr;             // 后端私有数据
    int width           = 0;
    int height          = 0;

    bool isValid() const { return this->texture != ImTextureID_Invalid && this->width > 0 && this->height > 0; }
};

// ---------------------------------------------------------------------------
// 后端接口
// ---------------------------------------------------------------------------

class RenderBackend
{
  public:
    virtual ~RenderBackend() = default;

    // 后端名字，日志与调试层展示用（"OpenGL" / "OpenGL ES 3" / "none"）。
    virtual const char* getName() const = 0;

    // ---- 窗口 ----

    // 创建窗口时要加的 SDL 标记（OpenGL 加 SDL_WINDOW_OPENGL，Vulkan 加 SDL_WINDOW_VULKAN）。
    // 这里返回 unsigned long long 是为了不让 core 依赖 SDL 类型；具体含义由平台实现解释。
    virtual unsigned long long getWindowFlags() const = 0;

    // 窗口创建之后、进入循环之前调用。
    // nativeWindow 在 SDL 平台上是 SDL_Window*，Switch 上是 NWindow*。
    // 返回 false 表示上下文初始化失败。
    virtual bool initContext(void* nativeWindow) = 0;

    // ---- ImGui 渲染后端 ----

    // ImGui 平台后端已经初始化好之后调用，这里初始化图形 API 那一侧的渲染后端。
    virtual bool initImGuiRenderer() = 0;
    virtual void shutdownImGuiRenderer() = 0;

    // 一帧的开始与结束。
    virtual void newFrame() = 0;

    // 清屏。
    virtual void clear(const float rgba[4]) = 0;

    // 把 ImGui 的绘制数据提交到当前后备缓冲。
    virtual void renderDrawData(ImDrawData* drawData) = 0;

    // 交换缓冲。
    virtual void present() = 0;

    // ---- 帧读回 ----
    //
    // 把当前已经画完、还没交换的后缓冲读成 RGBA8（左上角为原点）。
    // 离屏截图用：不依赖系统录屏权限，CI 里也能跑。默认返回 false（后端没实现）。
    virtual bool readPixels(int x, int y, int width, int height,
        std::vector<unsigned char>& outRgba)
    {
        (void)x;
        (void)y;
        (void)width;
        (void)height;
        (void)outRgba;
        return false;
    }

    // ---- 纹理 ----

    // 上传一段 RGBA8 像素。失败返回无效句柄。
    virtual TextureHandle createTexture(const unsigned char* pixels, int width, int height) = 0;

    // 就地更新已有纹理的像素（尺寸必须与创建时一致）。
    // 动图播放每帧只要换内容、不重建纹理，所以需要它；不实现的后端返回 false。
    virtual bool updateTexture(TextureHandle& handle, const unsigned char* pixels, int width,
        int height)
    {
        (void)handle;
        (void)pixels;
        (void)width;
        (void)height;
        return false;
    }

    // 释放纹理。必须由创建它的后端来释放。
    virtual void destroyTexture(TextureHandle& handle) = 0;

    // 后端是否已经可以创建纹理（上下文就绪）。
    // 上下文没就绪就创建纹理，在 macOS 的 Core Profile 下会直接崩。
    virtual bool isReady() const { return this->ready; }

  protected:
    bool ready = false;
};

// ---------------------------------------------------------------------------
// 纹理管理
// ---------------------------------------------------------------------------

// 加载后返回的引用。持有它比持有裸 ImTextureID 安全：
// 释放后 isValid() 会变成 false，不会被误用成野指针。
struct TextureRef
{
    int index = -1;
    ImTextureID texture = ImTextureID_Invalid;
    int width  = 0;
    int height = 0;

    bool isValid() const { return this->index >= 0 && this->texture != ImTextureID_Invalid && this->width > 0; }
};

// 设置当前后端。纹理的创建与释放都会走它。
void initTextureStore(RenderBackend* backend);

// 纹理仓库是否已就绪（也就是图形后端是否已经能上传纹理）。
// 视图树常常在 App::run() 之前就建好了，那时上传还做不了；
// Image 靠这个判断"要不要在每帧里重试加载"。
bool isTextureStoreReady();
// 释放全部缓存纹理并清空引用表（后端 shutdown 之前调用）。
void shutdownTextureStore();
// 当前后端名，调试层展示用。
const char* getTextureBackendName();

// —— 纹理预算（3.5 第一小步：只记账，不淘汰）——
// 0 = 不限（默认）。淘汰需要引用计数（否则会释放正在被使用的纹理），是下一小步。
void setTextureBudget(size_t maxTextures);
size_t getTextureBudget();
size_t getTextureCount();
bool isTextureBudgetExceeded();
// 路径缓存的条目数（只读；淘汰前后用它观察缓存是否真的降下来）
size_t getTextureCacheCount();
// 最近最少使用的路径（空串 = 缓存为空或无记录）。小步2 的淘汰会按它挑选候选；
// 现在只是记账，不做任何淘汰。
const std::string& getLeastRecentlyUsedPath();
// 引用计数（小步2）：TextureRef 是到处按值拷贝的小类型，所以计数放在仓库侧。
// 越界/已释放下标一律安全 no-op；release 在计数为 0 时只警告、不下溢。
void retainTexture(size_t index);
void releaseTexture(size_t index);
size_t getTextureRefCount(size_t index);
// 淘汰（2d）：只在超预算时做，候选 = refCount==0 的 LRU；淘汰时同步清路径缓存并释放 GPU 纹理。
void trimTextureCache();

// —— 背压（小步3 第一块：只存值，尚未接队列）——
// 0 一律表示"不限"（保持旧行为的逃生门）。默认 4 / 2：有上限但足够顺。
void setDecodeConcurrency(size_t maxConcurrentDecodes);
size_t getDecodeConcurrency();
void setMaxUploadsPerFrame(size_t maxUploads);
size_t getMaxUploadsPerFrame();

// 当前编入的是哪个图形后端（编译期常量，用于日志）。
const char* getGraphicsBackendName();

// 从文件加载（带缓存）。失败返回无效引用。
TextureRef loadTexture(const std::string& path);
// 从内存里的 PNG/JPEG 数据加载（不入缓存）。
TextureRef loadTextureFromMemory(const void* data, size_t size);
// 从**已经解码好的 RGBA8 像素**上传（不入缓存）。
// 异步加载用这个：解码在后台线程做，这里只负责上传，必须在主线程调。
TextureRef loadTextureFromPixels(const unsigned char* pixels, int width, int height);
// 就地更新纹理像素（动图播放用；尺寸必须与创建时一致）。失败返回 false。
bool updateTexture(const TextureRef& ref, const unsigned char* pixels, int width, int height);
// 释放一张纹理。
void releaseTexture(TextureRef& ref);
// 兼容旧写法：释放句柄形态的纹理。
void releaseTexture(TextureHandle& handle);

// 供调试层用：当前存活的纹理数量。
int getLiveTextureCount();

} // namespace bklv

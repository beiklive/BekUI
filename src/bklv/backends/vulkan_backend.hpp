#pragma once

// Vulkan 渲染后端。
//
// 构建：./build.sh --vk  ->  dist/vk/
// 依赖：Vulkan SDK（头文件 + loader）。Windows 用 MSYS2 的 vulkan-headers/loader 包，
//       macOS 上是 MoltenVK，Linux 上装 libvulkan-dev。
//
// 实现范围：单窗口、单 swapchain，正好覆盖 RenderBackend 的十个方法。
// ImGui 那一侧用官方 imgui_impl_vulkan，本文件只负责：
//   - 实例 / 设备 / 交换链 / 帧同步
//   - 渲染通道（优先动态渲染，驱动不支持时退回传统 VkRenderPass）
//   - "一段 RGBA 像素 -> ImTextureID"（即 VkImageView + VkDescriptorSet）
//
// 与 OpenGL 后端的差异见 notes/adding-render-backend.md。

#include "bklv/core/graphics.hpp"

// 桌面 Vulkan 后端（依赖 SDL3）。Switch 用 gles3_backend。
#if !defined(BKLV_SWITCH)

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <vector>

namespace bklv
{

class VulkanBackend : public RenderBackend
{
  public:
    VulkanBackend();
    ~VulkanBackend() override;

    const char* getName() const override { return "Vulkan"; }

    // ---- 窗口 ----
    unsigned long long getWindowFlags() const override;
    bool initContext(void* nativeWindow) override;

    // ---- ImGui 渲染后端 ----
    bool initImGuiRenderer() override;
    void shutdownImGuiRenderer() override;

    void newFrame() override;
    void clear(const float rgba[4]) override;
    void renderDrawData(ImDrawData* drawData) override;
    void present() override;

    // ---- 纹理 ----
    TextureHandle createTexture(const unsigned char* pixels, int width, int height) override;
    void destroyTexture(TextureHandle& handle) override;

  private:
    // 一张上传好的纹理。userData 指向它，销毁时才能找回 image/view/memory。
    struct TextureRecord
    {
        VkImage image         = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view      = VK_NULL_HANDLE;
        VkDescriptorSet set   = VK_NULL_HANDLE;
    };

    // ---- 初始化，按依赖顺序 ----
    bool createInstance();
    bool createSurface(SDL_Window* window);
    bool pickPhysicalDevice();
    bool createDevice();
    bool createSwapchain(VkSwapchainKHR oldSwapchain);
    bool createFrameResources();
    void destroySwapchainResources();
    void destroyFrameResources();

    // ---- 每帧 ----
    // 窗口尺寸变了就重建交换链。返回 false 表示这一帧画不了（最小化 / 重建失败）。
    bool prepareFrame();
    bool beginRendering();
    void recordImGui(ImDrawData* drawData);
    void endRendering();

    bool isDynamicRenderingEnabled() const;
    void logDeviceInfo() const;

    // ---- 句柄 ----
    SDL_Window* window = nullptr;

    VkInstance instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    uint32_t instanceApiVersion = VK_API_VERSION_1_0;
    VkSurfaceKHR surface      = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device           = VK_NULL_HANDLE;
    VkQueue queue             = VK_NULL_HANDLE;
    uint32_t queueFamily      = 0;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D extent    = { 0, 0 };

    // 传统渲染通道路径才需要；动态渲染下保持 VK_NULL_HANDLE。
    VkRenderPass renderPass = VK_NULL_HANDLE;

    std::vector<VkCommandPool> commandPools;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkFence> fences;
    std::vector<VkSemaphore> imageAvailable;  // 每帧一个：由取图像发出信号，由本帧提交等待
    std::vector<VkSemaphore> renderFinished;  // 与图像一一对应：由本帧提交发出信号，由呈现等待

    uint32_t imageCount     = 0;
    uint32_t framesInFlight = 0;
    uint32_t frameIndex     = 0;
    uint32_t imageIndex     = 0;
    bool imageAcquired      = false;
    bool blendDynamic       = false; // 设备是否支持 VK_KHR_dynamic_rendering

    std::vector<TextureRecord*> textures;

    float clearColor[4]        = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool inRenderPass          = false;
    bool recreateSwapchain     = false;
    bool imguiRendererInited   = false;
};

} // namespace bklv

#endif // !BKLV_SWITCH

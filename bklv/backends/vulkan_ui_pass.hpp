#pragma once

// 宿主要在 ImGui 通道前后插一脚时的口子（目前只有设计器的玻璃用）。
//
// 为什么要有这个：ImGui 的 Vulkan 后端在**渲染通道里面**执行绘制回调，
// 而抓帧/模糊/合成这类操作（图像拷贝、布局转换）必须在通道外做。
// 所以玻璃需要两个通道外的时机：
//   · UI 通道之前 —— 此时命令缓冲已开、交换链图像已 acquire 且已转成
//     COLOR_ATTACHMENT_OPTIMAL，但通道还没开始：可以在这里把背景画好；
//   · UI 通道之后（转成 PRESENT_SRC 之前）—— 可以把这一帧的成品拷进常驻图像，
//     留给下一帧当背景。
//
// 约定：只有 Vulkan 后端认这个钩子；没注册时是一串 no-op。
// 钩子内做的操作必须把交换链图像留在 COLOR_ATTACHMENT_OPTIMAL，后端接着开通道。
// GL / Switch 构建里这个头根本不会被包含（它带 vulkan.h）。

#include <vulkan/vulkan.h>

namespace bklv
{

struct VulkanUiPassHook
{
    virtual ~VulkanUiPassHook() = default;

    // 后端把创建资源需要的东西交出来（注册钩子时调一次）。
    // commandPool 是后端自用的池子，钩子可以在里面分配一次性命令（用完自己还回去）。
    virtual void attach(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
        VkQueue queue, uint32_t queueFamily, VkCommandPool commandPool)
        = 0;

    // 每帧/尺寸变化时告诉钩子当前交换链的格式与尺寸
    virtual void configure(VkFormat format, VkExtent2D extent) = 0;

    // UI 通道之前。返回 true 表示"这一帧的底色我已经铺好了" →
    // 后端会把 ImGui 通道的 loadOp 改成 LOAD（不清屏，保住钩子画的东西）。
    // 返回 false（或没钩子）→ 按原来的 CLEAR 走。
    virtual bool beforeUiPass(VkCommandBuffer commandBuffer, VkImage colorImage,
        VkImageView colorView, VkFormat format, VkExtent2D extent)
        = 0;

    // 后端即将销毁设备：钩子必须在这里释放所有用 device 创建的资源。
    // （踩过：钩子对象活得比设备久，析构里再 vkDestroyImage 就是 "Invalid device" 崩溃）
    virtual void detach() = 0;

    // UI 通道之后、转成 PRESENT_SRC 之前。图像仍是 COLOR_ATTACHMENT_OPTIMAL，
    // 钩子内部要把布局转回去再返回。
    virtual void afterUiPass(VkCommandBuffer commandBuffer, VkImage colorImage, VkFormat format,
        VkExtent2D extent)
        = 0;
};

} // namespace bklv

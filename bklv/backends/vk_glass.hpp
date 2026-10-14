#pragma once

// Vulkan 版玻璃：只做"背景模糊"，且**不用 shader** —— 顺着 mip 链用 vkCmdBlitImage
// 逐级下采样（硬件 box 滤波）就得到磨砂效果。圆角、染色、描边高光由 ImGui 层
// 叠在上面画（DesignerGlass 里那几行），所以这里不需要管线/描述符/SPIR-V。
//
// 时序（由 bklv 的 VulkanUiPassHook 给出）：
//   beforeUiPass —— ImGui 通道之前、通道外：
//       ① 把上一帧成品（常驻图像 mip0）拷回交换链 → 恢复"壁纸 + 上一帧画面"
//       ② 生成 mip 链（模糊）
//       ③ 每个玻璃矩形：从对应的 mip 级别 blit 缩放过去
//       返回 true → 后端把 ImGui 通道的 loadOp 改成 LOAD（不清屏，保住①③）
//   afterUiPass —— ImGui 通道之后、转 present 之前：把这一帧成品拷进 mip0，留给下一帧
//
// 只在 Vulkan 构建里编译（整个文件被 BKLV_VULKAN 包住）。

#if defined(BKLV_VULKAN)

#if defined(BKLV_VULKAN)
#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "bklv/backends/vulkan_ui_pass.hpp"

namespace bklv::glass
{

// 一块玻璃的矩形（物理像素）+ 想要的模糊程度（0..1）
struct VkGlassRect
{
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float blurScale = 0.5f;
};

class VkGlass : public bklv::VulkanUiPassHook
{
  public:
    VkGlass();
    ~VkGlass() override;

    void attach(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
        VkQueue queue, uint32_t queueFamily, VkCommandPool commandPool) override;
    void configure(VkFormat format, VkExtent2D extent) override;
    bool beforeUiPass(VkCommandBuffer commandBuffer, VkImage colorImage, VkImageView colorView,
        VkFormat format, VkExtent2D extent) override;
    void afterUiPass(VkCommandBuffer commandBuffer, VkImage colorImage, VkFormat format,
        VkExtent2D extent) override;
    void detach() override;

    bool isReady() const { return this->ready; }
    const std::string& getStatus() const { return this->status; }

    // 一帧的请求：beginFrame → addRect… → endFrame
    void beginFrame(float blurRadius, bool showBars);
    void addRect(const VkGlassRect& rect);
    void endFrame() { this->recording = false; }

  private:
    void destroyBackdrop();
    bool ensureBackdrop(VkExtent2D extent, VkFormat format);
    void transitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout,
        VkImageLayout newLayout, uint32_t baseMip, uint32_t mipCount);
    void blitImage(VkCommandBuffer commandBuffer, VkImage source, uint32_t sourceMip,
        VkImageLayout sourceLayout, VkImage target, uint32_t targetMip, VkImageLayout targetLayout,
        VkOffset3D sourceMin, VkOffset3D sourceMax, VkOffset3D targetMin, VkOffset3D targetMax,
        VkFilter filter);

    VkInstance instance         = VK_NULL_HANDLE;
    VkPhysicalDevice physical   = VK_NULL_HANDLE;
    VkDevice device             = VK_NULL_HANDLE;
    VkQueue queue               = VK_NULL_HANDLE;
    uint32_t queueFamily        = 0;
    VkCommandPool commandPool   = VK_NULL_HANDLE;

    VkImage backdropImage       = VK_NULL_HANDLE;
    VkDeviceMemory backdropMemory = VK_NULL_HANDLE;
    VkExtent2D backdropExtent   = { 0, 0 };
    VkFormat backdropFormat     = VK_FORMAT_UNDEFINED;
    uint32_t backdropMips       = 0;
    std::vector<VkImageLayout> backdropLayouts;
    // 常驻图像里是否已经有"上一帧画面"（第一帧没有，那帧不铺底）
    bool contentValid = false;

    bool ready = false;
    std::string status = "未初始化";

    bool recording = false;
    float blurRadius = 32.0f;
    std::vector<VkGlassRect> pending;
};

} // namespace bklv::glass

#endif // BKLV_VULKAN

#endif // BKLV_VULKAN
#pragma once

// Vulkan 渲染后端（待实现）。
//
// 构建：./build.sh --vk  ->  dist/vk/
// 依赖：Vulkan SDK（macOS 上是 MoltenVK）。
//
// 目前这里是占位实现：即使 SDK 齐备也不会绘制，而是明确报"未实现"，
// 避免出现"编过了但画面是黑的"这种更难排查的情况。
// 实现步骤见 notes/adding-render-backend.md。

#include "bklv/core/graphics.hpp"

#if defined(BKLV_VULKAN)

namespace bklv
{

class VulkanBackend : public RenderBackend
{
  public:
    VulkanBackend();
    ~VulkanBackend() override;

    const char* getName() const override { return "Vulkan"; }

    unsigned long long getWindowFlags() const override;
    bool initContext(void* nativeWindow) override;

    bool initImGuiRenderer() override;
    void shutdownImGuiRenderer() override;

    void newFrame() override;
    void clear(const float rgba[4]) override;
    void renderDrawData(ImDrawData* drawData) override;
    void present() override;

    TextureHandle createTexture(const unsigned char* pixels, int width, int height) override;
    void destroyTexture(TextureHandle& handle) override;
};

} // namespace bklv

#endif // BKLV_VULKAN

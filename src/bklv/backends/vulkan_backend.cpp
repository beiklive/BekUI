#include "bklv/backends/vulkan_backend.hpp"

#if defined(BKLV_VULKAN)

#include <vulkan/vulkan.h>

#include "bklv/core/logger.hpp"

namespace bklv
{

VulkanBackend::VulkanBackend()
{
    Logger::error("Vulkan 后端尚未实现：构建系统与平台接口已就位，缺的是后端本体");
    Logger::error("实现步骤见 notes/adding-render-backend.md");
}

VulkanBackend::~VulkanBackend() = default;

unsigned long long VulkanBackend::getWindowFlags() const
{
    // SDL_WINDOW_VULKAN
    return 0x0000000010000000ULL;
}

bool VulkanBackend::initContext(void* /*nativeWindow*/)
{
    Logger::error("VulkanBackend::initContext 未实现");
    return false;
}

bool VulkanBackend::initImGuiRenderer()
{
    return false;
}

void VulkanBackend::shutdownImGuiRenderer() { }

void VulkanBackend::newFrame() { }

void VulkanBackend::clear(const float[4]) { }

void VulkanBackend::renderDrawData(ImDrawData*) { }

void VulkanBackend::present() { }

TextureHandle VulkanBackend::createTexture(const unsigned char*, int, int)
{
    return TextureHandle();
}

void VulkanBackend::destroyTexture(TextureHandle& handle)
{
    handle = TextureHandle();
}

} // namespace bklv

#endif // BKLV_VULKAN

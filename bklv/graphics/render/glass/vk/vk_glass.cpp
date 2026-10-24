// Vulkan 的玻璃后端（路线 A：mip 链模糊 + ImGui 叠染色/高光），只在 VK 构建里编。
// 其它后端不编这个 TU（类继承 VulkanUiPassHook，只有 VK 构建才有）。
#if defined(BKLV_VULKAN)
// Vulkan 版玻璃的实现。设计要点见头文件注释：
// 只用 vkCmdBlitImage（mip 链下采样 = 模糊），不建管线、不用 SPIR-V。

#include "bklv/graphics/render/glass/vk/vk_glass.hpp"
#include "bklv/graphics/render/glass/vk/vk_glass.hpp"

#if defined(BKLV_VULKAN)

#include <algorithm>
#include <cmath>

#include "bklv/graphics/render/vk/vulkan_backend.hpp"
#include "bklv/foundation/core/logger.hpp"

namespace bklv::glass
{

namespace
{

constexpr uint32_t kMaxMips = 7;

uint32_t mipCountFor(VkExtent2D extent)
{
    uint32_t levels = 1;
    uint32_t size   = std::max(extent.width, extent.height);
    while (size > 1 && levels < kMaxMips)
    {
        size /= 2;
        levels++;
    }
    return levels;
}

} // namespace

VkGlass::VkGlass() = default;

VkGlass::~VkGlass()
{
    this->destroyBackdrop();
}

// ---------------------------------------------------------------------------
// GlassBackend：给玻璃服务用的那几个入口
// ---------------------------------------------------------------------------

bool VkGlass::ensure(int framebufferWidth, int framebufferHeight)
{
    (void)framebufferWidth;
    (void)framebufferHeight;

    // 钩子是懒挂的：只有真的要用玻璃（服务调到这里）才挂，
    // 否则"钩子在 = ImGui 通道不清屏"会改变没有玻璃那些帧的语义。
    if (this->owner != nullptr && !this->hooked)
    {
        if (!this->owner->hasUiPassHook())
            this->owner->installUiPassHook(this);
        this->hooked = true;
    }
    return this->isReady();
}

EffectCapabilities VkGlass::getCapabilities() const
{
    // 路线 A：只做模糊 + 抓背景，圆角/染色/描边由 ImGui 叠
    EffectCapabilities caps;
    caps.backdropCapture = true;
    caps.blur            = true;
    caps.refraction      = false;
    caps.dispersion      = false;
    caps.maxGlassRegions = 32;
    return caps;
}

void VkGlass::beginFrame(const GlassTuning& tuning)
{
    this->beginFrame(tuning.blurRadius, false);
}

void VkGlass::submit(GlassRequestQueue& queue, const GlassRequest& request,
    const GlassTuning& tuning, int frameWidth, int frameHeight)
{
    // Vulkan 这条路不走队列：模糊由 UI 通道外的 mip 链做，染色/描边当场叠成 ImGui 图元
    (void)queue;
    (void)frameWidth;
    (void)frameHeight;

    VkGlassRect rect;
    rect.minX      = request.minMax.x;
    rect.minY      = request.minMax.y;
    rect.maxX      = request.minMax.z;
    rect.maxY      = request.minMax.w;
    rect.blurScale = std::min(tuning.blurRadius / 48.0f, 1.0f);
    this->addRect(rect);

    // 模糊只有形状没有颜色：圆角、染色、描边高光由 ImGui 叠在它上面
    //（VK 这条路的取舍：不做折射/色散）
    if (request.drawList != nullptr)
    {
        const float radius = std::min(
            (request.minMax.z - request.minMax.x) * 0.5f, request.radii.topLeft * request.density);
        const ImU32 tint = ImGui::ColorConvertFloat4ToU32(
            ImVec4(tuning.tintR, tuning.tintG, tuning.tintB, std::max(tuning.tintA, 0.35f)));
        const ImU32 rim = ImGui::ColorConvertFloat4ToU32(
            ImVec4(1.0f, 1.0f, 1.0f, 0.10f + tuning.rimGlow * 0.35f));
        const float density = request.density > 0.0f ? request.density : 1.0f;
        const ImVec2 min(request.minMax.x / density, request.minMax.y / density);
        const ImVec2 max(request.minMax.z / density, request.minMax.w / density);
        request.drawList->AddRectFilled(min, max, tint, radius / density);
        request.drawList->AddRect(min, max, rim, radius / density, 0, 1.0f);
    }
}

void VkGlass::prepareDrawData(ImDrawData& drawData, const GlassRequestQueue& queue)
{
    // 抓帧与合成都发生在 UI 通道之外（beforeUiPass / afterUiPass），绘制数据不用动
    (void)drawData;
    (void)queue;
}

void VkGlass::attach(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
    VkQueue queue, uint32_t queueFamily, VkCommandPool commandPool)
{
    this->instance     = instance;
    this->physical     = physicalDevice;
    this->device       = device;
    this->queue        = queue;
    this->queueFamily  = queueFamily;
    this->commandPool  = commandPool;

    if (device == VK_NULL_HANDLE)
    {
        this->status = "拿不到 VkDevice，玻璃关闭";
        BKLV_LOG_WARNING("[玻璃][VK] %s", this->status.c_str());
        return;
    }

    // 大图拷贝 + blit 需要 TRANSFER 能力；交换链格式要支持 LINEAR 缩放 blit
    VkFormatProperties props = {};
    vkGetPhysicalDeviceFormatProperties(this->physical, VK_FORMAT_B8G8R8A8_UNORM, &props);
    this->ready  = true;
    this->status = "已就绪（mip 链模糊，无 shader）";
    BKLV_LOG_INFO("[玻璃][VK] 后端就绪：%s", this->status.c_str());
}

void VkGlass::detach()
{
    // 设备要没了：先把自己的资源交回去，再把句柄清干净（析构时就不碰设备了）
    this->destroyBackdrop();
    this->device        = VK_NULL_HANDLE;
    this->physical      = VK_NULL_HANDLE;
    this->instance      = VK_NULL_HANDLE;
    this->queue         = VK_NULL_HANDLE;
    this->commandPool   = VK_NULL_HANDLE;
    this->ready         = false;
    this->status        = "已随设备释放";
}

void VkGlass::configure(VkFormat format, VkExtent2D extent)
{
    if (this->device == VK_NULL_HANDLE)
        return;

    if (this->backdropImage != VK_NULL_HANDLE && this->backdropExtent.width == extent.width
        && this->backdropExtent.height == extent.height && this->backdropFormat == format)
        return;

    this->destroyBackdrop();
    if (!this->ensureBackdrop(extent, format))
        this->ready = false;
}

bool VkGlass::ensureBackdrop(VkExtent2D extent, VkFormat format)
{
    if (extent.width == 0 || extent.height == 0)
        return false;

    const uint32_t mips = mipCountFor(extent);

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = format;
    imageInfo.extent.width  = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = mips;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(this->device, &imageInfo, nullptr, &this->backdropImage) != VK_SUCCESS)
    {
        this->status = "常驻图像创建失败";
        BKLV_LOG_ERROR("[玻璃][VK] %s", this->status.c_str());
        return false;
    }

    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements(this->device, this->backdropImage, &requirements);

    VkPhysicalDeviceMemoryProperties memory = {};
    vkGetPhysicalDeviceMemoryProperties(this->physical, &memory);

    uint32_t typeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memory.memoryTypeCount; i++)
    {
        if ((requirements.memoryTypeBits & (1u << i)) != 0
            && (memory.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0)
        {
            typeIndex = i;
            break;
        }
    }
    if (typeIndex == UINT32_MAX)
    {
        this->status = "找不到合适的显存类型";
        BKLV_LOG_ERROR("[玻璃][VK] %s", this->status.c_str());
        return false;
    }

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize  = requirements.size;
    allocateInfo.memoryTypeIndex = typeIndex;
    if (vkAllocateMemory(this->device, &allocateInfo, nullptr, &this->backdropMemory) != VK_SUCCESS)
    {
        this->status = "常驻图像显存分配失败";
        BKLV_LOG_ERROR("[玻璃][VK] %s", this->status.c_str());
        return false;
    }
    vkBindImageMemory(this->device, this->backdropImage, this->backdropMemory, 0);

    this->backdropExtent = extent;
    this->backdropFormat = format;
    this->backdropMips   = mips;
    this->backdropLayouts.assign(mips, VK_IMAGE_LAYOUT_UNDEFINED);
    this->contentValid   = false;
    BKLV_LOG_INFO("[玻璃][VK] 常驻图像就绪：%ux%u，%u 级 mip", extent.width, extent.height, mips);
    return true;
}

void VkGlass::destroyBackdrop()
{
    if (this->device == VK_NULL_HANDLE)
        return;
    if (this->backdropImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(this->device, this->backdropImage, nullptr);
        this->backdropImage = VK_NULL_HANDLE;
    }
    if (this->backdropMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(this->device, this->backdropMemory, nullptr);
        this->backdropMemory = VK_NULL_HANDLE;
    }
    this->backdropLayouts.clear();
    this->backdropMips = 0;
    this->contentValid = false;
}

void VkGlass::transitionImage(VkCommandBuffer commandBuffer, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t baseMip, uint32_t mipCount)
{
    // 拷贝/blit 用的转换：源读、目标写。这里只服务 TRANSFER 系列 + 颜色附件两头的切换
    VkImageMemoryBarrier barrier = {};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = baseMip;
    barrier.subresourceRange.levelCount     = mipCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    auto isTransferDst = [](VkImageLayout layout) {
        return layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    };

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = isTransferDst(newLayout) ? VK_ACCESS_TRANSFER_WRITE_BIT
                                                         : VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        return;
    }

    if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            : VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                : VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        return;
    }

    if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        // 回到颜色附件（ImGui 要往这里画）
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        return;
    }

    // 其余情况：TRANSFER_SRC（给 blit 当源读）
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        // 从"颜色附件写"变成"传输读"：可用性得挂在 COLOR_ATTACHMENT_OUTPUT 上，
        // 挂 TRANSFER 会被校验层抓（写完之后没有可用的时刻）
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        return;
    }
    barrier.srcAccessMask = isTransferDst(oldLayout) ? VK_ACCESS_TRANSFER_WRITE_BIT
                                                     : VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VkGlass::blitImage(VkCommandBuffer commandBuffer, VkImage source, uint32_t sourceMip,
    VkImageLayout sourceLayout, VkImage target, uint32_t targetMip, VkImageLayout targetLayout,
    VkOffset3D sourceMin, VkOffset3D sourceMax, VkOffset3D targetMin, VkOffset3D targetMax,
    VkFilter filter)
{
    VkImageBlit region   = {};
    region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel       = sourceMip;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount     = 1;
    region.srcOffsets[0]                 = sourceMin;
    region.srcOffsets[1]                 = sourceMax;
    region.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel       = targetMip;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount     = 1;
    region.dstOffsets[0]                 = targetMin;
    region.dstOffsets[1]                 = targetMax;

    vkCmdBlitImage(commandBuffer, source, sourceLayout, target, targetLayout, 1, &region,
        filter);
}

void VkGlass::beginFrame(float radius, bool showBars)
{
    this->blurRadius = std::max(radius, 1.0f);
    this->pending.clear();
    this->recording = true;
    (void)showBars;
}

void VkGlass::addRect(const VkGlassRect& rect)
{
    if (!this->recording)
        return;
    if (rect.maxX <= rect.minX || rect.maxY <= rect.minY)
        return;
    this->pending.push_back(rect);
}

bool VkGlass::beforeUiPass(VkCommandBuffer commandBuffer, VkImage colorImage,
    VkImageView colorView, VkFormat format, VkExtent2D extent)
{
    (void)colorView;
    if (!this->ready || this->device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE)
        return false;

    this->configure(format, extent);
    if (this->backdropImage == VK_NULL_HANDLE)
        return false;

    // 第一帧没有"上一帧"可采：交给 ImGui 正常清屏
    if (!this->contentValid)
        return false;

    // ① 交换链：颜色附件 → 传输目标
    this->transitionImage(commandBuffer, colorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1);

    // ② 常驻图像 mip0：保证是 TRANSFER_SRC，然后整幅拷回交换链（恢复上一帧画面）
    if (this->backdropLayouts[0] != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        this->transitionImage(commandBuffer, this->backdropImage, this->backdropLayouts[0],
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1);
        this->backdropLayouts[0] = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    this->blitImage(commandBuffer, this->backdropImage, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        colorImage, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { 0, 0, 0 },
        { (int32_t)this->backdropExtent.width, (int32_t)this->backdropExtent.height, 1 },
        { 0, 0, 0 }, { (int32_t)extent.width, (int32_t)extent.height, 1 }, VK_FILTER_LINEAR);

    // ③ mip 链：逐级下采样 = 模糊（硬件 box 滤波，不需要 shader）
    for (uint32_t level = 1; level < this->backdropMips; level++)
    {
        if (this->backdropLayouts[level] != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            this->transitionImage(commandBuffer, this->backdropImage, this->backdropLayouts[level],
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, level, 1);
            this->backdropLayouts[level] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }
        this->blitImage(commandBuffer, this->backdropImage, level - 1,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, this->backdropImage, level,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { 0, 0, 0 },
            { (int32_t)std::max(1u, this->backdropExtent.width >> (level - 1)),
                (int32_t)std::max(1u, this->backdropExtent.height >> (level - 1)), 1 },
            { 0, 0, 0 },
            { (int32_t)std::max(1u, this->backdropExtent.width >> level),
                (int32_t)std::max(1u, this->backdropExtent.height >> level), 1 },
            VK_FILTER_LINEAR);
    }

    // 每块玻璃按模糊半径挑一级 mip（1/4 ~ 1/32）
    const int level = std::min<int>((int)this->backdropMips - 1,
        std::max(1, 2 + (int)std::lround(std::min(this->blurRadius / 48.0f, 1.0f) * 3.0f)));

    for (const VkGlassRect& rect : this->pending)
    {
        if (this->backdropLayouts[level] != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            this->transitionImage(commandBuffer, this->backdropImage,
                this->backdropLayouts[level], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, level, 1);
            this->backdropLayouts[level] = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        }

        VkOffset3D targetMin = { (int32_t)std::floor(rect.minX), (int32_t)std::floor(rect.minY),
            0 };
        VkOffset3D targetMax = { (int32_t)std::ceil(rect.maxX), (int32_t)std::ceil(rect.maxY), 1 };
        targetMin.x          = std::max(targetMin.x, 0);
        targetMin.y          = std::max(targetMin.y, 0);
        targetMax.x          = std::min(targetMax.x, (int32_t)extent.width);
        targetMax.y          = std::min(targetMax.y, (int32_t)extent.height);
        if (targetMax.x <= targetMin.x || targetMax.y <= targetMin.y)
            continue;

        this->blitImage(commandBuffer, this->backdropImage, (uint32_t)level,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage, 0,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { 0, 0, 0 },
            { (int32_t)std::max(1u, this->backdropExtent.width >> level),
                (int32_t)std::max(1u, this->backdropExtent.height >> level), 1 },
            targetMin, targetMax, VK_FILTER_LINEAR);
    }

    // ④ 交换链回到颜色附件：ImGui 这一趟用 LOAD 接着画
    this->transitionImage(commandBuffer, colorImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 1);
    return true;
}

void VkGlass::afterUiPass(VkCommandBuffer commandBuffer, VkImage colorImage, VkFormat format,
    VkExtent2D extent)
{
    if (!this->ready || this->device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE)
        return;

    this->configure(format, extent);
    if (this->backdropImage == VK_NULL_HANDLE)
        return;

    // 交换链 → 传输源
    this->transitionImage(commandBuffer, colorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1);
    // mip0 → 传输目标，把这一帧成品拷进去（下一帧的"上一帧画面"）
    this->transitionImage(commandBuffer, this->backdropImage, this->backdropLayouts[0],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1);
    this->backdropLayouts[0] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    this->blitImage(commandBuffer, colorImage, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        this->backdropImage, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { 0, 0, 0 },
        { (int32_t)extent.width, (int32_t)extent.height, 1 }, { 0, 0, 0 },
        { (int32_t)this->backdropExtent.width, (int32_t)this->backdropExtent.height, 1 },
        VK_FILTER_LINEAR);

    // mip0 留给下一帧当源
    this->transitionImage(commandBuffer, this->backdropImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1);
    this->backdropLayouts[0] = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    // 交换链交还给后端的呈现转换（它期望 COLOR_ATTACHMENT_OPTIMAL）
    this->transitionImage(commandBuffer, colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 1);

    this->contentValid = true;
}

} // namespace bklv::glass

#endif // BKLV_VULKAN

#endif // BKLV_VULKAN

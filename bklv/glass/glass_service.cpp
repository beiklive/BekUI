#include "bklv/glass/glass_service.hpp"

#include "bklv/core/logger.hpp"

#if defined(BKLV_VULKAN)
#include "bklv/backends/vulkan_backend.hpp"
#endif

namespace bklv::glass
{

GlassService& GlassService::getInstance()
{
    static GlassService instance;
    return instance;
}

GlassService& getGlassService()
{
    return GlassService::getInstance();
}

bool GlassService::ensureBackend()
{
    if (this->frameWidth <= 0 || this->frameHeight <= 0)
        return false;

#if defined(BKLV_VULKAN)
    // 路线 A：玻璃挂在 UI 通道的前/后钩子上（抓帧与模糊都在通道外做）
    if (VulkanBackend* backend = activeVulkanBackend())
    {
        if (!this->vkHooked)
        {
            if (!backend->isUiPassHookActive())
                backend->setUiPassHook(&this->vkGlass);
            this->vkHooked = true;
        }
        EffectCapabilities caps;
        caps.backdropCapture    = true;
        caps.blur               = true;
        caps.refraction         = false;   // 路线 A 不做折射
        caps.dispersion         = false;   // 也不做色散
        caps.maxGlassRegions    = 32;
        this->capabilities      = caps;
        const bool ready        = this->vkGlass.isReady();
        if (ready && !this->supported)
            Logger::info("[玻璃] Vulkan 后端就绪（路线 A：mip 链模糊，无折射/色散）");
        return ready;
    }
    return false;
#else

    // 尺寸变了会重建资源；失败会被记住（这个尺寸下不再每帧重试），所以这里可以每帧调。
    this->backend.ensure(this->frameWidth, this->frameHeight);
    this->capabilities = this->backend.getCapabilities();

    const bool ready = this->backend.isReady() && this->capabilities.backdropCapture
        && this->capabilities.blur;
    if (ready && !this->supported)
        Logger::info("[玻璃] 后端就绪：" + std::to_string(this->frameWidth) + "x"
            + std::to_string(this->frameHeight));
    return ready;
#endif // BKLV_VULKAN
}

void GlassService::beginFrame(int framebufferWidth, int framebufferHeight, float frameDensity)
{
    this->frameWidth  = framebufferWidth;
    this->frameHeight = framebufferHeight;
    this->density     = frameDensity > 0.0f ? frameDensity : 1.0f;
    this->injected    = false;

    // 整帧的模糊参数来自全局 tuning（外观层可以按层覆盖自己的样式）
    GlassBackdropStyle style;
    style.blurRadius = this->tuning.blurRadius;
    style.quality    = this->getQuality();
    this->queue.beginFrame(style, this->density);

#if defined(BKLV_VULKAN)
    if (activeVulkanBackend() != nullptr)
        this->vkGlass.beginFrame(this->tuning.blurRadius, false);
#endif

    this->supported = this->isEnabled() && this->ensureBackend();
}

void GlassService::submit(GlassRequest request)
{
    if (!this->isEnabled())
        return;

#if defined(BKLV_VULKAN)
    if (activeVulkanBackend() != nullptr)
    {
        VkGlassRect rect;
        rect.minX      = request.minMax.x;
        rect.minY      = request.minMax.y;
        rect.maxX      = request.minMax.z;
        rect.maxY      = request.minMax.w;
        rect.blurScale = std::min(this->tuning.blurRadius / 48.0f, 1.0f);
        this->vkGlass.addRect(rect);

        // 模糊只有形状没有颜色：圆角、染色、描边高光由 ImGui 叠在它上面
        //（VK 这条路的取舍：不做折射/色散）
        if (request.drawList != nullptr)
        {
            const float radius = std::min(
                (request.minMax.z - request.minMax.x) * 0.5f, request.radii.topLeft * request.density);
            const ImU32 tint = ImGui::ColorConvertFloat4ToU32(ImVec4(this->tuning.tintR,
                this->tuning.tintG, this->tuning.tintB,
                std::max(this->tuning.tintA, 0.35f)));
            const ImU32 rim = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f,
                0.10f + this->tuning.rimGlow * 0.35f));
            const float density = request.density > 0.0f ? request.density : this->density;
            const ImVec2 min(request.minMax.x / density, request.minMax.y / density);
            const ImVec2 max(request.minMax.z / density, request.minMax.w / density);
            request.drawList->AddRectFilled(min, max, tint, radius / density);
            request.drawList->AddRect(min, max, rim, radius / density, 0, 1.0f);
        }
        return;
    }
#endif

    // 后端起不来时静默丢弃：视图不需要知道玻璃有没有生效（它只是"声明"）
    if (!this->backend.isReady())
        return;

    if (request.density <= 0.0f)
        request.density = this->density;

    // 全局 tuning 是**默认值**：外观层（阶段 4 的 op）将来在它之上覆盖单个字段
    if (request.style.tint.w <= 0.0f)
        request.style = makeGlassStyle(this->tuning);
    request.blurMix = 1.0f;

    // 没给裁剪矩形就按整帧：空的 clipRect 会让合成那步什么都不画
    //（设计器那边一直显式填整帧，库这边替调用方兜住默认值）
    if (request.clipRect.z <= request.clipRect.x || request.clipRect.w <= request.clipRect.y)
    {
        request.clipRect = ImVec4(0.0f, 0.0f, (float)this->frameWidth, (float)this->frameHeight);
    }
    this->queue.submit(request);
}

bool GlassService::submitBackdrop(ImDrawList* drawList, const ImVec4& pixelBounds,
    const CornerRadii& radii, GlassShapeKind shape, float frameDensity)
{
    if (!this->isEnabled() || drawList == nullptr)
        return false;
    if (pixelBounds.z <= pixelBounds.x || pixelBounds.w <= pixelBounds.y)
        return false;

    // 关键：先切一条新命令。ImGui 会把连续图元合并进同一条命令，
    // 直接记 CmdBuffer.Size 拿到的不是真边界 —— 自己的内容会被并进前一条命令，
    // 于是玻璃插在它后面，把它盖住。
    drawList->AddDrawCmd();

    GlassRequest request;
    request.minMax            = pixelBounds;
    request.radii             = radii;
    request.shape             = shape;
    request.density           = frameDensity > 0.0f ? frameDensity : this->density;
    request.drawList = drawList;
    // 后端把玻璃插在"下标 == drawCommandOffset 的那条命令**之前**"。
    // 刚 AddDrawCmd() 出来的空命令（下标 Size-1）会承载调用方接下来画的内容，
    // 所以玻璃要插在**它之前** → 偏移取 Size-1。顺序就是：背景 → 玻璃 → 我的内容。
    request.drawCommandOffset = drawList->CmdBuffer.Size - 1;

    if (!this->isEnabled())
        return false;

    this->submit(request);
    return true;
}

void GlassService::prepareDrawData(ImDrawData& drawData)
{
    if (this->injected || !this->supported || this->queue.isEmpty())
        return;

    this->injected = true;
    this->backend.inject(drawData, this->queue);
}

void GlassService::finishFrame()
{
    this->queue.seal();
    this->backend.finishFrame();
#if defined(BKLV_VULKAN)
    if (activeVulkanBackend() != nullptr)
        this->vkGlass.endFrame();
#endif
}

EffectCapabilities GlassService::getCapabilities() const
{
    return this->capabilities;
}

} // namespace bklv::glass

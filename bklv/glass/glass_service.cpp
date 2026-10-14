#include "bklv/glass/glass_service.hpp"

#include "bklv/core/logger.hpp"

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

    // 尺寸变了会重建资源；失败会被记住（这个尺寸下不再每帧重试），所以这里可以每帧调。
    this->backend.ensure(this->frameWidth, this->frameHeight);
    this->capabilities = this->backend.getCapabilities();

    const bool ready = this->backend.isReady() && this->capabilities.backdropCapture
        && this->capabilities.blur;
    if (ready && !this->supported)
        Logger::info("[玻璃] 后端就绪：" + std::to_string(this->frameWidth) + "x"
            + std::to_string(this->frameHeight));
    return ready;
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

    this->supported = this->isEnabled() && this->ensureBackend();
}

void GlassService::submit(GlassRequest request)
{
    if (!this->isEnabled())
        return;

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
    request.drawList          = drawList;
    request.drawCommandOffset = drawList->CmdBuffer.Size;

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
}

EffectCapabilities GlassService::getCapabilities() const
{
    return this->capabilities;
}

} // namespace bklv::glass

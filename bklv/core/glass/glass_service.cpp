#include "bklv/core/glass/glass_service.hpp"

#include "bklv/core/globals.hpp"
#include "bklv/core/logger.hpp"

namespace bklv::glass
{

namespace
{

// 把服务登记进全局表（借用指针：服务活到进程结束）。
// 只登记一次 —— getGlassService() 在绘制路径上会被反复调用，不该每次都查表。
bool registerGlassServiceOnce(GlassService& service)
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        globals::getGlobalStore().set<GlassService>("glassService", &service, false);
    }
    return true;
}

} // namespace

GlassService& GlassService::getInstance()
{
    static GlassService instance;
    static const bool registered = registerGlassServiceOnce(instance);
    (void)registered;
    return instance;
}

GlassService& getGlassService()
{
    return GlassService::getInstance();
}

// 后端由图形后端注入（App::run 从 RenderBackend::getGlassBackend() 转交）。
// 没注入 = 这个后端没有玻璃能力，整层静默关闭。
bool GlassService::ensureBackend()
{
    if (this->backend == nullptr || this->frameWidth <= 0 || this->frameHeight <= 0)
        return false;

    // 尺寸变了会重建资源；失败会被记住（这个尺寸下不再每帧重试），所以这里可以每帧调。
    // 顺序要紧：**先 ensure 再读能力** —— GL 实现的能力是建资源时填的，
    // 反过来的话第一帧会读到"没有能力"，白报一次不可用。
    const bool ensured = this->backend->ensure(this->frameWidth, this->frameHeight);
    this->capabilities = this->backend->getCapabilities();

    const bool ready = ensured && this->capabilities.backdropCapture && this->capabilities.blur;
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

    if (this->backend != nullptr)
        this->backend->beginFrame(this->tuning);

    this->supported = this->isEnabled() && this->ensureBackend();
    if (this->isEnabled() && !this->supported && !this->warnedUnsupported)
    {
        this->warnedUnsupported = true;
        Logger::warning("[玻璃] 已开启但后端不可用 —— 声明了玻璃的层会回退成\"什么都不画\" "
                        "（真机上看着就是完全透明）。逐条原因见上面的 [玻璃] 日志。");
    }
}

void GlassService::submit(GlassRequest request)
{
    if (!this->isEnabled() || this->backend == nullptr)
        return;

    if (request.density <= 0.0f)
        request.density = this->density;

    // GL 与 Vulkan 的收法完全不同（排进队列等注入 vs. 当场记进自己的通道），
    // 由实现自己决定 —— 服务只负责补齐 density 与把请求转手。
    this->backend->submit(this->queue, request, this->tuning, this->frameWidth, this->frameHeight);
}

bool GlassService::submitBackdrop(ImDrawList* drawList, const ImVec4& pixelBounds,
    const CornerRadii& radii, GlassShapeKind shape, float frameDensity,
    const GlassLayerOptions* options)
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
    if (options != nullptr)
    {
        // 外观层写了的样式覆盖 Shell 的默认值（没写就保持 makeGlassStyle(tuning) 的结果）
        if (options->hasTint && options->tint.w > 0.0f)
            request.style.tint = options->tint;
        request.opacity = std::max(0.0f, std::min(1.0f, options->opacity));
    }
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
    if (this->injected || this->backend == nullptr || !this->supported || this->queue.isEmpty())
        return;

    this->injected = true;
    this->backend->prepareDrawData(drawData, this->queue);
}

void GlassService::finishFrame()
{
    this->queue.seal();
    if (this->backend != nullptr)
        this->backend->finishFrame();
}

EffectCapabilities GlassService::getCapabilities() const
{
    return this->capabilities;
}

} // namespace bklv::glass

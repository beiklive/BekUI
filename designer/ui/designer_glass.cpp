#include "designer/ui/designer_glass.hpp"

#include "bklv/core/app.hpp"
#include "bklv/core/graphics.hpp"

#include "bklv/core/logger.hpp"

#include "bklv/3rd/imgui/imgui.h"

#include <cstdlib>

namespace designer
{

DesignerGlass::DesignerGlass() = default;

void DesignerGlass::applyEnvOverrides()
{
    if (const char* env = std::getenv("BKLV_DESIGNER_GLASS"))
        this->tuning.enabled = std::string(env) != "0";
    if (const char* env = std::getenv("BKLV_DESIGNER_GLASS_PANEL"))
        this->tuning.showPanel = std::string(env) != "0";
}

void DesignerGlass::beginFrame(int framebufferWidth, int framebufferHeight, float density)
{
#if defined(BKLV_VULKAN)
    // Vulkan：ImGui 的绘制回调在渲染通道里，抓帧/模糊必须在通道外，
    // 所以走"UI 通道前铺底 + 通道后存档"这条路（见 vk_glass.hpp）。
    // 后端从 App 拿（库侧不再有"当前 Vulkan 后端"这种全局指针）。
    bklv::RenderBackend* backend = bklv::App::getCurrent() != nullptr
        ? bklv::App::getCurrent()->getRenderBackend()
        : nullptr;
    if (backend != nullptr && backend->isUiPassHookSupported())
    {
        if (this->vk == nullptr)
            this->vk = std::make_unique<glass::VkGlass>();
        // 只挂一次（每帧重挂会重复 attach/configure，日志刷屏也没必要）
        if (!backend->hasUiPassHook())
            backend->installUiPassHook(this->vk.get());

        this->vulkanActive     = backend->hasUiPassHook();
        this->vulkanDensity    = density > 0.0f ? density : 1.0f;
        this->injectedThisFrame = false;
        this->width  = framebufferWidth;
        this->height = framebufferHeight;
        if (this->tuning.enabled)
            this->vk->beginFrame(this->tuning.blurRadius, true);
        return;
    }
#endif

    // 玻璃后端是 GL（GLSL 150）。跑在 Vulkan 下时连 GL 上下文都没有，
    // 直接关掉 —— 不然会在没有当前上下文的情况下调 GL，轻则报错重则崩。
    if (this->tuning.enabled && !this->backendChecked)
    {
        this->backendChecked = true;
        const std::string backend = bklv::getGraphicsBackendName();
        if (backend.find("OpenGL") == std::string::npos && backend.find("GL") == std::string::npos)
        {
            this->tuning.enabled = false;
            this->status  = "当前后端不是 GL，玻璃关闭";
            BKLV_LOG_INFO("[玻璃] 当前后端是 %s，玻璃只在 GL 下启用", backend.c_str());
        }
    }

    this->injectedThisFrame = false;
    this->width             = framebufferWidth;
    this->height            = framebufferHeight;

    // 整个外壳共用一份背景模糊：半径按物理像素（源项目的调参范围是 4~24）
    glass::GlassBackdropStyle backdrop;
    backdrop.blurRadius = this->tuning.blurRadius;
    backdrop.quality    = this->tuning.quality <= 0 ? glass::GlassQuality::MINIMAL
        : (this->tuning.quality >= 2 ? glass::GlassQuality::FULL
                                    : glass::GlassQuality::BALANCED);

    this->forbiddenRects.clear();
    this->queue.beginFrame(backdrop, density);
}

#if defined(BKLV_VULKAN)
void DesignerGlass::addVulkanRect(const ImVec4& rect, ImDrawList* list, bool isBar)
{
    const glass::GlassTuning& tuning = this->tuning;

    // 禁区（预览区）：玻璃一律不许碰 —— 和 GL 那条路同一个规则
    float minX = rect.x;
    float minY = rect.y;
    float maxX = rect.z;
    float maxY = rect.w;
    for (const ImVec4& forbidden : this->forbiddenRects)
    {
        const bool intersects = minX < forbidden.z && maxX > forbidden.x && minY < forbidden.w
            && maxY > forbidden.y;
        if (!intersects)
            continue;
        // 取"避开的更大那一半"（和 gl 后端同一套裁剪）
        const float above = std::max(0.0f, forbidden.y - minY);
        const float below = std::max(0.0f, maxY - forbidden.w);
        const float left  = std::max(0.0f, forbidden.x - minX);
        const float right = std::max(0.0f, maxX - forbidden.z);
        const float best  = std::max(std::max(above, below), std::max(left, right));
        if (best <= 0.0f)
            return;
        if (best == above)
            maxY = forbidden.y;
        else if (best == below)
            minY = forbidden.w;
        else if (best == left)
            maxX = forbidden.x;
        else
            minX = forbidden.z;
    }
    if (maxX <= minX || maxY <= minY)
        return;

    const float density = this->vulkanDensity;
    glass::VkGlassRect physical;
    physical.minX      = minX * density;
    physical.minY      = minY * density;
    physical.maxX      = maxX * density;
    physical.maxY      = maxY * density;
    physical.blurScale = std::min(tuning.blurRadius / 48.0f, 1.0f);
    if (this->vk != nullptr && this->tuning.enabled)
        this->vk->addRect(physical);

    // 模糊只有形状没有颜色：圆角、染色、描边高光由 ImGui 叠在这上面画
    // （VK 这条路的取舍：不做折射/色散，先把"磨砂 + 染色 + 边缘高光"做出来）
    const float radius = isBar ? tuning.barRadius : tuning.panelRadius;
    const ImU32 tint   = ImGui::ColorConvertFloat4ToU32(ImVec4(tuning.tintR, tuning.tintG,
        tuning.tintB, std::max(tuning.tintA, 0.35f)));
    const ImU32 rim    = ImGui::ColorConvertFloat4ToU32(
        ImVec4(1.0f, 1.0f, 1.0f, 0.10f + tuning.rimGlow * 0.35f));
    list->AddRectFilled(ImVec2(minX, minY), ImVec2(maxX, maxY), tint, radius);
    list->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), rim, radius, 0, 1.0f);
}
#endif

void DesignerGlass::addBar(const ImVec4& rect, ImDrawList* list, int commandOffset)
{
    if (!this->isEnabled() || list == nullptr)
        return;

#if defined(BKLV_VULKAN)
    if (this->vulkanActive)
    {
        this->addVulkanRect(rect, list, true);
        return;
    }
#endif

    const glass::GlassTuning& tuning = this->tuning;

    glass::GlassStyle style;
    style.refractionHeight    = tuning.refractionHeight * 0.8f;
    style.refractionAmount    = tuning.refractionAmount * 0.9f;
    style.depthEffect         = tuning.depthEffect * 0.85f;
    style.chromaticAberration = tuning.chromaticAberration;
    style.contrast            = tuning.contrast;
    style.tint = ImVec4(tuning.tintR, tuning.tintG, tuning.tintB, tuning.tintA);
    style.lighting.innerShadow  = tuning.innerShadow;
    style.lighting.rimGlow      = tuning.rimGlow;
    style.lighting.specular     = tuning.specular;
    // 横条默认不外扩阴影：它会往预览面板里渗几像素（验收要求预览区逐像素一致）
    style.lighting.outerShadow  = tuning.barOuterShadow ? tuning.outerShadow : 0.0f;
    style.lighting.shadowExtent = tuning.barOuterShadow ? tuning.shadowExtent : 0.0f;

    this->submit(rect, glass::CornerRadii::all(tuning.barRadius), style, list, commandOffset);
}

void DesignerGlass::addPanel(const ImVec4& rect, ImDrawList* list, int commandOffset)
{
    if (!this->isEnabled() || list == nullptr)
        return;

#if defined(BKLV_VULKAN)
    if (this->vulkanActive)
    {
        this->addVulkanRect(rect, list, false);
        return;
    }
#endif

    const glass::GlassTuning& tuning = this->tuning;

    glass::GlassStyle style;
    style.refractionHeight    = tuning.refractionHeight;
    style.refractionAmount    = tuning.refractionAmount;
    style.depthEffect         = tuning.depthEffect;
    style.chromaticAberration = tuning.chromaticAberration;
    style.contrast            = tuning.contrast;
    style.tint = ImVec4(tuning.tintR, tuning.tintG, tuning.tintB, tuning.tintA);
    style.lighting.innerShadow  = tuning.innerShadow;
    style.lighting.rimGlow      = tuning.rimGlow;
    style.lighting.specular     = tuning.specular;
    style.lighting.outerShadow  = tuning.outerShadow;
    style.lighting.shadowExtent = tuning.shadowExtent;

    this->submit(rect, glass::CornerRadii::all(tuning.panelRadius), style, list, commandOffset);
}

void DesignerGlass::submit(const ImVec4& rect, const glass::CornerRadii& radii,
    const glass::GlassStyle& style, ImDrawList* list, int commandOffset)
{
    // 传进来的是 ImGui 的逻辑坐标；玻璃着色器与 GL 视口都是物理像素，
    // 所以这里按 density 放大（Retina 上不放大，模糊半径与折射会差一倍）
    const float density = this->queue.getDensity();
    ImVec4 physical      = ImVec4(rect.x * density, rect.y * density, rect.z * density,
        rect.w * density);

    // 预览区是禁区：裁到"避开的更大那一半"，全被吃掉就不交这块玻璃
    if (!this->clipAgainstForbidden(physical))
        return;

    glass::GlassRequest request;
    request.minMax           = physical;
    request.radii            = radii;
    request.style            = style;
    request.clipRect         = ImVec4(0.0f, 0.0f, (float)this->width, (float)this->height);
    request.drawList         = list;
    request.drawCommandOffset = commandOffset;
    request.density          = density;

    this->queue.submit(request);
}

void DesignerGlass::excludeRect(const ImVec4& rect)
{
    if (!this->isEnabled())
        return;

    // 这里传进来的是逻辑坐标；队列里存的是物理像素，统一换算后存本地
    const float density = this->queue.getDensity();
    this->forbiddenRects.push_back(ImVec4(rect.x * density, rect.y * density, rect.z * density,
        rect.w * density));
}

// 把一块玻璃裁到"避开禁区的更大那一半"；整块都被吃掉返回 false。
// 逻辑与原来的 GlassRequestQueue::largestRemainder 一致（只是搬到了设计器这边）。
bool DesignerGlass::clipAgainstForbidden(ImVec4& rect) const
{
    float minX = rect.x;
    float minY = rect.y;
    float maxX = rect.z;
    float maxY = rect.w;

    for (const ImVec4& forbidden : this->forbiddenRects)
    {
        const bool intersects = minX < forbidden.z && maxX > forbidden.x && minY < forbidden.w
            && maxY > forbidden.y;
        if (!intersects)
            continue;

        const float above = std::max(0.0f, forbidden.y - minY);
        const float below = std::max(0.0f, maxY - forbidden.w);
        const float left  = std::max(0.0f, forbidden.x - minX);
        const float right = std::max(0.0f, maxX - forbidden.z);
        const float best  = std::max(std::max(above, below), std::max(left, right));
        if (best <= 0.0f)
            return false;

        if (best == above)
            maxY = forbidden.y;
        else if (best == below)
            minY = forbidden.w;
        else if (best == left)
            maxX = forbidden.x;
        else
            minX = forbidden.z;
    }

    if (maxX <= minX || maxY <= minY)
        return false;

    rect = ImVec4(minX, minY, maxX, maxY);
    return true;
}

void DesignerGlass::endFrame()
{
    if (!this->isEnabled())
        return;

#if defined(BKLV_VULKAN)
    if (this->vulkanActive)
    {
        if (this->vk != nullptr)
            this->vk->endFrame();
        return;
    }
#endif
    this->queue.seal();
}

void DesignerGlass::prepareDrawData(ImDrawData* drawData)
{
    if (!this->isEnabled() || drawData == nullptr || this->queue.isEmpty())
        return;

    if (!this->backend.ensure(this->width, this->height))
    {
        if (this->status != "不可用（回退普通绘制）")
        {
            this->status = "不可用（回退普通绘制）";
            BKLV_LOG_WARNING("[玻璃] 后端不可用，设计器面板回退普通绘制");
        }
        this->capabilities = {};
        return;
    }

    this->capabilities = this->backend.getCapabilities();
    if (this->status != "已启用")
    {
        this->status = "已启用";
        BKLV_LOG_INFO("[玻璃] 已启用（%d 块以内）", (int)this->capabilities.maxGlassRegions);
    }

    this->backend.inject(*drawData, this->queue);
    this->injectedThisFrame = true;
}

void DesignerGlass::finishFrame()
{
    if (this->injectedThisFrame)
        this->backend.finishFrame();
}

} // namespace designer

#include "designer/ui/designer_glass.hpp"

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

    this->queue.beginFrame(backdrop, density);
}

void DesignerGlass::addBar(const ImVec4& rect, ImDrawList* list, int commandOffset)
{
    if (!this->isEnabled() || list == nullptr)
        return;

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
    glass::GlassRequest request;
    request.minMax           = ImVec4(rect.x * density, rect.y * density, rect.z * density,
        rect.w * density);
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

    const float density = this->queue.getDensity();
    this->queue.excludeRect(ImVec4(rect.x * density, rect.y * density, rect.z * density,
        rect.w * density));
}

void DesignerGlass::endFrame()
{
    if (!this->isEnabled())
        return;
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

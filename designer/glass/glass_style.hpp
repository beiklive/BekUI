#pragma once

// 液态玻璃的样式参数（只用于设计器界面）。
//
// 思路与默认值来自 ../developing/imgui-liquid-glass（MIT）：把"玻璃"拆成
// 【参数（本文件）】→【每帧请求（glass_request.hpp）】→【模糊计划（glass_blur_plan）】
// →【后端实现（gl/）】四层，参数与平台无关，后端可换（GL 已有，VK 以后再说）。
//
// 这里只保留设计器用得到的部分：玻璃面、模糊半径与质量档。

#include "imgui.h"

#include <cstdint>

namespace designer::glass
{

enum class GlassQuality : uint8_t
{
    AUTO,
    FULL,
    BALANCED,
    MINIMAL,
};

enum class GlassShapeKind : uint8_t
{
    ROUNDED_RECT,
    CAPSULE,
    CIRCLE,
};

struct CornerRadii
{
    float topLeft     = 28.0f;
    float topRight    = 28.0f;
    float bottomRight = 28.0f;
    float bottomLeft  = 28.0f;

    static constexpr CornerRadii all(float radius) noexcept
    {
        return CornerRadii{ radius, radius, radius, radius };
    }
};

// 光照：内阴影 / 边缘高光 / 高光斑 / 外阴影
struct GlassLighting
{
    float innerShadow   = 0.12f;
    float rimGlow       = 0.20f;
    float specular      = 0.35f;
    float outerShadow   = 0.14f;
    float shadowExtent  = 5.0f;
    float specularPower = 28.0f;
    ImVec2 lightDirection = ImVec2(-0.7071f, -0.7071f);
};

// 一块玻璃的观感：折射、色散、染色、光照
struct GlassStyle
{
    float refractionHeight    = 20.0f;
    float refractionAmount    = -70.0f;
    float depthEffect         = 0.3f;
    float chromaticAberration = 0.5f;
    float contrast            = 0.0f;
    float whitePoint          = 0.0f;
    float chromaMultiplier    = 1.0f;
    ImVec4 tint               = ImVec4(1.0f, 1.0f, 1.0f, 0.0f);
    GlassLighting lighting{};
};

// 所有玻璃面共用同一份"这一帧的背景模糊"
struct GlassBackdropStyle
{
    float blurRadius     = 0.01f;
    GlassQuality quality = GlassQuality::AUTO;
};

} // namespace designer::glass

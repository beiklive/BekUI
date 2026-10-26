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

namespace bklv::glass
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

// 外观层（op: glass）能逐项覆盖 GlassStyle 的哪些字段 —— 位掩码。
// 没置位的字段继续用 GlassTuning 折出来的全局值；置位的用请求里带的那个值。
// blurMix 不是 GlassStyle 的成员（它是"采样模糊底还是原始底"的混合比），单独一位。
namespace glass_field
{
inline constexpr uint32_t kBlurMix             = 1u << 0;
inline constexpr uint32_t kRefractionHeight    = 1u << 1;
inline constexpr uint32_t kRefractionAmount    = 1u << 2;
inline constexpr uint32_t kDepthEffect         = 1u << 3;
inline constexpr uint32_t kChromaticAberration = 1u << 4;
inline constexpr uint32_t kContrast            = 1u << 5;
inline constexpr uint32_t kWhitePoint          = 1u << 6;
inline constexpr uint32_t kChromaMultiplier    = 1u << 7;
inline constexpr uint32_t kLightInnerShadow    = 1u << 8;
inline constexpr uint32_t kLightRimGlow        = 1u << 9;
inline constexpr uint32_t kLightSpecular       = 1u << 10;
inline constexpr uint32_t kLightOuterShadow    = 1u << 11;
inline constexpr uint32_t kLightShadowExtent   = 1u << 12;
inline constexpr uint32_t kLightSpecularPower  = 1u << 13;
inline constexpr uint32_t kLightDirection      = 1u << 14;
} // namespace glass_field

} // namespace bklv::glass

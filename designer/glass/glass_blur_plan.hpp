#pragma once

// 模糊计划：把"物理半径 + 质量档"算成一组双线性配对权重（可分离高斯，两趟）。
// 逻辑与 imgui-liquid-glass 的 GlassBlurPolicy 一致：模糊在半分辨率上跑，
// sigma 用工作纹素表示，配对采样把纹理请求数减半。

#include "designer/glass/glass_style.hpp"

#include <array>
#include <cstdint>

namespace designer::glass
{

inline constexpr uint32_t kGlassGaussianPairCapacity = 12;

struct GlassBlurPlan
{
    uint32_t pairCount = 0;
    float centerWeight = 1.0f;
    std::array<float, kGlassGaussianPairCapacity> pairWeights{};
    std::array<float, kGlassGaussianPairCapacity> pairOffsets{};
};

GlassBlurPlan makeGlassBlurPlan(float physicalRadius, GlassQuality quality) noexcept;

} // namespace designer::glass

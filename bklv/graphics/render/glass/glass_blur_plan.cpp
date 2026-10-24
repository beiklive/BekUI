#include "bklv/graphics/render/glass/glass_blur_plan.hpp"

#include <algorithm>
#include <cmath>

namespace bklv::glass
{

namespace
{

constexpr uint32_t maximumPairs(GlassQuality quality) noexcept
{
    switch (quality)
    {
        case GlassQuality::MINIMAL:
            return 5;
        case GlassQuality::BALANCED:
            return 8;
        case GlassQuality::FULL:
        case GlassQuality::AUTO:
            return kGlassGaussianPairCapacity;
    }
    return kGlassGaussianPairCapacity;
}

} // namespace

GlassBlurPlan makeGlassBlurPlan(float physicalRadius, GlassQuality quality) noexcept
{
    if (!std::isfinite(physicalRadius) || physicalRadius <= 0.01f)
        return {};

    // 模糊在半分辨率上跑，sigma 用工作纹素表示
    const float sigma = std::max(0.5f, physicalRadius * 0.25f);
    const uint32_t support = std::max(1u, (uint32_t)std::ceil(sigma * 3.0f));

    GlassBlurPlan plan;
    plan.pairCount = std::min((support + 1u) / 2u, maximumPairs(quality));

    float normalization = 1.0f;
    for (uint32_t pair = 0; pair < plan.pairCount; pair++)
    {
        const uint32_t firstIndex  = pair * 2u + 1u;
        const uint32_t secondIndex = firstIndex + 1u;

        const float firstWeight = std::exp(-0.5f * (float)(firstIndex * firstIndex)
            / (sigma * sigma));
        const float secondWeight = secondIndex <= support
            ? std::exp(-0.5f * (float)(secondIndex * secondIndex) / (sigma * sigma))
            : 0.0f;

        const float combinedWeight = firstWeight + secondWeight;
        plan.pairWeights[pair]     = combinedWeight;
        plan.pairOffsets[pair]     = ((float)firstIndex * firstWeight
                                         + (float)secondIndex * secondWeight)
            / combinedWeight;

        normalization += combinedWeight * 2.0f;
    }

    plan.centerWeight = 1.0f / normalization;
    for (uint32_t pair = 0; pair < plan.pairCount; pair++)
        plan.pairWeights[pair] /= normalization;

    return plan;
}

} // namespace bklv::glass

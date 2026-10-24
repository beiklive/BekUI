#include "bklv/graphics/render/glass/glass_tuning.hpp"

namespace bklv::glass
{

GlassStyle makeGlassStyle(const GlassTuning& tuning) noexcept
{
    GlassStyle style;
    style.refractionHeight    = tuning.refractionHeight;
    style.refractionAmount    = tuning.refractionAmount;
    style.depthEffect         = tuning.depthEffect;
    style.chromaticAberration = tuning.chromaticAberration;
    style.contrast            = tuning.contrast;
    style.tint                = ImVec4(tuning.tintR, tuning.tintG, tuning.tintB, tuning.tintA);
    style.lighting.innerShadow  = tuning.innerShadow;
    style.lighting.rimGlow      = tuning.rimGlow;
    style.lighting.specular     = tuning.specular;
    style.lighting.outerShadow  = tuning.outerShadow;
    style.lighting.shadowExtent = tuning.shadowExtent;
    return style;
}

} // namespace bklv::glass

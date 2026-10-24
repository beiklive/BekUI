#include "bklv/graphics/render/glass/glass_geometry.hpp"

#include <algorithm>

namespace bklv::glass
{

float resolveGlassRadius(const ImVec4& minMax, const CornerRadii& radii, GlassShapeKind shape) noexcept
{
    const float width  = minMax.z - minMax.x;
    const float height = minMax.w - minMax.y;
    if (width <= 0.0f || height <= 0.0f)
        return 0.0f;

    const float halfMin = std::min(width, height) * 0.5f;

    float radius = 0.0f;
    switch (shape)
    {
        case GlassShapeKind::CAPSULE:
        case GlassShapeKind::CIRCLE:
            radius = halfMin;
            break;
        case GlassShapeKind::ROUNDED_RECT:
        default:
            radius = std::max(std::max(radii.topLeft, radii.topRight),
                std::max(radii.bottomRight, radii.bottomLeft));
            break;
    }

    return std::max(0.0f, std::min(radius, halfMin));
}

} // namespace bklv::glass

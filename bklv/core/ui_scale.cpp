#include "bklv/core/ui_scale.hpp"

#include <cmath>

namespace bklv
{

namespace
{
float g_uiScale = 1.0f;
} // namespace

float getUiScale()
{
    return g_uiScale;
}

void setUiScale(float scale)
{
    if (scale <= 0.0f || std::isnan(scale))
        return;

    g_uiScale = scale;
}

bool updateUiScaleFromCanvas(float canvasWidth, float canvasHeight)
{
    (void)canvasHeight; // 以宽度为基准（和 borealis 的 windowScale 一致）

    if (canvasWidth <= 0.0f)
        return false;

    const float scale = canvasWidth / UI_BASE_WIDTH;
    if (std::fabs(scale - g_uiScale) < 0.001f)
        return false;

    g_uiScale = scale;
    return true;
}

} // namespace bklv

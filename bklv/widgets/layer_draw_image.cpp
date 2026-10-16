#include "bklv/widgets/layer_draw.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{
namespace layers
{

void drawImage(const compile::LayerDrawContext& ctx)
{
    ImDrawList* drawList         = static_cast<ImDrawList*>(ctx.drawList);
    AppearanceView* view         = static_cast<AppearanceView*>(ctx.view);
    const int index              = ctx.layerIndex;

    const float radius     = ctx.instance.getNumber(index, AppearanceProp::RADIUS);
    const float opacity    = ctx.instance.getNumber(index, AppearanceProp::OPACITY);
    const float rect[4]    = { ctx.rect.minX, ctx.rect.minY, ctx.rect.maxX, ctx.rect.maxY };
    const float alphaScale = std::max(0.0f, std::min(1.0f, opacity)) * view->getAlpha();
    // 图与"是否有效"由调用方算好放进 ctx；nullptr = 没有图（原来的 isValid() 校验在那边）
    if (ctx.image == nullptr || alphaScale <= 0.001f)
        return;

    const ImVec2 min(rect[0], rect[1]);
    const ImVec2 max(rect[2], rect[3]);
    // 着色 = 白色 × 透明度（统一走 core/geometry.hpp 的 colorWithAlpha）
    const ImU32 tint = colorWithAlpha(IM_COL32(255, 255, 255, 255), alphaScale);

    if (radius > 0.0f)
        drawList->AddImageRounded(reinterpret_cast<ImTextureID>(ctx.image), min, max, ImVec2(0, 0), ImVec2(1, 1), tint,
            radius);
    else
        drawList->AddImage(reinterpret_cast<ImTextureID>(ctx.image), min, max, ImVec2(0, 0), ImVec2(1, 1), tint);
}

} // namespace layers
} // namespace bklv

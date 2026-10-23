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

    const AppearanceLayer& layer = ctx.layer;
    const bool sliced = layer.slice[0] > 0.0f || layer.slice[1] > 0.0f
        || layer.slice[2] > 0.0f || layer.slice[3] > 0.0f;
    if (sliced && radius <= 0.0f && ctx.imageWidth > 0 && ctx.imageHeight > 0)
    {
        const float imageW = (float)ctx.imageWidth;
        const float imageH = (float)ctx.imageHeight;
        const float srcX[4] = { 0.0f, layer.slice[3], imageW - layer.slice[1], imageW };
        const float srcY[4] = { 0.0f, layer.slice[0], imageH - layer.slice[2], imageH };
        const float dstX[4] = { rect[0], rect[0] + layer.slice[3], rect[2] - layer.slice[1], rect[2] };
        const float dstY[4] = { rect[1], rect[1] + layer.slice[0], rect[3] - layer.slice[2], rect[3] };

        for (int row = 0; row < 3; row++)
        {
            for (int col = 0; col < 3; col++)
            {
                if (dstX[col + 1] <= dstX[col] || dstY[row + 1] <= dstY[row]
                    || srcX[col + 1] <= srcX[col] || srcY[row + 1] <= srcY[row])
                    continue;
                drawList->AddImage(reinterpret_cast<ImTextureID>(ctx.image),
                    ImVec2(dstX[col], dstY[row]), ImVec2(dstX[col + 1], dstY[row + 1]),
                    ImVec2(srcX[col] / imageW, srcY[row] / imageH),
                    ImVec2(srcX[col + 1] / imageW, srcY[row + 1] / imageH), tint);
            }
        }
        return;
    }

    if (radius > 0.0f)
        drawList->AddImageRounded(reinterpret_cast<ImTextureID>(ctx.image), min, max, ImVec2(0, 0), ImVec2(1, 1), tint,
            radius);
    else
        drawList->AddImage(reinterpret_cast<ImTextureID>(ctx.image), min, max, ImVec2(0, 0), ImVec2(1, 1), tint);
}

} // namespace layers
} // namespace bklv

#include "bklv/widgets/layer_draw.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/core/shadow.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{
namespace layers
{

void drawShadow(const compile::LayerDrawContext& ctx)
{
    const AppearanceLayer& layer = ctx.layer;
    ImDrawList* drawList         = static_cast<ImDrawList*>(ctx.drawList);
    AppearanceView* view         = static_cast<AppearanceView*>(ctx.view);
    const int index              = ctx.layerIndex;

    const ImU32 fill        = ctx.instance.getColor(index, AppearanceProp::FILL);
    const ImU32 stroke      = ctx.instance.getColor(index, AppearanceProp::STROKE);
    const float strokeWidth = ctx.instance.getNumber(index, AppearanceProp::STROKE_WIDTH);
    const float radius      = ctx.instance.getNumber(index, AppearanceProp::RADIUS);
    const float opacity     = ctx.instance.getNumber(index, AppearanceProp::OPACITY);
    const float rect[4]     = { ctx.rect.minX, ctx.rect.minY, ctx.rect.maxX, ctx.rect.maxY };
    const float alphaScale  = std::max(0.0f, std::min(1.0f, opacity)) * view->getAlpha();
    auto scaleAlpha         = [alphaScale](ImU32 color) {
        const unsigned int a      = (color >> 24) & 0xFFu;
        const unsigned int scaled = (unsigned int)std::round((float)a * alphaScale);
        return (color & 0x00FFFFFFu) | ((scaled & 0xFFu) << 24);
    };
    (void)fill; (void)stroke; (void)strokeWidth; (void)radius; (void)scaleAlpha;

    if (alphaScale <= 0.001f || (fill >> 24) == 0)
        return;

    // 阴影形状 = 层矩形（inset/width/height/align 都按层语法来），再按 offset 平移；
    // 这几个值都从 AppearanceInstance 取，所以状态动效（offsetY / feather / spread）算数
    const float feather = ctx.instance.getNumber(index, AppearanceProp::FEATHER);
    const float spread  = ctx.instance.getNumber(index, AppearanceProp::SPREAD);
    const float offsetX = ctx.instance.getNumber(index, AppearanceProp::OFFSET_X);
    const float offsetY = ctx.instance.getNumber(index, AppearanceProp::OFFSET_Y);

    shadow::drawSoft(drawList, rect[0] + offsetX, rect[1] + offsetY,
        rect[2] + offsetX, rect[3] + offsetY, radius, fill, alphaScale, feather,
        spread, layer.cutout);
}

} // namespace layers
} // namespace bklv

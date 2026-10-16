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
    const float radius      = ctx.instance.getNumber(index, AppearanceProp::RADIUS);
    const float opacity     = ctx.instance.getNumber(index, AppearanceProp::OPACITY);
    const float rect[4]     = { ctx.rect.minX, ctx.rect.minY, ctx.rect.maxX, ctx.rect.maxY };
    // 透明度与着色的统一实现见 core/geometry.hpp（各层曾经各写一份 scaleAlpha lambda）。
    // 阴影的浓度由 shadow::drawSoft 按 alphaScale 处理，所以这里不需要自己乘颜色。
    const float alphaScale = std::max(0.0f, std::min(1.0f, opacity)) * view->getAlpha();

    if (alphaScale <= 0.001f || (fill >> 24) == 0)
        return;

    // 阴影形状 = 层矩形（inset/width/height/align 都按层语法来），再按 offset 平移；
    // 这几个值都从 AppearanceInstance 取，所以状态动效（offsetY / feather / spread）算数
    const float feather = ctx.instance.getNumber(index, AppearanceProp::FEATHER);
    const float spread  = ctx.instance.getNumber(index, AppearanceProp::SPREAD);
    const float offsetX = ctx.instance.getNumber(index, AppearanceProp::OFFSET_X);
    const float offsetY = ctx.instance.getNumber(index, AppearanceProp::OFFSET_Y);

    // 内阴影：没有方向 —— `offset` 对它无效（它画的是"从内缘往内扩散"的一圈带子，
    // 不是"某个方向的投影"）。想做出某一侧更重的凹陷感就叠两层、其中一层用 inset 收窄。
    if (layer.inner)
    {
        shadow::drawInnerSoft(drawList, rect[0], rect[1], rect[2], rect[3], radius, fill,
            alphaScale, feather, spread);
        return;
    }

    shadow::drawSoft(drawList, rect[0] + offsetX, rect[1] + offsetY,
        rect[2] + offsetX, rect[3] + offsetY, radius, fill, alphaScale, feather,
        spread, layer.cutout);
}

} // namespace layers
} // namespace bklv

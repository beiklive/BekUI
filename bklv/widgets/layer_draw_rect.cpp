#include "bklv/widgets/layer_draw.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/core/gradient.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{
namespace layers
{

void drawRect(const compile::LayerDrawContext& ctx)
{
    // draw() 里 switch 之前那批量，在钩子里按同样的名字重算一遍 -> case 内部一行都不用改
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
    // 逐层透明度 = 层 opacity × 控件 alpha；着色统一走 core/geometry.hpp 的 colorWithAlpha
    // （以前每个 layer_draw_*.cpp 里各写一个 scaleAlpha lambda，6 份逐字重复）
    const float alphaScale = std::max(0.0f, std::min(1.0f, opacity)) * view->getAlpha();

    if (alphaScale <= 0.001f)
        return;
    if (layer.gradient.isValid())
    {
        // 渐变优先于单色 fill（fill 继续服务状态覆盖：状态里写 fill 的层不带渐变）
        addGradientRectMesh(drawList, layer.gradient, ImVec2(rect[0], rect[1]),
            ImVec2(rect[2], rect[3]), radius, alphaScale);
    }
    else if ((fill >> 24) != 0)
    {
        drawList->AddRectFilled(ImVec2(rect[0], rect[1]), ImVec2(rect[2], rect[3]),
            colorWithAlpha(fill, alphaScale), radius);
    }
    if (strokeWidth > 0.0f && (stroke >> 24) != 0)
        drawList->AddRect(ImVec2(rect[0], rect[1]), ImVec2(rect[2], rect[3]),
            colorWithAlpha(stroke, alphaScale), radius, 0, strokeWidth);
}

void ensureLayerDrawsRegistered()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    if (!compile::setLayerDraw("rect", drawRect))
        BKLV_LOG_ERROR("注册 rect 的绘制钩子失败：注册表里没有这个名字");
    if (!compile::setLayerDraw("shadow", drawShadow))
        BKLV_LOG_ERROR("注册 shadow 的绘制钩子失败：注册表里没有这个名字");
    if (!compile::setLayerDraw("glass", drawGlass))
        BKLV_LOG_ERROR("注册 glass 的绘制钩子失败：注册表里没有这个名字");
    if (!compile::setLayerDraw("text", drawText))
        BKLV_LOG_ERROR("注册 text 的绘制钩子失败：注册表里没有这个名字");
    if (!compile::setLayerDraw("image", drawImage))
        BKLV_LOG_ERROR("注册 image 的绘制钩子失败：注册表里没有这个名字");
    if (!compile::setLayerDraw("custom", drawCustom))
        BKLV_LOG_ERROR("注册 custom 的绘制钩子失败：注册表里没有这个名字");
}

} // namespace layers
} // namespace bklv

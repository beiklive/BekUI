#include "bklv/layer/layer_draw.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/geom/geometry.hpp"
#include "bklv/theme/attributes.hpp"
#include "bklv/media/font.hpp"

namespace bklv
{
namespace layers
{

void drawText(const compile::LayerDrawContext& ctx)
{
    const AppearanceLayer& layer = ctx.layer;
    ImDrawList* drawList         = static_cast<ImDrawList*>(ctx.drawList);
    const int index              = ctx.layerIndex;

    const ImU32 textColor   = ctx.instance.getColor(index, AppearanceProp::TEXT_COLOR);
    const float fontSize    = ctx.instance.getNumber(index, AppearanceProp::FONT_SIZE);
    const float rect[4]     = { ctx.rect.minX, ctx.rect.minY, ctx.rect.maxX, ctx.rect.maxY };
    // 透明度与着色的统一实现见 core/geometry.hpp（各层曾经各写一份 scaleAlpha lambda）
    // 逐层透明度由绘图入口算好（= 层 opacity × 控件 alpha），这里直接用
    const float alphaScale = ctx.rect.alpha;
    // 内容槽由调用方（AppearanceView::draw）用虚函数算好放进 ctx，行为与原来一致
    ImFont* font              = static_cast<ImFont*>(ctx.font);
    const std::string content = ctx.text != nullptr ? *ctx.text : std::string();

    if (content.empty() || font == nullptr || alphaScale <= 0.001f)
        return;

    const float size = fontSize > 0.0f ? fontSize : 15.0f;
    const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, content.c_str());

    float textX = rect[0];
    if (enumMatches(layer.align, "center"))
        textX = rect[0] + ((rect[2] - rect[0]) - textSize.x) * 0.5f;
    else if (enumMatches(layer.align, "right"))
        textX = rect[2] - textSize.x;

    float textY = rect[1];
    if (enumMatches(layer.valign, "center"))
        textY = rect[1] + ((rect[3] - rect[1]) - textSize.y) * 0.5f;
    else if (enumMatches(layer.valign, "bottom"))
        textY = rect[3] - textSize.y;

    drawList->AddText(font, size, ImVec2(textX, textY), colorWithAlpha(textColor, alphaScale),
        content.c_str());
}

} // namespace layers
} // namespace bklv

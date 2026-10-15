#include "bklv/widgets/layer_draw.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/core/attributes.hpp"
#include "bklv/core/font.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{
namespace layers
{

void drawText(const compile::LayerDrawContext& ctx)
{
    const AppearanceLayer& layer = ctx.layer;
    ImDrawList* drawList         = static_cast<ImDrawList*>(ctx.drawList);
    AppearanceView* view         = static_cast<AppearanceView*>(ctx.view);
    const int index              = ctx.layerIndex;

    const ImU32 textColor   = ctx.instance.getColor(index, AppearanceProp::TEXT_COLOR);
    const float fontSize    = ctx.instance.getNumber(index, AppearanceProp::FONT_SIZE);
    const float opacity     = ctx.instance.getNumber(index, AppearanceProp::OPACITY);
    const float rect[4]     = { ctx.rect.minX, ctx.rect.minY, ctx.rect.maxX, ctx.rect.maxY };
    const float alphaScale  = std::max(0.0f, std::min(1.0f, opacity)) * view->getAlpha();
    auto scaleAlpha         = [alphaScale](ImU32 color) {
        const unsigned int a      = (color >> 24) & 0xFFu;
        const unsigned int scaled = (unsigned int)std::round((float)a * alphaScale);
        return (color & 0x00FFFFFFu) | ((scaled & 0xFFu) << 24);
    };
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

    drawList->AddText(font, size, ImVec2(textX, textY), scaleAlpha(textColor), content.c_str());
}

} // namespace layers
} // namespace bklv

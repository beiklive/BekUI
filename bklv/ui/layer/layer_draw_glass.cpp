#include "bklv/ui/layer/layer_draw.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/graphics/render/glass/glass_service.hpp"

namespace bklv
{
namespace layers
{

void drawGlass(const compile::LayerDrawContext& ctx)
{
    const AppearanceLayer& layer = ctx.layer;
    ImDrawList* drawList         = static_cast<ImDrawList*>(ctx.drawList);
    const int index              = ctx.layerIndex;

    const ImU32 fill        = ctx.instance.getColor(index, AppearanceProp::FILL);
    const ImU32 stroke      = ctx.instance.getColor(index, AppearanceProp::STROKE);
    const float strokeWidth = ctx.instance.getNumber(index, AppearanceProp::STROKE_WIDTH);
    const float radius      = ctx.instance.getNumber(index, AppearanceProp::RADIUS);
    const float rect[4]     = { ctx.rect.minX, ctx.rect.minY, ctx.rect.maxX, ctx.rect.maxY };
    // 逐层透明度由绘图入口算好（= 层 opacity × 控件 alpha），这里直接用
    const float alphaScale  = ctx.rect.alpha;
    auto scaleAlpha         = [alphaScale](ImU32 color) {
        const unsigned int a      = (color >> 24) & 0xFFu;
        const unsigned int scaled = (unsigned int)std::round((float)a * alphaScale);
        return (color & 0x00FFFFFFu) | ((scaled & 0xFFu) << 24);
    };
    (void)fill; (void)stroke; (void)strokeWidth; (void)radius; (void)scaleAlpha;

    // 外观层声明"这块要玻璃"。玻璃画在这一层的位置上，后面的内容层自然压在它上面
    //（所以 glass 层必须排在内容层之前）。
    // 能被外观覆盖的只有样式：fill(染色) / opacity / radius；
    // blurRadius、quality 与总开关只由 Shell 决定（两条后端的背景模糊是整帧共用一份）。
    glass::GlassService& service = glass::getGlassService();
    if (alphaScale <= 0.001f)
        return;

    // 玻璃不可用（后端没起来 / 关掉了）：退化成半透明底。
    // 不能什么都不画 —— 玻璃变体把底色覆盖成了透明，那样控件在真机上会整个消失
    // （Switch 上踩过：玻璃没生效时控件完全透明）。
    // 玻璃的边缘高光：层里写了 stroke/strokeWidth 就沿内缘描一圈。
    // 这是"玻璃"最有效的一笔（borealis 的面板也靠 1px 内描边立起来）；
    // 半径用的是控件自己的 radius，所以圆角不会错位。
    const ImU32 rimColor     = layer.stroke;
    const float rimThickness = layer.strokeWidth;
    const float rimInset     = rimThickness * 0.5f;

    if (!service.isEnabled() || !service.isSupported())
    {
        // 玻璃不可用时的兜底：染色太淡（< 0.16）就抬到 0.16 —— 玻璃层把 fill 当
        // 染色（可以很淡），但退化成一个纯色块时太淡会让控件"消失"（Switch 上踩过）。
        ImU32 fallbackFill   = fill;
        const unsigned int a = (fill >> 24) & 0xFFu;
        if (a != 0 && a < 41u)
            fallbackFill = (fill & 0x00FFFFFFu) | (41u << 24);
        if ((fallbackFill >> 24) != 0)
            drawList->AddRectFilled(ImVec2(rect[0], rect[1]), ImVec2(rect[2], rect[3]),
                scaleAlpha(fallbackFill), radius);
        if ((rimColor >> 24) != 0 && rimThickness > 0.0f)
            drawList->AddRect(ImVec2(rect[0] + rimInset, rect[1] + rimInset),
                ImVec2(rect[2] - rimInset, rect[3] - rimInset), scaleAlpha(rimColor),
                radius, 0, rimThickness);
        return;
    }

    const ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale;
    const float density  = fbScale.x > 0.0f ? fbScale.x : 1.0f;

    glass::GlassLayerOptions options;
    options.opacity = alphaScale;
    if ((fill >> 24) != 0)
    {
        const ImVec4 tint = ImGui::ColorConvertU32ToFloat4(fill);
        options.hasTint   = true;
        options.tint      = ImVec4(tint.x, tint.y, tint.z, tint.w * alphaScale);
    }

    // 外观层逐项覆盖玻璃样式（原版是每个控件一份 GlassStyle）：
    // NaN = JSON 没写，继续用 GlassTuning；写了 0 就是真的 0。
    auto takeNumber = [&options](float value, uint32_t field, float& target) {
        if (!std::isfinite(value))
            return;
        options.styleFields |= field;
        target = value;
    };
    glass::GlassStyle& style = options.style;
    takeNumber(layer.glass.blurMix, glass::glass_field::kBlurMix, options.blurMix);
    takeNumber(layer.glass.refractionHeight, glass::glass_field::kRefractionHeight,
        style.refractionHeight);
    takeNumber(layer.glass.refractionAmount, glass::glass_field::kRefractionAmount,
        style.refractionAmount);
    takeNumber(layer.glass.depthEffect, glass::glass_field::kDepthEffect, style.depthEffect);
    takeNumber(layer.glass.chromaticAberration,
        glass::glass_field::kChromaticAberration, style.chromaticAberration);
    takeNumber(layer.glass.contrast, glass::glass_field::kContrast, style.contrast);
    takeNumber(layer.glass.whitePoint, glass::glass_field::kWhitePoint, style.whitePoint);
    takeNumber(layer.glass.chromaMultiplier, glass::glass_field::kChromaMultiplier,
        style.chromaMultiplier);
    takeNumber(layer.glass.lightInnerShadow, glass::glass_field::kLightInnerShadow,
        style.lighting.innerShadow);
    takeNumber(layer.glass.lightRimGlow, glass::glass_field::kLightRimGlow,
        style.lighting.rimGlow);
    takeNumber(layer.glass.lightSpecular, glass::glass_field::kLightSpecular,
        style.lighting.specular);
    takeNumber(layer.glass.lightOuterShadow, glass::glass_field::kLightOuterShadow,
        style.lighting.outerShadow);
    takeNumber(layer.glass.lightShadowExtent, glass::glass_field::kLightShadowExtent,
        style.lighting.shadowExtent);
    takeNumber(layer.glass.lightSpecularPower, glass::glass_field::kLightSpecularPower,
        style.lighting.specularPower);
    if (std::isfinite(layer.glass.lightX) && std::isfinite(layer.glass.lightY))
    {
        // 方向两轴必须一起写（只给一轴没有意义，另一轴保持默认会让光照方向怪）
        options.styleFields |= glass::glass_field::kLightDirection;
        style.lighting.lightDirection = ImVec2(layer.glass.lightX, layer.glass.lightY);
    }

    service.submitBackdrop(drawList,
        ImVec4(rect[0] * density, rect[1] * density, rect[2] * density,
            rect[3] * density),
        glass::CornerRadii::all(radius), glass::GlassShapeKind::ROUNDED_RECT, density,
        &options);

    // 高光画在玻璃之后（submitBackdrop 会切一条新命令，所以这圈描边压在玻璃上面）
    if ((rimColor >> 24) != 0 && rimThickness > 0.0f)
        drawList->AddRect(ImVec2(rect[0] + rimInset, rect[1] + rimInset),
            ImVec2(rect[2] - rimInset, rect[3] - rimInset), scaleAlpha(rimColor),
            radius, 0, rimThickness);
}

} // namespace layers
} // namespace bklv

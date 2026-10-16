#pragma once

// 一次玻璃绘制的请求：一块矩形（可带圆角/胶囊/圆）加上采样与样式参数。
// 后端按这些数据生成几何、采样背景、合成，不改 bklv 库里的任何东西。

#include "bklv/core/glass/glass_style.hpp"

#include "imgui.h"

#include <cstdint>

namespace bklv::glass
{

enum class GlassBackdropSource : uint8_t
{
    FRAME,            // 这一帧到此刻为止已经画好的内容
    PREVIOUS_CONTENT, // 上一次抓取的"上一批内容"（面板互相覆盖时用）
};

struct GlassRequest
{
    ImVec4 minMax = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // x,y = 左上；z,w = 右下
    CornerRadii radii{};
    GlassShapeKind shape = GlassShapeKind::ROUNDED_RECT;
    GlassStyle style{};
    ImVec4 clipRect = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    ImVec2 deformation = ImVec2(0.0f, 0.0f);

    // 注入点：往哪个 draw list 的哪条命令之后插（设计器自己记录）
    ImDrawList* drawList = nullptr;
    int drawCommandOffset = 0;

    GlassBackdropSource backdropSource = GlassBackdropSource::FRAME;
    float density = 1.0f;
    float opacity = 1.0f;
    float blurMix = 1.0f;
};

// 后端能力：拿不到就整体回退成普通绘制（设计器不至于不能用）
//
// refraction / dispersion 由 `GlassService::ensureBackend` 按后端如实填（VK 路线 A 填 false），
// 读取方是设计器的调参面板（能力位为 false 时那几个折射滑块会被禁用并提示原因）。
// 绘制路径不查它：GL 的 `composite` 无条件上传折射 uniform（GL 的能力位本来就是 true），
// VK 压根没有折射那条路 —— 所以"能力位"目前只用来告诉使用者"这套后端有没有这个效果"。
struct EffectCapabilities
{
    bool backdropCapture = false;
    bool blur           = false;
    bool refraction     = false; // 读方：设计器调参面板
    bool dispersion     = false; // 读方：同上（和折射一起禁用）
    uint32_t maxGlassRegions = 0;
};

} // namespace bklv::glass

#pragma once

// `rect` 层的渐变（线性 / 径向）。
//
// 这一层是**纯数据 + 纯函数**：不碰 ImGui 绘制，所以能无窗口单测。
// 真正画在 `widgets/appearance_view.cpp`：把渐变算成**每顶点颜色**，交给 ImDrawList 的
// 顶点色网格 —— 因此 GL / Switch-GLES3 / Vulkan 三条后端**零改动**（都只吃 ImDrawList 顶点）。
//
// JSON 写法见 `docs/json-syntax.md` 与 `docs/rect-gradient.md`：
//   "gradient": { "type": "linear", "angle": 90,
//                 "stops": [ { "at": 0.0, "color": "#3A60C8" },
//                            { "at": 1.0, "color": "@theme/color.accent.base" } ] }

#include "imgui.h"

#include <cstdint>
#include <string>

namespace bklv
{

// 停靠点上限：定长存在层里（不给每层加堆分配），4 个覆盖绝大多数场景
inline constexpr int kMaxGradientStops = 4;

struct GradientStop
{
    float at    = 0.0f; // 0..1，必须严格递增（解析期校验）
    ImU32 color = 0;
};

struct Gradient
{
    enum class Type : uint8_t
    {
        LINEAR, // 沿 angle 方向
        RADIAL, // 以 center 为中心的径向
    };

    Type  type      = Type::LINEAR;
    float angle     = 90.0f;          // 度：0 = 左→右，90 = 上→下（顺时针）；只有 LINEAR 用
    float center[2] = { 0.5f, 0.5f }; // 0..1 相对层矩形；只有 RADIAL 用
    GradientStop stops[kMaxGradientStops];
    int   stopCount = 0;

    // 少于 2 个停靠点等于"没有渐变"，绘制时退回单色 fill
    bool isValid() const noexcept { return this->stopCount >= 2; }
};

// 名字 → 枚举（大小写与 `_`/`-` 都不敏感，和全库的 enumMatches 约定一致）
bool gradientTypeFromName(const std::string& name, Gradient::Type& out);
const char* gradientTypeName(Gradient::Type type) noexcept;

// t 落在哪一段、段内局部 t（两者都按钳制后的 t 算）
void gradientSegment(const Gradient& gradient, float t, int& segmentIndex, float& localT) noexcept;

// t 处的颜色：多段线性插值（t 先钳到 [0,1]；无效渐变返回 0）
ImU32 sampleGradient(const Gradient& gradient, float t) noexcept;

// 把渐变画成**顶点色网格**（只是 ImDrawList 顶点，所以 GL / Switch-GLES3 / Vulkan 通吃）：
//   · 直角（radius <= 0.5）：轴对齐角度只发 4 个顶点（精确）；斜角切 8x8 网格逐顶点采样；
//   · 圆角：沿圆角矩形边界取样 + 中心点做三角扇（线性渐变在扇形上精确），
//     再沿边界补一圈"外扩一个羽边、alpha=0"的顶点做抗锯齿（手写网格没有 ImGui 的自动羽边）。
void addGradientRectMesh(ImDrawList* drawList, const Gradient& gradient, const ImVec2& min,
    const ImVec2& max, float radius, float alphaScale);

// 层矩形（min..max）内某点对应的渐变参数 t：
//   LINEAR —— 沿 angle 方向投影（角度按顺时针，与 JSON 里的一致）
//   RADIAL —— 到 center 的归一化距离（按矩形的长边归一到 0..1，保证圆不过度拉伸）
float gradientParameter(const Gradient& gradient, const ImVec2& point, const ImVec2& min,
    const ImVec2& max) noexcept;

} // namespace bklv

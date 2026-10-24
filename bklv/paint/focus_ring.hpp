#pragma once

// 焦点框（流光框）的几何与绘制 —— 纯数据 + 纯函数，不依赖任何控件、也不读主题。
//
// 分工（刻意保持单向依赖，谁都不认识谁的绘制代码）：
//   · 控件只回答一个问题："我是什么形状"（View::getFocusOutline()）
//   · 顶层外壳（Shell）拥有画框的那个视图，并提供开关 / 换类型 / 调参数的接口
//   · 画框的视图每帧**拉取**当前焦点的形状，然后调这里画到浮层（HUD）上
//
// 形状用**圆角矩形**统一表达：矩形是 r=0，胶囊是 r=高/2，正圆是 r=短边/2（半径自动钳制）。
// 所以"聚焦到圆形控件上就变圆"不需要任何额外声明 —— 圆角半径够了，形状自己就成圆了。
//
// 颜色**只来自一张 LUT 贴图**（resources/focus_ring_gradient.png，横向 1D 色谱）：
//   U 轴 = 沿边框的弧长位置（+ 随时间推进的相位）→ 颜色就绕着边框流动；
//   V 轴 = 描边宽度方向（贴图各行相同，纯一维）。
// 这与 GBAStation 的 nvgBoxGradientLUT 是同一套做法（那边是给 NanoVG 加自定义 paint，
// 这里用 ImDrawList 的分段贴图四边形达到同样效果）。
//
// 注意：**样式不跟 JSON/主题走**，默认值写死在代码里，想改走 Shell 的接口。

#include <functional>

#include "bklv/geom/geometry.hpp"
#include "bklv/render/graphics.hpp"

namespace bklv
{
namespace focus_ring
{

// 焦点框类型。换类型 = 换画法，调用方只认这一个枚举。
enum class Type
{
    NONE,      // 不画（等于关掉）
    FLOW,      // 流光：贴图沿边框流动（默认）
    BREATHING, // 呼吸：同一张贴图，整体透明度随时间起伏
    SOLID,     // 静止：贴图不动
    CUSTOM,    // 用 style.customRenderer 自己画（加新类型不用改这里）
};

// 一个形状：圆角矩形。半径会被自动钳到短边的一半（超过就是圆/胶囊）。
struct Outline
{
    Rect  bounds;
    float cornerRadius = 0.0f;

    // 变换链里的等比缩放：控件被放大/缩小时半径要跟着放大，否则框和控件形状对不上。
    float scale = 1.0f;
};

// 描边样式。默认值是**代码写死**的（用户要求焦点框不受主题 JSON 控制）。
struct Style
{
    Type  type        = Type::FLOW;
    float lineWidth   = 3.0f;  // 线宽
    float expand      = 2.0f;  // 框与控件之间的缝（框画在控件外侧）
    float flowSeconds = 1.8f;  // 贴图绕一圈的时间（流光/呼吸都按它走）
    float alpha       = 1.0f;
    int   segments    = 0;     // 0 = 按周长自动（钳在 [kMinSegments, kMaxSegments]）

    // 色谱贴图。未加载成功时**什么都不画**（不退回自己算的颜色）。
    TextureRef lut;

    // Type::CUSTOM 时用：自己往 drawList 上画。几何工具（perimeter/pointAt/lutSpanAt…）
    // 都在本命名空间里，自定义渲染器可以直接复用。
    std::function<void(ImDrawList*, const Outline&, const Style&, float phase)> customRenderer;
};

// 分段数的上下限：太少能看出折线，太多白烧顶点。
inline constexpr int kMinSegments = 24;
inline constexpr int kMaxSegments = 192;

// 把时间折成 [0,1) 的循环相位。flowSeconds <= 0 时恒为 0（等于不流动）。
float phaseForTime(float seconds, float flowSeconds);

// 圆角矩形的中线周长（半径已钳制、已含 scale）。
float perimeter(const Outline& outline);

// 沿中线取点，t ∈ [0,1)：从"上边起点"开始顺时针走一圈。
Point pointAt(const Outline& outline, float t);

// 实际分段数：style.segments > 0 用它，否则按周长自动（约每 6px 一段）。
int segmentCount(const Outline& outline, const Style& style);

// 一段对应的贴图 U 区间。跨过接缝（u1 < u0）时要拆成两段画，
// 否则整段 UV 会被拉成一条横跨整张贴图的条纹。
struct LutSpan
{
    float u0      = 0.0f;
    float u1      = 0.0f;
    bool  wrapped = false;
};

LutSpan lutSpanAt(float t0, float t1, float phase);

// 把整圈描出来。几何来自上面的纯函数，颜色来自 style.lut 贴图。
// drawList 为空 / type 为 NONE / alpha <= 0 / 贴图无效 / 尺寸非正时什么都不画。
void draw(ImDrawList* drawList, const Outline& outline, const Style& style, float phase);

} // namespace focus_ring
} // namespace bklv

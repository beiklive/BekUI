#pragma once

// 阴影的绘制档位（全局开关）。
//
// 两种实现都是"多层圆角矩形按距离衰减"的近似，区别只在层数与衰减：
//   FAST   8 层，等权衰减——便宜，羽化半径大时仔细看能有层带
//   SMOOTH 24 层，高斯权重 + 亚像素扩张——肉眼即模糊；三角形的量仍然很小
//
// 之所以不做真正的离屏高斯模糊：那需要给三个后端各加一套离屏渲染 + 模糊 shader
// （RenderBackend 目前没有 render-to-texture 抽象），而 UI 阴影的羽化半径只有
// 10px 上下，多层的效果已经够了。以后真要上离屏模糊，换掉这个开关背后的实现即可，
// 外观 JSON 与调用方都不用动。

#include <string>

#include "bklv/3rd/imgui/imgui.h"

namespace bklv
{

enum class ShadowQuality
{
    FAST = 0,
    SMOOTH = 1,
};

namespace shadow
{

// 全局档位。默认 FAST；首次读取时会看环境变量 BKLV_SHADOW_QUALITY（fast / smooth），
// 之后以 setQuality() 设的为准。
ShadowQuality getQuality();
void setQuality(ShadowQuality quality);

// 按名字解析（"fast" / "smooth"），不认识时返回 false 且不改当前值
bool setQualityByName(const std::string& name);
const char* qualityName(ShadowQuality quality);

// 当前档位下画几层（FAST = 8，SMOOTH = 24）。drawSoft 内部用它，
// 测试也断言它——这样"档位开关真的改变了绘制"是可验证的。
int layerCount();

// 画一团柔和阴影（多层圆角矩形近似）。外观层的 SHADOW 走这条，
// 控件自己在 custom 层画的东西（比如滑块把手）也可以直接调它。
// alphaScale 会与颜色自带的 alpha 相乘。
// cutout = true 时挖掉 (minX..maxX, minY..maxY) 这块（只画外圈），
// 这样控件自身矩形里不会留下阴影——底色即使半透明也不会透出阴影（borealis 用 NVG_HOLE 做同一件事）。
void drawSoft(ImDrawList* drawList, float minX, float minY, float maxX, float maxY,
    float radius, ImU32 color, float alphaScale, float feather, float spread,
    bool cutout = false);

// 画一圈**内阴影**：贴着形状内缘最暗、往内渐透的内侧渐变带。
//
// **没有方向**：它不以"某个方向的光"为模型，就是"从边缘向内扩散"——四周对称，
// 四个角由圆角矩形 SDF 保证等宽内缩（不是"到最近边的距离"，那样四角会叠得更黑）。
// 想做出"某一侧更重"的凹陷感，叠加两层（其中一层用 inset 只占半边）比给内阴影加方向参数更直白。
//
// 带子宽度 = spread + feather（两者都算"往内伸多远"），最宽夹到短边的一半。
// 顶点按"离轮廓多远"逐点算 alpha，所以整条带子始终待在形状内部 —— ImGui 的裁剪只有矩形，
// 靠几何内缩才不会有溢出（圆角处尤其明显）。
void drawInnerSoft(ImDrawList* drawList, float minX, float minY, float maxX, float maxY,
    float radius, ImU32 color, float alphaScale, float feather, float spread);

// 清掉"已经读过环境变量"的标记（测试用：改完环境变量再读一次）
void resetEnvironmentProbeForTest();

} // namespace shadow

} // namespace bklv

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

// 清掉"已经读过环境变量"的标记（测试用：改完环境变量再读一次）
void resetEnvironmentProbeForTest();

} // namespace shadow

} // namespace bklv

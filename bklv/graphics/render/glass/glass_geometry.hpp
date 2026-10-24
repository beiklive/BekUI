#pragma once

// 玻璃的形状解析：把"矩形 + 四角半径 + 形状种类"折成**一个有效圆角半径**。
//
// 纯函数、无平台依赖 —— 两个后端（GL / VK）与测试共用同一套判断，
// 避免"同一个形状在两条后端上圆角算得不一样"。
//
// 规则（和焦点框共用同一种表达）：
//   ROUNDED_RECT  四角半径取最大值
//   CAPSULE       短边的一半（两端是半圆）
//   CIRCLE        短边的一半（配合正方形就是正圆）
// 最后统一钳到短边的一半：超过就退化成胶囊/正圆，不会出现"半径比控件还大"的怪形状。

#include "bklv/graphics/render/glass/glass_style.hpp"

namespace bklv::glass
{

// minMax：x,y = 左上；z,w = 右下（逻辑坐标或像素都行，只要和调用方一致）。
// 尺寸非正时返回 0（调用方据此跳过这块玻璃）。
float resolveGlassRadius(const ImVec4& minMax, const CornerRadii& radii, GlassShapeKind shape) noexcept;

} // namespace bklv::glass

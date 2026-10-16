#pragma once

// 把视图摆到容器（层）的角落 / 边上 / 铺满 —— 一组绝对定位的摆放工具。
//
// 原来这三个函数是 Shell 的成员（`Shell::addToCorner` / `addToEdge` / `fillLayer`），
// 只能作用于外壳自己的覆盖层，而且和内容栈、弹窗、背景动画一起挤在 700 行的 shell.cpp 里。
// 抽成自由函数之后：① Shell 那三个方法只做转发；② 任何容器都能用同一套摆放规则；
// ③ 居中类那一层"铺满的包装盒"只有一份实现（真角落直接给两条边偏移，不需要包装盒）。
//
// 约定：函数只碰 Box/View，不碰 Shell —— 所以它不认识外壳，也不认识主题。

#include "bklv/core/box.hpp"

namespace bklv
{

// 角落（九个方位）。居中类（*_CENTER / CENTER_* / CENTER）靠一层铺满的包装盒
// + justifyContent/alignItems 实现；四个真角落直接给两条边偏移。
enum class Corner
{
    TOP_LEFT,
    TOP_CENTER,
    TOP_RIGHT,
    CENTER_LEFT,
    CENTER,
    CENTER_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_CENTER,
    BOTTOM_RIGHT,
};

// 贴哪条边
enum class Edge
{
    TOP,
    RIGHT,
    BOTTOM,
    LEFT,
};

// 把 view 加进 layer 并摆到 corner（绝对定位）。view 为 nullptr 时什么都不做。
void placeInCorner(Box* layer, View* view, Corner corner, float insetX, float insetY);

// 贴某条边：这条边留 inset，另一轴两端归零（铺满）。
void placeOnEdge(Box* layer, View* view, Edge edge, float inset);

// 铺满整层（四边归零）：全屏遮罩、loading。
void fillLayerArea(Box* layer, View* view);

} // namespace bklv

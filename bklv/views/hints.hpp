#pragma once

// 底部提示栏，对应 borealis 的 views/hint.hpp + Hints 视图。
// 内容来自当前焦点视图注册的动作（View::registerAction）。

#include "imgui.h"

namespace bklv
{

// 在屏幕底部居中绘制提示栏。
void drawHints(ImDrawList* drawList, float screenWidth, float screenHeight);

} // namespace bklv

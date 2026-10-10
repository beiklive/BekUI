#pragma once

// 调试层，对应 borealis 的 views/debug_layer.hpp。
// 叠在界面最上层，显示当前焦点和整棵布局树，排布局问题时比打日志快得多。

#include "imgui.h"

namespace bklv
{

class View;

// 右侧面板：布局树 + 当前焦点。
void drawDebugLayer(ImDrawList* drawList, View* root, float screenWidth);

} // namespace bklv

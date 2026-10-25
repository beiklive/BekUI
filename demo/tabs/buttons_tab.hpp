#pragma once

#include "bklv/ui/view/view.hpp"

namespace demo
{

class DemoApp;

// 第一个标签页：按钮家族（文字 / 图标 / 图标+文字）+ 标签条排列开关。
bklv::View* buildButtonsTab(DemoApp& app);

} // namespace demo

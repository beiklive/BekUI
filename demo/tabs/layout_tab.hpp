#pragma once

#include "bklv/ui/view/view.hpp"

namespace demo
{

class DemoApp;

// 第二个标签页：布局（Box 的轴 / 间距 / 对齐 / 拉伸 / 内边距），用彩色矩形看清几何。
bklv::View* buildLayoutTab(DemoApp& app);

} // namespace demo

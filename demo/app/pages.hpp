#pragma once

// demo 的页面工厂：主页 + 三个演示内容页。
// 页面只负责"把自己的内容搭出来"；导航（push/pop、页头标题）由 DemoApp 负责。

#include "bklv/bklv.hpp"

namespace demo
{

// 图标总览：两套图标字体里实际存在的字形
bklv::Box* buildIconsPage();

// 水平 / 竖直 TabFrame 演示（内容自带测试数据）
bklv::Box* buildHorizontalTabPage();
bklv::Box* buildVerticalTabPage();

} // namespace demo

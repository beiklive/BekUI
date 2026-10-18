#pragma once

// demo 只保留一个页面：图标总览。
// （其余页面、贡献层与 JSON 样例已按要求清理；新增页面时在这里声明工厂函数。）

#include "bklv/bklv.hpp"

namespace demo
{

// 图标总览页：把两套图标字体里实际存在的字形全列出来（Switch / Material 各一节）
bklv::Box* buildIconsPage();

} // namespace demo

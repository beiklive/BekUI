#pragma once

// 各个页面共用的排版小工具。
//
// 都是"造一个已经配好的控件"，避免每个页面重复写一堆 setXxx。
// 返回的都是新建对象，所有权交给调用它的容器。

#include <string>

#include "bklv/core/box.hpp"
#include "bklv/views/label.hpp"

namespace demo
{

// 一行普通文字
bklv::Label* makeText(const std::string& text, float size, ImU32 color, bool singleLine = false);

// 页面标题
bklv::Label* makeTitle(const std::string& text);

// 页面说明文字（灰一点、行距大一点）
bklv::Label* makeHint(const std::string& text);

// 卡片：带背景、圆角、内边距，内部纵向排列，默认带下外边距
bklv::Box* makeCard(float padding = 16.0f);

// 横向一行，默认垂直居中
bklv::Box* makeRow(float gap = 12.0f, bklv::AlignItems align = bklv::AlignItems::CENTER);

} // namespace demo

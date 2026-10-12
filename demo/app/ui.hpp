#pragma once

// demo 的排版小工具：全部基于**库里的控件**，不再手写 Box + Label 拼控件。
//
//   makeText/makeHint  ->  Label（语义控件）；页面标题归全局 header，页面不再自己画
//   makeCard                     ->  Rectangle 的 "card" 变体（外观驱动的卡片）
//   makeRow/makeColumn           ->  Box（纯布局）

#include <string>

#include "bklv/bklv.hpp"

namespace demo
{

// 一行文字；singleLine=true 时超宽会截断成省略号
bklv::Label* makeText(const std::string& text, float size, ImU32 color, bool singleLine = false);

// 说明文字：跟随主题的次级文字色，行距大一点
bklv::Label* makeHint(const std::string& text);

// 卡片：外观是 widgets/rectangle.card.json，内边距取 metric.card.padding
bklv::Rectangle* makeCard();

// 横向一行 / 纵向一列
bklv::Box* makeRow(float gap = 12.0f, bklv::AlignItems align = bklv::AlignItems::CENTER);
bklv::Box* makeColumn(float gap = 10.0f);

} // namespace demo

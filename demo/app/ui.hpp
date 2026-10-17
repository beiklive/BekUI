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

// 变体版："card" 是不透明卡片，"card.glass" 是半透明玻璃卡片（主题里的 base/rectangle.card.glass）
bklv::Rectangle* makeCard(const std::string& variant);

// 玻璃的"背景"：渐变 + 斜条纹 + 柔光圆，铺在外壳的背景层（最早绘制）。
// 玻璃采的是"这一帧已经画好的内容"，所以它必须在最底层、且要够亮 + 有高频细节。
bklv::View* makeGlassWallpaper();

// 横向一行 / 纵向一列
bklv::Box* makeRow(float gap = 14.0f, bklv::AlignItems align = bklv::AlignItems::CENTER);
bklv::Box* makeColumn(float gap = 14.0f);

} // namespace demo

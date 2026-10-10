#pragma once

// 字体加载，对应 borealis 的 core/font.hpp 的简化版。
// borealis 支持 regular / 简繁中文 / emoji / 图标字体等多套字体并做回退，
// 这里只保留一套默认字体：优先系统中文字体，找不到就退回 ImGui 内置字体。

struct ImFont;

namespace bklv
{

// 在 ImGui 上下文创建之后调用一次。
void loadFonts();

// Label 默认使用的字体。
ImFont* getDefaultFont();
void setDefaultFont(ImFont* font);

// 字体版本号。每次默认字体变化时自增，用来判断"旧的布局是在没有字体时算的"。
unsigned int getFontGeneration();

} // namespace bklv

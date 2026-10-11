#pragma once

// 字体加载，对应 borealis 的 core/font.hpp。
//
// 各平台拿字体的方式不同：
//   桌面   读系统字体文件（Windows 微软雅黑 / macOS 苹方·冬青黑 / Linux 文泉驿·Noto）
//   Switch 走 pl 系统服务拿内置共享字体，不需要字体文件——Switch 上没有可读的系统字体
//          目录，共享字体里才有完整的中日韩字形
// Switch 上还会把 Standard / 简中扩展 / 繁中 / 韩文 / Nintendo 图标合并成 fallback 链，
// 这是 ImGui 侧对 borealis 的 nvgAddFallbackFontId 的等价做法（ImFontConfig::MergeMode）。
//
// 两个平台都优先使用用户自备字体（Switch 上是 sdmc:/bklv/font.ttf），
// 都没有时退回 ImGui 内置字体（中文会显示成方块）。

struct ImFont;

namespace bklv
{

// 在 ImGui 上下文创建之后调用一次。
void loadFonts();

// Label 默认使用的字体。
ImFont* getDefaultFont();

// 图标字体（hint 栏的按键字形）。没加载时退回默认字体；用 hasIconFont() 判断。
ImFont* getIconFont();
bool hasIconFont();
void setDefaultFont(ImFont* font);

// 字体版本号。每次默认字体变化时自增，用来判断"旧的布局是在没有字体时算的"。
unsigned int getFontGeneration();

} // namespace bklv

#pragma once

// 玻璃用到的 GLSL 源码（桌面 GL 3.2 Core / GLSL 150）。
//
// 改自 imgui-liquid-glass 的 shaders/opengl/*（MIT，(c) 2025 QmDeve，
// 原始出自 AndroidLiquidGlassView@9cdd915）。改动只有版本适配：
//   · `#version 300 es` + `precision highp float;` → `#version 150`
//   · 原来靠运行时"标记注入"拼进来的公共模块（blur_kernel / glass_math /
//     glass_color / glass_lighting）在这里直接内联成完整源码 ——
//     设计器不引入构建期着色器工具，源码就是常量字符串
//
// 四个程序：
//   · fullscreen.vert  + blur.frag     —— 半分辨率两趟可分离高斯模糊
//   · liquid_glass.vert + liquid_glass.frag —— 折射 / 色散 / 染色 / 边缘光照

namespace bklv::glass::shaders
{

extern const char* const kFullscreenVertex;
extern const char* const kBlurFragment;
extern const char* const kGlassVertex;
extern const char* const kGlassFragment;

} // namespace bklv::glass::shaders

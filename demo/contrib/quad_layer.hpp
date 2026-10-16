#pragma once

// 「不规则四边形」图层的**纯数学**部分（可无窗口测，见 tests/quad_layer_test.cpp）。
//
// 和绘制分开的理由：随机游走是纯函数（给定 seed / 时间 / 速度必得同一结果），
// 而绘制要 ImDrawList。分开之后"确定性、范围、连续性、四顶点互不相同"这些性质能直接断言，
// 不用建窗口。

#include <cstdint>

namespace demo
{

// 注册这一层（在程序初始化里调一次，**必须在主题加载之前**）。
// 声明放在样例自己的头里，这样样例是"头 + 实现"的完整模块，谁都能直接 include 用。
void registerQuadLayer();

namespace quad
{

// 一个顶点在某一轴上的游走偏移，范围 [-1, 1]。
//
// 实现是**值噪声**：把时间切成 1/speed 秒一段，段内在两个哈希随机值之间用 smoothstep 插值。
// 所以它是连续的（不像每帧重掷骰子那样抖成一团），但看起来是随机游走；
// 同一个 (seed, vertex, axis, t, speed) 永远给同一个值 —— 可测、可复现。
float wanderOffset(uint32_t seed, int vertex, int axis, float t, float speed);

// 四个顶点的偏移（单位：倍率）—— 下标 0..3 = 左上 / 右上 / 右下 / 左下。
struct VertexOffsets
{
    float x[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float y[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

// gain 是每个顶点自己的幅度系数（让四边形"不规则"）；传 nullptr 表示四个都是 1.0。
// 注意：最终位移在绘制侧还会按层短边夹一次（见 quad_layer.cpp），所以 gain 写多大都不会
// 把顶点甩到层外面去 —— 这里是纯数学，不做夹取。
VertexOffsets vertexOffsets(uint32_t seed, float t, float speed, const float* gain);

} // namespace quad
} // namespace demo

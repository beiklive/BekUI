// 「不规则四边形」扩展图层：**四个顶点各自随时间随机游走**。
//
// 这是"通用图层注册模板"（bklv/compile/layer_register.hpp）的第二个实物样例，和
// stripe_layer.cpp 的区别在于：它有**多个参数**、而且参数要靠绘制钩子自己算动画 ——
// 正好演示"引擎解析参数、钩子只读结果"这条分工。bklv/ 里一行都没改。
//
// 对应的主题 JSON：
//
//   { "op": "quad", "id": "blob",
//     "amplitude": 8,          // 顶点最大位移（像素，跟 uiScale 缩放）
//     "speed": 1.5,            // 每秒几个时间片（越大越活跃）
//     "seed": 7,               // 随机种子（同 seed 同画面）
//     "gain0": 1, "gain1": 0.6, "gain2": 1.4, "gain3": 0.8,   // 每顶点幅度
//     "fill": "@theme/color.accent.base",
//     "stroke": "@theme/color.text.primary", "strokeWidth": 2,
//     "inset": 4 }
//
// 顶点顺序：左上 → 右上 → 右下 → 左下（顺时针）。

#include "support/quad_layer.hpp"

#include <cmath>
#include <vector>

#include "bklv/3rd/imgui/imgui.h"

#include "bklv/compile/layer_register.hpp"
#include "bklv/core/geometry.hpp" // colorWithAlpha
#include "bklv/core/time.hpp"

namespace
{

using bklv::LayerParamType;
using bklv::compile::LayerDrawContext;
using bklv::compile::LayerOrder;
using bklv::compile::LayerParam;

// 整数哈希 -> [0,1)：便宜、无状态、同输入同输出（值噪声用它取"随机"值）
float hash01(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return (float)(x & 0xFFFFFFu) / (float)0x1000000u;
}

float smoothstep(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

struct QuadLayer
{
    static constexpr const char* kName     = "quad";
    static constexpr const char* kLabel    = "不规则四边形";
    static constexpr const char* kIdPrefix = "quad";
    static constexpr LayerOrder kOrder     = LayerOrder::DECORATION;
    static constexpr const char* kHelp     = "不规则四边形：四顶点各自随机游走"
                                             "（amplitude 幅度 / speed 速度 / seed 种子 / gain0-3 每顶点幅度）";

    // 七个参数（上限是 8）。都没有 parse 钩子 —— 标量参数由引擎按模式解析。
    static std::vector<LayerParam> params()
    {
        return {
            LayerParam::pixels("amplitude", 8.0f, "幅度", "顶点最大位移（像素）"),
            LayerParam::number("speed", 1.5f, "速度", "每秒几个时间片，越大越活跃"),
            LayerParam::number("seed", 1.0f, "种子", "同种子同画面；改它换一套随机"),
            LayerParam::number("gain0", 1.0f, "左上幅度", "左上顶点的幅度系数"),
            LayerParam::number("gain1", 1.0f, "右上幅度"),
            LayerParam::number("gain2", 1.0f, "右下幅度"),
            LayerParam::number("gain3", 1.0f, "左下幅度"),
        };
    }

    static void draw(const LayerDrawContext& ctx);
};

// 四个顶点的偏移是纯函数（见 contrib/quad_layer.hpp），这里只是把参数喂给它
demo::quad::VertexOffsets offsetsFor(const LayerDrawContext& ctx, float time)
{
    const float gain[4] = {
        bklv::compile::numberParam(ctx, "gain0", 1.0f),
        bklv::compile::numberParam(ctx, "gain1", 1.0f),
        bklv::compile::numberParam(ctx, "gain2", 1.0f),
        bklv::compile::numberParam(ctx, "gain3", 1.0f),
    };
    const uint32_t seed = (uint32_t)std::lround(bklv::compile::numberParam(ctx, "seed", 1.0f));
    const float speed   = bklv::compile::numberParam(ctx, "speed", 1.5f);
    return demo::quad::vertexOffsets(seed, time, speed, gain);
}

void QuadLayer::draw(const LayerDrawContext& ctx)
{
    ImDrawList* drawList         = static_cast<ImDrawList*>(ctx.drawList);
    const bklv::compile::LayerRect& rect = ctx.rect;
    const float width            = rect.maxX - rect.minX;
    const float height           = rect.maxY - rect.minY;
    if (drawList == nullptr || width <= 1.0f || height <= 1.0f || rect.alpha <= 0.001f)
        return;

    // 幅度夹到短边的 20%：再大就可能把四边形抖成凹的/自交，填充会出现瑕疵
    const float shortSide = std::min(width, height);
    const float amplitude = std::min(bklv::compile::numberParam(ctx, "amplitude", 8.0f),
        shortSide * 0.2f);

    // 时间：框架的单调时钟（无窗口也安全，见 core/time.hpp 的约定）
    const float time = (float)bklv::getSecondsNow();
    const demo::quad::VertexOffsets offsets = offsetsFor(ctx, time);

    // 四角（顺时针：左上 / 右上 / 右下 / 左下）+ 各自的游走偏移
    const float baseX[4] = { rect.minX, rect.maxX, rect.maxX, rect.minX };
    const float baseY[4] = { rect.minY, rect.minY, rect.maxY, rect.maxY };
    // 顶点偏移也各自夹一次：`gain` 是乘在 amplitude **之后**的，光夹 amplitude 拦不住
    // `gain0: 9` 这种写法（顶点会飞到层外面，形状也没法看）。夹到短边的 35% 以内。
    const float limit = shortSide * 0.35f;
    auto clampOffset = [limit](float value) {
        return std::max(-limit, std::min(limit, value));
    };
    ImVec2 points[4];
    for (int i = 0; i < 4; i++)
    {
        points[i].x = baseX[i] + clampOffset(offsets.x[i] * amplitude);
        points[i].y = baseY[i] + clampOffset(offsets.y[i] * amplitude);
    }

    // 填充：ImDrawList 只有"凸多边形填充"，抖过的四边形理论上可能变凹 ——
    // 判一次叉积符号，凹的时候拆成两个三角形（三角形填充对凹形状总是正确的，
    // 代价是共用边上会有一道抗锯齿接缝，正常幅度下看不到）。
    const ImU32 fill = bklv::colorWithAlpha(ctx.layer.fill, rect.alpha);
    auto cross = [](const ImVec2& a, const ImVec2& b, const ImVec2& c) {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    };
    const float c0 = cross(points[0], points[1], points[2]);
    const float c1 = cross(points[1], points[2], points[3]);
    const float c2 = cross(points[2], points[3], points[0]);
    const float c3 = cross(points[3], points[0], points[1]);
    const bool convex = (c0 >= 0.0f && c1 >= 0.0f && c2 >= 0.0f && c3 >= 0.0f)
        || (c0 <= 0.0f && c1 <= 0.0f && c2 <= 0.0f && c3 <= 0.0f);

    if ((fill >> 24) != 0)
    {
        if (convex)
            drawList->AddConvexPolyFilled(points, 4, fill);
        else
        {
            drawList->AddTriangleFilled(points[0], points[1], points[2], fill);
            drawList->AddTriangleFilled(points[0], points[2], points[3], fill);
        }
    }

    // 描边：引擎字段 stroke / strokeWidth（层语法里的圆角对四边形没有意义，忽略）
    if (ctx.layer.strokeWidth > 0.0f && (ctx.layer.stroke >> 24) != 0)
        drawList->AddPolyline(points, 4,
            bklv::colorWithAlpha(ctx.layer.stroke, rect.alpha), ImDrawFlags_Closed,
            ctx.layer.strokeWidth);
}

} // namespace

namespace demo
{

namespace quad
{

float wanderOffset(uint32_t seed, int vertex, int axis, float t, float speed)
{
    // speed <= 0 表示"不动"（回到中心，也就是矩形本身）
    if (speed <= 0.0f)
        return 0.0f;

    const float ticks  = t * speed;
    const float bucket = std::floor(ticks);
    const float frac   = smoothstep(ticks - bucket);

    // 每个顶点、每个轴一条独立的随机序列（只靠哈希混合，不需要状态）
    const uint32_t base = seed * 2654435761u + (uint32_t)vertex * 97u + (uint32_t)axis * 31u;
    const float a = hash01(base + (uint32_t)(int32_t)bucket);
    const float b = hash01(base + (uint32_t)(int32_t)bucket + 1u);
    return (a + (b - a) * frac) * 2.0f - 1.0f;
}

VertexOffsets vertexOffsets(uint32_t seed, float t, float speed, const float* gain)
{
    VertexOffsets out;
    for (int i = 0; i < 4; i++)
    {
        const float g = gain != nullptr ? gain[i] : 1.0f;
        out.x[i]      = wanderOffset(seed, i, 0, t, speed) * g;
        out.y[i]      = wanderOffset(seed, i, 1, t, speed) * g;
    }
    return out;
}

} // namespace quad

void registerQuadLayer()
{
    bklv::compile::registerLayer<QuadLayer>();
}

} // namespace demo

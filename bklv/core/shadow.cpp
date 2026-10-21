#include "bklv/core/shadow.hpp"

#include <cmath>
#include <cstdlib>
#include <vector>

#include "bklv/core/logger.hpp"

namespace bklv
{
namespace shadow
{
namespace
{

ShadowQuality current = ShadowQuality::FAST;
bool probed          = false;

void probeEnvironment()
{
    if (probed)
        return;
    probed = true;

    const char* requested = std::getenv("BKLV_SHADOW_QUALITY");
    if (requested == nullptr || *requested == '\0')
        return;

    if (requested[0] == 's' || requested[0] == 'S')
    {
        current = ShadowQuality::SMOOTH;
        BKLV_LOG_INFO("阴影档位：smooth（环境变量 BKLV_SHADOW_QUALITY）");
    }
    else if (requested[0] == 'f' || requested[0] == 'F')
    {
        current = ShadowQuality::FAST;
        BKLV_LOG_INFO("阴影档位：fast（环境变量 BKLV_SHADOW_QUALITY）");
    }
    else
    {
        BKLV_LOG_WARNING("BKLV_SHADOW_QUALITY=%s 不认识（可用 fast / smooth），按 fast 处理",
            requested);
    }
}

} // namespace

ShadowQuality getQuality()
{
    probeEnvironment();
    return current;
}

void setQuality(ShadowQuality quality)
{
    probed  = true;
    current = quality;
}

bool setQualityByName(const std::string& name)
{
    if (name == "fast" || name == "FAST")
    {
        setQuality(ShadowQuality::FAST);
        return true;
    }
    if (name == "smooth" || name == "SMOOTH")
    {
        setQuality(ShadowQuality::SMOOTH);
        return true;
    }
    return false;
}

const char* qualityName(ShadowQuality quality)
{
    return quality == ShadowQuality::SMOOTH ? "smooth" : "fast";
}

int layerCount()
{
    return getQuality() == ShadowQuality::SMOOTH ? 24 : 8;
}

void drawSoft(ImDrawList* drawList, float minX, float minY, float maxX, float maxY, float radius,
    ImU32 color, float alphaScale, float feather, float spread, bool cutout, float offsetX,
    float offsetY)
{
    if (drawList == nullptr || maxX <= minX || maxY <= minY)
        return;

    const int steps = layerCount();

    const float target = (float)((color >> 24) & 0xFF) / 255.0f * alphaScale;
    if (target <= 0.001f)
        return;

    // 高斯权重：贴着形状的那层最实，最外层最淡
    float weights[24];
    float weightSum = 0.0f;
    const float span = (float)(steps > 1 ? steps - 1 : 1);
    for (int i = 0; i < steps; i++)
    {
        const float t = 1.0f - (float)i / span; // 1 = 最外
        weights[i]    = std::exp(-4.5f * t * t);
        weightSum += weights[i];
    }

    // 每层 alpha 之和 = 目标 alpha：叠加后的实际不透明度约等于 1-exp(-target)，
    // 在阴影常用的 0.2~0.4 区间偏差 <10%，够用且不引入魔数
    const float unit       = 255.0f * target / (weightSum > 0.0f ? weightSum : 1.0f);
    const unsigned int rgb = color & 0x00FFFFFFu;

    // 从最外层往内画：内层后画才盖在上面
    for (int i = 0; i < steps; i++)
    {
        const float t      = 1.0f - (float)i / span;
        const float expand = spread + feather * t;

        const unsigned int alpha = (unsigned int)std::lround(unit * weights[i]);
        if (alpha == 0)
            continue;

        const float outerMinX = minX + offsetX - expand;
        const float outerMinY = minY + offsetY - expand;
        const float outerMaxX = maxX + offsetX + expand;
        const float outerMaxY = maxY + offsetY + expand;
        const ImU32 col       = rgb | (alpha << 24);

        if (!cutout)
        {
            drawList->AddRectFilled(ImVec2(outerMinX, outerMinY), ImVec2(outerMaxX, outerMaxY),
                col, radius + expand);
            continue;
        }

        // cutout：只画**控件矩形之外**的阴影。
        // 阴影形状跟着 offset 走（外投影该往下就往下），但挖空区域始终是控件自己的
        // 矩形 —— 两者是不同位置，不能像以前那样沿同一个矩形描边：
        //   · 沿阴影矩形描边 → 孔跟着阴影跑，看起来"从中间把上层 layer 剪掉一块"；
        //   · 沿控件矩形描边 → offset 被完全吃掉（"设置 offset 无效"）。
        // 做法：把同一个外扩圆角矩形分成四块（上/下/左/右，都在控件矩形之外），
        // 每块用 PushClipRect 裁一下再画。四块互不重叠，圆角形状不受影响。
        const ImVec2 outerMin(minX + offsetX - expand, minY + offsetY - expand);
        const ImVec2 outerMax(maxX + offsetX + expand, maxY + offsetY + expand);
        struct Band
        {
            float x0, y0, x1, y1;
        };
        const Band bands[4] = {
            { outerMin.x, outerMin.y, outerMax.x, minY }, // 上
            { outerMin.x, maxY, outerMax.x, outerMax.y }, // 下
            { outerMin.x, minY, minX, maxY },             // 左
            { maxX, minY, outerMax.x, maxY },             // 右
        };
        for (const Band& band : bands)
        {
            if (band.x1 <= band.x0 || band.y1 <= band.y0)
                continue;
            drawList->PushClipRect(ImVec2(band.x0, band.y0), ImVec2(band.x1, band.y1), true);
            drawList->AddRectFilled(outerMin, outerMax, col, radius + expand);
            drawList->PopClipRect();
        }
    }
}

namespace
{

// 圆角矩形有符号距离：< 0 在内部，> 0 在外部（和 shader 里那份 sdRoundedRect 同一套公式）
float roundedRectSdf(float px, float py, float cx, float cy, float halfW, float halfH, float r)
{
    const float radius  = std::max(0.0f, std::min(r, std::min(halfW, halfH)));
    const float qx      = std::fabs(px - cx) - (halfW - radius);
    const float qy      = std::fabs(py - cy) - (halfH - radius);
    const float outside = std::sqrt(std::max(qx, 0.0f) * std::max(qx, 0.0f)
        + std::max(qy, 0.0f) * std::max(qy, 0.0f));
    return outside + std::min(std::max(qx, qy), 0.0f) - radius;
}

float smoothstep01(float t)
{
    const float c = std::max(0.0f, std::min(1.0f, t));
    return c * c * (3.0f - 2.0f * c);
}

constexpr int kInnerCornerSegments = 8; // 每角 8 段 -> 一圈 36 个点

// 一圈圆角矩形边界点（相邻角的端点之间就是直边）
void appendRoundedRectBoundary(std::vector<ImVec2>& out, float minX, float minY, float maxX,
    float maxY, float radius)
{
    const float width  = maxX - minX;
    const float height = maxY - minY;
    const float corner = std::max(0.0f, std::min(radius, std::min(width, height) * 0.5f));
    const float cx[4] = { minX + corner, maxX - corner, maxX - corner, minX + corner };
    const float cy[4] = { minY + corner, minY + corner, maxY - corner, maxY - corner };
    const float start[4] = { 180.0f, 270.0f, 0.0f, 90.0f };

    for (int c = 0; c < 4; c++)
    {
        for (int i = 0; i <= kInnerCornerSegments; i++)
        {
            const float radians = (start[c] + 90.0f * (float)i / kInnerCornerSegments)
                * 3.14159265358979323846f / 180.0f;
            out.push_back(ImVec2(
                cx[c] + std::cos(radians) * corner, cy[c] + std::sin(radians) * corner));
        }
    }
}

} // namespace

void drawInnerSoft(ImDrawList* drawList, float minX, float minY, float maxX, float maxY,
    float radius, ImU32 color, float alphaScale, float feather, float spread)
{
    if (drawList == nullptr || maxX <= minX || maxY <= minY)
        return;

    // 渐隐宽度：spread 与 feather 都算"往内伸多远"
    const float reach = std::max(0.0f, spread) + std::max(0.0f, feather);
    if (reach <= 0.01f)
        return;

    const float target = (float)((color >> 24) & 0xFFu) / 255.0f * alphaScale;
    if (target <= 0.001f)
        return;

    const float width   = maxX - minX;
    const float height  = maxY - minY;
    const float halfMax = std::min(width, height) * 0.5f;

    // 要覆盖的深度 = 渐隐宽度。夹到短边的一半：再深两边就碰上了。
    const float depth = std::min(reach, std::max(halfMax - 0.5f, 0.0f));
    if (depth <= 0.5f)
        return;

    // 内阴影**没有方向**：以形状自己的轮廓为基准，贴内缘最暗、往内渐透。
    // 距离用圆角矩形 SDF 算，所以是"到轮廓的等宽内缩"——四个角不会像"到最近边的距离"
    // 那样叠得更黑，圆角也能正确跟随（这是选 SDF 而不是 min(dx,dy) 的原因）。
    const float centerX = (minX + maxX) * 0.5f;
    const float centerY = (minY + maxY) * 0.5f;
    const float halfW   = width * 0.5f;
    const float halfH   = height * 0.5f;

    const int rings = (getQuality() == ShadowQuality::SMOOTH) ? 8 : 5;
    const float span = (float)(rings - 1);

    // 每一圈：形状按 depth * k/span 内缩后的圆角矩形边界点 + 每点的 alpha。
    //
    // 为什么用网格逐顶点算 alpha 而不是描边：PathStroke 一个路径只有一个颜色，
    // 而这里的 alpha 要按"离轮廓多远"逐点算（圆角上各点不同、直边上相同）。
    // 顶点 alpha 精确算、圈与圈之间线性插值 —— 和 core/gradient.cpp 的网格是同一套路子。
    std::vector<ImVec2> outerBoundary;
    std::vector<ImVec2> innerBoundary;
    std::vector<unsigned int> outerAlpha;
    std::vector<unsigned int> innerAlpha;

    auto buildRing = [&](int ringIndex, std::vector<ImVec2>& points,
                         std::vector<unsigned int>& alphas, float& outInset) {
        const float inset = depth * (float)ringIndex / span;
        outInset          = inset;
        points.clear();
        alphas.clear();
        appendRoundedRectBoundary(points, minX + inset, minY + inset, maxX - inset,
            maxY - inset, std::max(0.0f, radius - inset));

        for (const ImVec2& point : points)
        {
            const float distance = roundedRectSdf(point.x, point.y, centerX, centerY, halfW,
                halfH, radius);
            // 内缘 distance = 0 -> 1（最暗）；深入 distance = -reach -> 0；再深夹到 0
            const float rel = smoothstep01((distance + reach) / reach);
            alphas.push_back((unsigned int)std::lround(255.0f * target * rel));
        }
    };

    const unsigned int rgb = color & 0x00FFFFFFu;
    for (int ringIndex = 0; ringIndex < rings - 1; ringIndex++)
    {
        float outerInset = 0.0f;
        float innerInset = 0.0f;
        buildRing(ringIndex, outerBoundary, outerAlpha, outerInset);
        buildRing(ringIndex + 1, innerBoundary, innerAlpha, innerInset);
        if (outerBoundary.size() != innerBoundary.size() || outerBoundary.empty())
            continue;

        const size_t count = outerBoundary.size();
        for (size_t i = 0; i < count; i++)
        {
            const size_t next = (i + 1) % count;
            // 两个三角形：外[i] 外[i+1] 内[i+1] + 外[i] 内[i+1] 内[i]
            // 每个三角形现写三个顶点 —— 索引天然是 3 的倍数（见 gradient.cpp 里那次踩坑）
            drawList->PrimReserve(6, 6);
            const ImVec2 quad[6] = { outerBoundary[i], outerBoundary[next],
                innerBoundary[next], outerBoundary[i], innerBoundary[next], innerBoundary[i] };
            const unsigned int alphaOf[6] = { outerAlpha[i], outerAlpha[next], innerAlpha[next],
                outerAlpha[i], innerAlpha[next], innerAlpha[i] };
            for (int v = 0; v < 6; v++)
                drawList->PrimVtx(quad[v], ImVec2(0.0f, 0.0f),
                    rgb | (alphaOf[v] << 24));
        }
    }
}

void resetEnvironmentProbeForTest()
{
    probed = false;
}

} // namespace shadow
} // namespace bklv

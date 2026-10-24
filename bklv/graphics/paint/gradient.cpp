#include "bklv/graphics/paint/gradient.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace bklv
{

namespace
{

// 大小写与 '_'/'-' 都不敏感的比较（和全库 enumMatches 同一套语义）
bool matches(const std::string& text, const char* name) noexcept
{
    size_t i = 0;
    for (; i < text.size() && name[i] != 0; i++)
    {
        char a = text[i];
        char b = name[i];
        if (a == '-' || a == '_')
            a = '_';
        if (b == '-' || b == '_')
            b = '_';
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if (a != b)
            return false;
    }
    return i == text.size() && name[i] == 0;
}

float clamp01(float value) noexcept
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

int channel(ImU32 color, int shift) noexcept
{
    return (int)((color >> shift) & 0xFFu);
}

} // namespace

// 把渐变画成**顶点色网格**：只是 ImDrawList 顶点，所以 GL / Switch-GLES3 / Vulkan 通吃。
//
// 两条路径：
//   · 直角（radius <= 0.5）：轴对齐角度只发 4 个顶点（精确）；斜角切 8x8 网格逐顶点采样。
//   · 圆角：沿圆角矩形边界取样 + 中心点做**三角扇**（线性渐变在扇形上精确），
//     再沿边界补一圈"外扩一个羽边、alpha=0"的顶点做抗锯齿（手写网格没有 ImGui 的自动羽边）。
void addGradientRectMesh(ImDrawList* drawList, const Gradient& gradient, const ImVec2& min,
    const ImVec2& max, float radius, float alphaScale)
{
    if (drawList == nullptr || !gradient.isValid() || max.x <= min.x || max.y <= min.y
        || alphaScale <= 0.001f)
        return;

    const ImVec2 whiteUv = ImGui::GetFontTexUvWhitePixel();
    const float fringe   = drawList->_FringeScale > 0.0f ? drawList->_FringeScale : 1.0f;
    const float width    = max.x - min.x;
    const float height   = max.y - min.y;
    const float corner   = std::max(0.0f, std::min(radius, std::min(width, height) * 0.5f));

    // 位置 → 颜色（t 由渐变决定；alpha 再乘这一层的整体透明度）
    auto colorAt = [&](const ImVec2& point) -> ImU32 {
        const ImU32 sampled = sampleGradient(gradient, gradientParameter(gradient, point, min, max));
        const float alpha   = (float)((sampled >> 24) & 0xFFu) * alphaScale;
        return (sampled & 0x00FFFFFFu)
            | ((ImU32)std::lround(std::max(0.0f, std::min(255.0f, alpha))) << 24);
    };

    if (corner <= 0.5f)
    {
        // 角度是否轴对齐（0/90/180/270）
        const float normalized = std::fmod(std::fmod(gradient.angle, 360.0f) + 360.0f, 360.0f);
        const bool axisAligned = std::fabs(normalized - std::lround(normalized / 90.0f) * 90.0f)
            < 0.01f;

        if (axisAligned)
        {
            const ImVec2 corners[4] = { ImVec2(min.x, min.y), ImVec2(max.x, min.y),
                ImVec2(max.x, max.y), ImVec2(min.x, max.y) };
            drawList->PrimReserve(6, 6);
            const int order[6] = { 0, 1, 2, 0, 2, 3 };
            for (int i = 0; i < 6; i++)
                drawList->PrimVtx(corners[order[i]], whiteUv, colorAt(corners[order[i]]));
            return;
        }

        // 斜角：8x8 网格逐顶点采样（每个格子两个三角形，顶点不复用 —— 换实现简单）
        const int cells = 8;
        drawList->PrimReserve(cells * cells * 6, cells * cells * 6);
        for (int cy = 0; cy < cells; cy++)
        {
            for (int cx = 0; cx < cells; cx++)
            {
                const ImVec2 p00(min.x + width * (float)cx / cells, min.y + height * (float)cy / cells);
                const ImVec2 p10(min.x + width * (float)(cx + 1) / cells, p00.y);
                const ImVec2 p11(p10.x, min.y + height * (float)(cy + 1) / cells);
                const ImVec2 p01(p00.x, p11.y);
                const ImVec2 quad[6] = { p00, p10, p11, p00, p11, p01 };
                for (const ImVec2& point : quad)
                    drawList->PrimVtx(point, whiteUv, colorAt(point));
            }
        }
        return;
    }

    // 圆角：沿边界取样（每角 8 段；直边各 1 段，边界点不重复）
    const int kCornerSegments = 8;
    std::vector<ImVec2> boundary;
    boundary.reserve(4 * (kCornerSegments + 1) + 4);
    struct CornerSpec
    {
        ImVec2 center;
        float  startAngle; // 度
    };
    const CornerSpec corners[4] = {
        { ImVec2(min.x + corner, min.y + corner), 180.0f }, // 左上
        { ImVec2(max.x - corner, min.y + corner), 270.0f }, // 右上
        { ImVec2(max.x - corner, max.y - corner), 0.0f },   // 右下
        { ImVec2(min.x + corner, max.y - corner), 90.0f },  // 左下
    };
    for (const CornerSpec& spec : corners)
    {
        for (int i = 0; i <= kCornerSegments; i++)
        {
            const float radians = (spec.startAngle + 90.0f * (float)i / kCornerSegments)
                * 3.14159265358979323846f / 180.0f;
            boundary.push_back(ImVec2(spec.center.x + std::cos(radians) * corner,
                spec.center.y + std::sin(radians) * corner));
        }
    }

    const ImVec2 center(min.x + width * 0.5f, min.y + height * 0.5f);
    const int boundaryCount = (int)boundary.size();

    // 扇形本体：每个三角形 = 中心 + 相邻的两个边界点。
    //
    // 索引必须**按 3 成组**发出。原因：ImGui 把一条 draw list 的**所有**图元合并进一个
    // ImDrawCmd，渲染时一次 `glDrawElementsBaseVertex(GL_TRIANGLES, ElemCount, …)`，
    // 索引按连续三元组消费 —— 只要有一段图元发出的索引不是 3 的倍数，**它之后的所有三角形
    // 都会整体错位**、顶点配错。踩过：这里原本是 `PrimReserve(boundaryCount + 2, …)` +
    // 逐点 PrimVtx（PrimVtx = 写一个索引 + 一个顶点），总索引 = 36 + 2 = 38，38 % 3 == 2，
    // 症状是渐变卡所在滚动区里的**文字被切成横条 + 斜向短划**；
    // 而且那 38 个索引里只有第一个三角形是对的，其余是边界点之间的碎片 —— 渐变内部其实
    // 没填满（卡片内部偏暗）。
    //
    // 现在每个三角形的三个点都现写（边界点会重复写，多几十个顶点）：换来实现简单、
    // 天然 3 对齐，代价可忽略（斜角网格那条路径本来就发 384 个顶点）。
    const ImU32 centerColor = colorAt(center);
    drawList->PrimReserve(boundaryCount * 3, boundaryCount * 3);
    for (int i = 0; i < boundaryCount; i++)
    {
        const ImVec2& a = boundary[i];
        const ImVec2& b = boundary[(i + 1) % boundaryCount];
        drawList->PrimVtx(center, whiteUv, centerColor);
        drawList->PrimVtx(a, whiteUv, colorAt(a));
        drawList->PrimVtx(b, whiteUv, colorAt(b));
    }

    // 抗锯齿：每个边界点沿"远离中心"的方向外扩一个羽边，alpha = 0
    drawList->PrimReserve(boundaryCount * 6, boundaryCount * 6);
    auto outerPoint = [&](const ImVec2& point) -> ImVec2 {
        const float dx = point.x - center.x;
        const float dy = point.y - center.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0001f)
            return point;
        return ImVec2(point.x + dx / length * fringe, point.y + dy / length * fringe);
    };
    for (int i = 0; i < boundaryCount; i++)
    {
        const ImVec2& a = boundary[i];
        const ImVec2& b = boundary[(i + 1) % boundaryCount];
        const ImVec2 ao = outerPoint(a);
        const ImVec2 bo = outerPoint(b);
        const ImU32 ca = colorAt(a);
        const ImU32 cb = colorAt(b);
        struct FringeVtx
        {
            ImVec2 pos;
            ImU32  col;
        };
        // 内圈用真实颜色、外圈把 alpha 抹成 0 —— 手写网格的"羽边"
        const FringeVtx strip[6] = { { a, ca }, { b, cb }, { bo, cb & 0x00FFFFFFu },
            { a, ca }, { bo, cb & 0x00FFFFFFu }, { ao, ca & 0x00FFFFFFu } };
        for (const FringeVtx& vtx : strip)
            drawList->PrimVtx(vtx.pos, whiteUv, vtx.col);
    }
}

bool gradientTypeFromName(const std::string& name, Gradient::Type& out)
{
    if (matches(name, "linear"))
    {
        out = Gradient::Type::LINEAR;
        return true;
    }
    if (matches(name, "radial"))
    {
        out = Gradient::Type::RADIAL;
        return true;
    }
    return false;
}

const char* gradientTypeName(Gradient::Type type) noexcept
{
    return type == Gradient::Type::RADIAL ? "radial" : "linear";
}

void gradientSegment(const Gradient& gradient, float t, int& segmentIndex, float& localT) noexcept
{
    segmentIndex = 0;
    localT       = 0.0f;
    if (!gradient.isValid())
        return;

    const float clamped = clamp01(t);
    segmentIndex        = gradient.stopCount - 2;
    for (int i = 0; i < gradient.stopCount - 1; i++)
    {
        if (clamped <= gradient.stops[i + 1].at)
        {
            segmentIndex = i;
            break;
        }
    }

    const float from = gradient.stops[segmentIndex].at;
    const float to   = gradient.stops[segmentIndex + 1].at;
    localT           = to > from ? (clamped - from) / (to - from) : 0.0f;
}

ImU32 sampleGradient(const Gradient& gradient, float t) noexcept
{
    if (!gradient.isValid())
        return 0;

    int segment  = 0;
    float localT = 0.0f;
    gradientSegment(gradient, t, segment, localT);

    const ImU32 from = gradient.stops[segment].color;
    const ImU32 to   = gradient.stops[segment + 1].color;

    // 逐通道线性插值（含 alpha：透明度也要跟着渐变走）
    int out[4];
    for (int i = 0; i < 4; i++)
    {
        const int shift = i * 8;
        const int a     = channel(from, shift);
        const int b     = channel(to, shift);
        out[i]          = (int)std::lround((float)a + ((float)b - (float)a) * localT);
    }
    return IM_COL32(out[0], out[1], out[2], out[3]);
}

float gradientParameter(const Gradient& gradient, const ImVec2& point, const ImVec2& min,
    const ImVec2& max) noexcept
{
    const float width  = max.x - min.x;
    const float height = max.y - min.y;
    if (width <= 0.0f || height <= 0.0f)
        return 0.0f;

    if (gradient.type == Gradient::Type::RADIAL)
    {
        const float cx = min.x + width * gradient.center[0];
        const float cy = min.y + height * gradient.center[1];
        const float dx = point.x - cx;
        const float dy = point.y - cy;
        // 用长边归一：矩形被拉长时渐变不会被拉成椭圆
        const float radius = std::max(1.0f, std::max(width, height) * 0.5f);
        return std::sqrt(dx * dx + dy * dy) / radius;
    }

    // LINEAR：把点投到方向上。角度按顺时针，0 = 左→右，90 = 上→下。
    const float radians = gradient.angle * 3.14159265358979323846f / 180.0f;
    const float dirX    = std::cos(radians);
    const float dirY    = std::sin(radians);

    // 投影范围按矩形**四个角的投影**取最小/最大，保证 t 正好铺满 0..1。
    // （不能把 width*dirX 与 height*dirY 分开比较：方向带负分量时那个式子会错 —— 45° 实测 t 会到 1.5）
    const float cornerX[4] = { 0.0f, width, width, 0.0f };
    const float cornerY[4] = { 0.0f, 0.0f, height, height };
    float start = 0.0f;
    float end   = 0.0f;
    for (int i = 0; i < 4; i++)
    {
        const float projection = cornerX[i] * dirX + cornerY[i] * dirY;
        start = i == 0 ? projection : std::min(start, projection);
        end   = i == 0 ? projection : std::max(end, projection);
    }

    const float projection = (point.x - min.x) * dirX + (point.y - min.y) * dirY;
    const float span       = end - start;
    if (span <= 0.0001f)
        return 0.0f;
    return (projection - start) / span;
}

} // namespace bklv

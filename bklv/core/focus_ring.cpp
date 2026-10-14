#include "bklv/core/focus_ring.hpp"

#include <algorithm>
#include <cmath>

namespace bklv
{
namespace focus_ring
{

namespace
{

constexpr float kPi    = 3.14159265358979f;
constexpr float kHalfPi = 1.5707963267949f;

// 半径钳到短边的一半：超过就退化成胶囊/正圆。
float clampedRadius(const Outline& outline)
{
    const float halfMin = std::min(outline.bounds.width, outline.bounds.height) * 0.5f;
    const float radius  = outline.cornerRadius * outline.scale;
    return std::max(0.0f, std::min(radius, std::max(0.0f, halfMin)));
}

// 一段圆弧上取点：angle 从"圆心正上方"顺时针算，和圆角矩形的四个角对上。
Point arcPoint(float cx, float cy, float radius, float angle)
{
    return Point(cx + std::sin(angle) * radius, cy - std::cos(angle) * radius);
}

float wrap01(float value)
{
    const float wrapped = std::fmod(value, 1.0f);
    return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
}

// 段的两侧偏移（用段的法线近似，段很短看不出接缝）
struct SegmentQuad
{
    Point outerFrom;
    Point outerTo;
    Point innerTo;
    Point innerFrom;
};

SegmentQuad makeQuad(const Point& p0, const Point& p1, float half)
{
    float nx = -(p1.y - p0.y);
    float ny = p1.x - p0.x;
    const float length = std::sqrt(nx * nx + ny * ny);
    if (length > 1e-6f)
    {
        nx = nx / length * half;
        ny = ny / length * half;
    }
    else
    {
        nx = 0.0f;
        ny = 0.0f;
    }

    SegmentQuad quad;
    quad.outerFrom = Point(p0.x + nx, p0.y + ny);
    quad.outerTo   = Point(p1.x + nx, p1.y + ny);
    quad.innerTo   = Point(p1.x - nx, p1.y - ny);
    quad.innerFrom = Point(p0.x - nx, p0.y - ny);
    return quad;
}

// 一个贴图片段：四个角 + 四个 UV
void addTexturedQuad(ImDrawList* drawList, const SegmentQuad& quad, const TextureRef& lut,
    float u0, float u1, ImU32 tint)
{
    drawList->AddImageQuad(lut.texture,
        ImVec2(quad.outerFrom.x, quad.outerFrom.y), ImVec2(quad.outerTo.x, quad.outerTo.y),
        ImVec2(quad.innerTo.x, quad.innerTo.y), ImVec2(quad.innerFrom.x, quad.innerFrom.y),
        ImVec2(u0, 0.0f), ImVec2(u1, 0.0f), ImVec2(u1, 1.0f), ImVec2(u0, 1.0f), tint);
}

} // namespace

float phaseForTime(float seconds, float flowSeconds)
{
    if (flowSeconds <= 0.0f)
        return 0.0f;

    return wrap01(seconds / flowSeconds);
}

float perimeter(const Outline& outline)
{
    const float w = outline.bounds.width;
    const float h = outline.bounds.height;
    const float r = clampedRadius(outline);

    // 四条直边（各扣掉两个圆角）+ 四个 1/4 圆角
    const float straight = std::max(0.0f, w - 2.0f * r) + std::max(0.0f, h - 2.0f * r);
    return 2.0f * straight + 2.0f * kPi * r;
}

Point pointAt(const Outline& outline, float t)
{
    const Rect& bounds = outline.bounds;
    const float r      = clampedRadius(outline);

    const float straightW = std::max(0.0f, bounds.width - 2.0f * r);  // 上下边直段
    const float straightH = std::max(0.0f, bounds.height - 2.0f * r); // 左右边直段
    const float arcLen    = kHalfPi * r;                              // 每个圆角
    const float total     = 2.0f * straightW + 2.0f * straightH + 4.0f * arcLen;
    if (total <= 0.0f)
        return Point(bounds.x, bounds.y);

    float d = wrap01(t) * total;

    const float left   = bounds.x;
    const float right  = bounds.x + bounds.width;
    const float top    = bounds.y;
    const float bottom = bounds.y + bounds.height;

    // 上边（含右上角起点处的直段）
    if (d < straightW)
        return Point(left + r + d, top);
    d -= straightW;

    // 右上角
    if (d < arcLen)
        return arcPoint(right - r, top + r, r, d / std::max(arcLen, 1e-6f) * kHalfPi);
    d -= arcLen;

    // 右边
    if (d < straightH)
        return Point(right, top + r + d);
    d -= straightH;

    // 右下角
    if (d < arcLen)
        return arcPoint(right - r, bottom - r, r, kHalfPi + d / std::max(arcLen, 1e-6f) * kHalfPi);
    d -= arcLen;

    // 下边
    if (d < straightW)
        return Point(right - r - d, bottom);
    d -= straightW;

    // 左下角
    if (d < arcLen)
        return arcPoint(left + r, bottom - r, r, kPi + d / std::max(arcLen, 1e-6f) * kHalfPi);
    d -= arcLen;

    // 左边
    if (d < straightH)
        return Point(left, bottom - r - d);
    d -= straightH;

    // 左上角（回到起点）
    return arcPoint(left + r, top + r, r, kPi + kHalfPi + d / std::max(arcLen, 1e-6f) * kHalfPi);
}

int segmentCount(const Outline& outline, const Style& style)
{
    if (style.segments > 0)
        return std::max(3, style.segments);

    // 约每 6px 一段：小控件不至于太粗糙，大控件也不至于顶点爆掉。
    const int automatic = (int)std::ceil(perimeter(outline) / 6.0f);
    return std::max(kMinSegments, std::min(kMaxSegments, automatic));
}

LutSpan lutSpanAt(float t0, float t1, float phase)
{
    LutSpan span;
    span.u0      = wrap01(t0 + phase);
    span.u1      = wrap01(t1 + phase);
    span.wrapped = span.u1 < span.u0;
    return span;
}

void draw(ImDrawList* drawList, const Outline& outline, const Style& style, float phase)
{
    if (drawList == nullptr || style.type == Type::NONE || style.alpha <= 0.0f
        || style.lineWidth <= 0.0f)
        return;

    // 框画在控件**外侧**：把形状整体外扩（缝 + 半个线宽），中线就落在描边正中间。
    // 圆形控件（圆角半径 = 短边一半）外扩后仍然是圆 —— 形状自然是跟着控件走的。
    const float grow = style.expand + style.lineWidth * 0.5f;
    Outline ring;
    ring.bounds = Rect(outline.bounds.x - grow, outline.bounds.y - grow,
        outline.bounds.width + grow * 2.0f, outline.bounds.height + grow * 2.0f);
    ring.cornerRadius = outline.cornerRadius + grow;
    ring.scale        = outline.scale;

    if (ring.bounds.width <= 0.0f || ring.bounds.height <= 0.0f)
        return;

    if (style.type == Type::CUSTOM)
    {
        if (style.customRenderer)
            style.customRenderer(drawList, ring, style, phase);
        return;
    }

    // 颜色只来自贴图：没加载成功就不画（不退回"自己算的颜色"）。
    if (!style.lut.isValid())
        return;

    const int segments = segmentCount(ring, style);
    const float half   = style.lineWidth * 0.5f;

    // 三种类型的区别只在"动什么"：
    //   FLOW      相位推动贴图（颜色绕圈流动）
    //   BREATHING 贴图不动，只有整体透明度在起伏
    //   SOLID     都不动
    float segmentAlpha = style.alpha;
    float segmentPhase = phase;
    if (style.type == Type::BREATHING)
    {
        const float pulse = 0.45f + 0.55f * (0.5f + 0.5f * std::sin(phase * 2.0f * kPi));
        segmentAlpha      = style.alpha * pulse;
        segmentPhase      = 0.0f;
    }
    else if (style.type == Type::SOLID)
    {
        segmentPhase = 0.0f;
    }

    const ImU32 tint = colorWithAlpha(IM_COL32_WHITE, segmentAlpha);

    for (int i = 0; i < segments; i++)
    {
        const float t0 = (float)i / (float)segments;
        const float t1 = (float)(i + 1) / (float)segments;

        const Point p0 = pointAt(ring, t0);
        const Point p1 = pointAt(ring, t1);
        const SegmentQuad quad = makeQuad(p0, p1, half);

        const LutSpan span = lutSpanAt(t0, t1, segmentPhase);
        if (!span.wrapped)
        {
            addTexturedQuad(drawList, quad, style.lut, span.u0, span.u1, tint);
            continue;
        }

        // 跨接缝：按 U 的比例切开，画成两段，保证 UV 单调、不被拉花
        const float length = 1.0f - span.u0 + span.u1;
        const float ratio  = length > 1e-6f ? (1.0f - span.u0) / length : 0.5f;

        const Point pm(p0.x + (p1.x - p0.x) * ratio, p0.y + (p1.y - p0.y) * ratio);
        addTexturedQuad(drawList, makeQuad(p0, pm, half), style.lut, span.u0, 1.0f, tint);

        // u1 正好是 0（最后一段的收尾）时后半段长度为 0，跳过它
        if (span.u1 > 1e-4f)
            addTexturedQuad(drawList, makeQuad(pm, p1, half), style.lut, 0.0f, span.u1, tint);
    }
}

} // namespace focus_ring
} // namespace bklv

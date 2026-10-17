#include "bklv/core/focus_ring.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace bklv
{
namespace focus_ring
{

namespace
{

constexpr float kPi    = 3.14159265358979f;

// 中线每段的目标弧长（px）：和 segmentCount() 的"每 6px 一段"同口径。
constexpr float kTargetSegmentPixels = 6.0f;

// 一个圆角至少切几段（8 段 = 每段 11.25°）。半径很小时按"每 6px 一段"只会切出 3~4 段，
// 圆角看着就是几根折线；固定下限让任何半径都圆。
constexpr int kMinCornerSamples = 8;
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

// 一个采样点：中线上的点 + 该点的**单位外法线**。
struct Sample
{
    Point point;
    Point normal;
    float u = 0.0f; // 沿中线的归一化位置（0..1），贴图 U 用它
};

// 中线采样：直边与圆角**分开**细分。
//
// 为什么不能整体按弧长均分：
//   ① 小半径的圆角只有十几像素，均分时可能整个圆角落在一段里 —— 那一段就是弦，
//      圆角被"削平"成切角（用户报的"精度有问题"就是这个观感）；
//   ② 采样点不一定落在"直边 ↔ 圆角"的交界上，交界处会多出一个不该有的折点。
// 分开细分保证：每个圆角至少 kMinCornerSamples 段、采样点正好落在 8 个特征交界上。
std::vector<Sample> buildSamples(const Outline& ring, float step)
{
    const Rect& bounds = ring.bounds;
    const float r      = clampedRadius(ring);
    const float left   = bounds.x;
    const float right  = bounds.x + bounds.width;
    const float top    = bounds.y;
    const float bottom = bounds.y + bounds.height;

    const float straightW = std::max(0.0f, bounds.width - 2.0f * r);  // 上下边直段
    const float straightH = std::max(0.0f, bounds.height - 2.0f * r); // 左右边直段
    const float arcLen    = kHalfPi * r;

    auto spansFor = [step](float length, int minimum) {
        if (length <= 0.0001f)
            return 0;
        return std::max(minimum, (int)std::ceil(length / std::max(step, 0.5f)));
    };

    const int wSpans = spansFor(straightW, 1);
    const int hSpans = spansFor(straightH, 1);
    const int aSpans = r > 0.01f ? spansFor(arcLen, kMinCornerSamples) : 0;

    // 每个特征只放"起点"，末点由下一个特征补上 —— 这样正好是闭环、没有重复点。
    std::vector<Point> points;
    points.reserve((size_t)(2 * wSpans + 2 * hSpans + 4 * aSpans));

    for (int i = 0; i < wSpans; i++) // 上边：左 → 右
        points.push_back(Point(left + r + straightW * (float)i / (float)wSpans, top));
    for (int i = 0; i < aSpans; i++) // 右上角
        points.push_back(arcPoint(right - r, top + r, r, kHalfPi * (float)i / (float)aSpans));
    for (int i = 0; i < hSpans; i++) // 右边：上 → 下
        points.push_back(Point(right, top + r + straightH * (float)i / (float)hSpans));
    for (int i = 0; i < aSpans; i++) // 右下角
        points.push_back(
            arcPoint(right - r, bottom - r, r, kHalfPi + kHalfPi * (float)i / (float)aSpans));
    for (int i = 0; i < wSpans; i++) // 下边：右 → 左
        points.push_back(Point(right - r - straightW * (float)i / (float)wSpans, bottom));
    for (int i = 0; i < aSpans; i++) // 左下角
        points.push_back(arcPoint(left + r, bottom - r, r, kPi + kHalfPi * (float)i / (float)aSpans));
    for (int i = 0; i < hSpans; i++) // 左边：下 → 上
        points.push_back(Point(left, bottom - r - straightH * (float)i / (float)hSpans));
    for (int i = 0; i < aSpans; i++) // 左上角
        points.push_back(
            arcPoint(left + r, top + r, r, kPi + kHalfPi + kHalfPi * (float)i / (float)aSpans));

    // 顶点法线 = 相邻两条边法线的角平分线，再按 1/cos(θ/2) 拉长（miter）。
    // 不拉长的话拐角外侧会缺一块、描边在转角处变窄；半径 0 的直角最明显。
    const size_t count = points.size();
    std::vector<Sample> samples(count);
    if (count == 0)
        return samples;

    auto edgeNormal = [&points, count](size_t i) {
        const Point& a = points[i];
        const Point& b = points[(i + 1) % count];
        float nx = -(b.y - a.y);
        float ny = b.x - a.x;
        const float length = std::sqrt(nx * nx + ny * ny);
        if (length > 1e-6f)
        {
            nx /= length;
            ny /= length;
        }
        return Point(nx, ny);
    };

    float accumulated = 0.0f;
    for (size_t i = 0; i < count; i++)
    {
        const Point prev = edgeNormal((i + count - 1) % count);
        const Point next = edgeNormal(i);

        float nx = prev.x + next.x;
        float ny = prev.y + next.y;
        float length = std::sqrt(nx * nx + ny * ny);
        if (length <= 1e-6f) // 180° 折返（正常形状不会出现）：退回后一条边的法线
        {
            nx     = next.x;
            ny     = next.y;
            length = 1.0f;
        }
        nx /= length;
        ny /= length;

        // miter 拉长系数：夹角越尖越长，但夹到 4 倍，避免极端形状戳出尖刺
        const float cosHalf = nx * next.x + ny * next.y;
        const float miter   = 1.0f / std::max(0.25f, cosHalf);

        samples[i].point  = points[i];
        samples[i].normal = Point(nx * std::min(miter, 4.0f), ny * std::min(miter, 4.0f));

        // U：沿中线的累计弧长（最后归一化）
        const Point& after = points[(i + 1) % count];
        accumulated += std::sqrt((after.x - points[i].x) * (after.x - points[i].x)
            + (after.y - points[i].y) * (after.y - points[i].y));
        samples[i].u = accumulated;
    }

    for (Sample& sample : samples)
        sample.u = accumulated > 1e-6f ? sample.u / accumulated : 0.0f;

    return samples;
}

// 一个贴图片段：用**两个采样点各自的法线**算四个角，所以相邻段共用同一条边 —— 不会开缝。
void addTexturedSpan(ImDrawList* drawList, const Sample& from, const Sample& to, float half,
    const TextureRef& lut, float u0, float u1, ImU32 tint)
{
    const Point outerFrom(from.point.x + from.normal.x * half, from.point.y + from.normal.y * half);
    const Point outerTo(to.point.x + to.normal.x * half, to.point.y + to.normal.y * half);
    const Point innerTo(to.point.x - to.normal.x * half, to.point.y - to.normal.y * half);
    const Point innerFrom(from.point.x - from.normal.x * half, from.point.y - from.normal.y * half);

    drawList->AddImageQuad(lut.texture, ImVec2(outerFrom.x, outerFrom.y),
        ImVec2(outerTo.x, outerTo.y), ImVec2(innerTo.x, innerTo.y),
        ImVec2(innerFrom.x, innerFrom.y), ImVec2(u0, 0.0f), ImVec2(u1, 0.0f),
        ImVec2(u1, 1.0f), ImVec2(u0, 1.0f), tint);
}

// 在 from→to 之间按比例 ratio 插一个采样点（跨接缝时切开用）：位置与法线都插值再归一化。
Sample lerpSample(const Sample& from, const Sample& to, float ratio)
{
    Sample out;
    out.point  = Point(from.point.x + (to.point.x - from.point.x) * ratio,
        from.point.y + (to.point.y - from.point.y) * ratio);
    float nx = from.normal.x + (to.normal.x - from.normal.x) * ratio;
    float ny = from.normal.y + (to.normal.y - from.normal.y) * ratio;
    const float length = std::sqrt(nx * nx + ny * ny);
    if (length > 1e-6f)
    {
        nx /= length;
        ny /= length;
    }
    out.normal = Point(nx, ny);
    out.u      = from.u + (to.u - from.u) * ratio;
    return out;
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

    const float half = style.lineWidth * 0.5f;

    // 段长：默认约 6px 一段；显式 segments 时按"总长 / segments"作为上限，
    // 保证每段的 U 跨度不超过 1/segments（tests/focus_ring_test.cpp 的接缝扫描依赖这条）。
    const float totalLength = perimeter(ring);
    const int segmentBudget = segmentCount(ring, style);
    const float step = std::max(0.5f,
        std::min(kTargetSegmentPixels, totalLength / (float)std::max(1, segmentBudget)));

    const std::vector<Sample> samples = buildSamples(ring, step);
    if (samples.size() < 2)
        return;

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

    for (size_t i = 0; i < samples.size(); i++)
    {
        const Sample& from = samples[i];
        const Sample& to   = samples[(i + 1) % samples.size()];

        const LutSpan span = lutSpanAt(from.u, to.u, segmentPhase);
        if (!span.wrapped)
        {
            addTexturedSpan(drawList, from, to, half, style.lut, span.u0, span.u1, tint);
            continue;
        }

        // 跨接缝：按 U 的比例切开，画成两段，保证 UV 单调、不被拉花
        const float length = 1.0f - span.u0 + span.u1;
        const float ratio  = length > 1e-6f ? (1.0f - span.u0) / length : 0.5f;
        const Sample mid   = lerpSample(from, to, ratio);

        addTexturedSpan(drawList, from, mid, half, style.lut, span.u0, 1.0f, tint);

        // u1 正好是 0（最后一段的收尾）时后半段长度为 0，跳过它
        if (span.u1 > 1e-4f)
            addTexturedSpan(drawList, mid, to, half, style.lut, 0.0f, span.u1, tint);
    }
}

} // namespace focus_ring
} // namespace bklv

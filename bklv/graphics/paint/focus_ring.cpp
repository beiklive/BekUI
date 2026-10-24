#include "bklv/graphics/paint/focus_ring.hpp"

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

// 中线每段的目标弧长（px）。4px 而不是 6px：圆是恒定曲率，段一长就看得见多边形。
constexpr float kTargetSegmentPixels = 4.0f;

// 一个 90° 圆角至少切几段 = 15，也就是每段转角 <= 6°。
// 半径很小时按弧长只会切出 3~4 段（圆角像折线）；圆是最吃这条的形状 —— 曲率恒定、段段有转角。
constexpr int kMinCornerSpans = 15;
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
std::vector<Sample> buildSamples(const Outline& ring, float step, bool enforceMinimum)
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

    // 显式 style.segments 时把它当权威（那是高级覆盖）；自动档才套"每角至少 15 段"的地板。
    const int cornerFloor = enforceMinimum ? kMinCornerSpans : 1;
    const int wSpans = spansFor(straightW, 1);
    const int hSpans = spansFor(straightH, 1);
    const int aSpans = r > 0.01f ? spansFor(arcLen, cornerFloor) : 0;

    // 每个特征只放"起点"，末点由下一个特征补上 —— 这样正好是闭环、没有重复点。
    //
    // 法线直接用**解析法线**（直边是轴对齐、圆角是径向），不用"相邻两条边法线的角平分线"：
    // 角平分线在圆角上最多偏半个段角（3°），会让 AA 带的宽度沿着圆抖一个像素 —— 这正是
    // 圆形焦点框边缘那层"毛边"的来源。解析法线没有这个误差，也不需要 miter 拉长。
    std::vector<Point> points;
    std::vector<Point> normals;
    points.reserve((size_t)(2 * wSpans + 2 * hSpans + 4 * aSpans));
    normals.reserve(points.capacity());

    auto addStraight = [&points, &normals](float x0, float y0, float x1, float y1, float nx,
                           float ny, int spans) {
        for (int i = 0; i < spans; i++)
        {
            const float t = spans > 0 ? (float)i / (float)spans : 0.0f;
            points.push_back(Point(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t));
            normals.push_back(Point(nx, ny));
        }
    };
    auto addArc = [&points, &normals](float cx, float cy, float r, float from, float to, int spans) {
        for (int i = 0; i < spans; i++)
        {
            const float t = spans > 0 ? (float)i / (float)spans : 0.0f;
            const float angle = from + (to - from) * t;
            points.push_back(arcPoint(cx, cy, r, angle));
            normals.push_back(Point(std::sin(angle), -std::cos(angle))); // 径向朝外
        }
    };

    // 上边（左 → 右）、右上角、右边（上 → 下）、右下角、下边（右 → 左）、左下角、左边（下 → 上）、左上角
    addStraight(left + r, top, right - r, top, 0.0f, -1.0f, wSpans);
    addArc(right - r, top + r, r, 0.0f, kHalfPi, aSpans);
    addStraight(right, top + r, right, bottom - r, 1.0f, 0.0f, hSpans);
    addArc(right - r, bottom - r, r, kHalfPi, kPi, aSpans);
    addStraight(right - r, bottom, left + r, bottom, 0.0f, 1.0f, wSpans);
    addArc(left + r, bottom - r, r, kPi, kPi + kHalfPi, aSpans);
    addStraight(left, bottom - r, left, top + r, -1.0f, 0.0f, hSpans);
    addArc(left + r, top + r, r, kPi + kHalfPi, 2.0f * kPi, aSpans);

    const size_t count = points.size();
    std::vector<Sample> samples(count);
    float accumulated = 0.0f;
    for (size_t i = 0; i < count; i++)
    {
        samples[i].point  = points[i];
        samples[i].normal = normals[i];

        // U：这一段的**起点**在整圈里的位置（先记后累加 —— 记成"含终点"的话，
        // 最后一个横截面会得到 1.0、wrap 成 0，于是最后一段的 UV 塌成常数、贴图在那儿不流动）
        samples[i].u = accumulated;
        const Point& after = points[(i + 1) % count];
        accumulated += std::sqrt((after.x - points[i].x) * (after.x - points[i].x)
            + (after.y - points[i].y) * (after.y - points[i].y));
    }

    if (accumulated > 1e-6f)
    {
        for (Sample& sample : samples)
            sample.u = sample.u / accumulated;
    }

    return samples;
}

// 一圈的**横截面**：从中线往两侧各取"描边外缘"和"羽边外沿"。
//
//   farA --- nearA ======= nearB --- farB
//   α=0       α=实        中线段      α=实    α=0
//
// 整圈由这些横截面串成**连续条带**（3 条：外羽边 / 实心 / 内羽边），相邻段共用同一个横截面。
// 所以段与段之间没有"封口"——以前每段自己发羽边，两条 alpha 斜坡在接缝处叠加（0.5+0.5=0.75
// 偏亮）或者双双落空（偏暗），一圈多少段就有多少个亮暗点，圆形焦点框上那就是一圈毛刺。
struct Cross
{
    Point farA;
    Point nearA;
    Point nearB;
    Point farB;
    float u = 0.0f; // 贴图 U（相邻段共用同一个值）
};

// 一个绘制单元：横截面 from -> to（UV 由两个横截面自己带，跨接缝时会被切成两段）
struct RibbonSpan
{
    size_t from = 0;
    size_t to   = 0;
};

std::vector<Cross> buildCrossSections(const std::vector<Sample>& samples, float half, float band,
    float phase)
{
    std::vector<Cross> crosses(samples.size());
    for (size_t i = 0; i < samples.size(); i++)
    {
        const Sample& sample = samples[i];

        // normal 已含 miter 拉长（保持描边等宽）；羽边再按单位方向往外铺固定宽度
        const float length = std::sqrt(sample.normal.x * sample.normal.x
            + sample.normal.y * sample.normal.y);
        const float dirX = length > 1e-6f ? sample.normal.x / length : 0.0f;
        const float dirY = length > 1e-6f ? sample.normal.y / length : 1.0f;

        Cross& cross = crosses[i];
        cross.nearA = Point(sample.point.x + sample.normal.x * half,
            sample.point.y + sample.normal.y * half);
        cross.nearB = Point(sample.point.x - sample.normal.x * half,
            sample.point.y - sample.normal.y * half);
        cross.farA  = Point(cross.nearA.x + dirX * band, cross.nearA.y + dirY * band);
        cross.farB  = Point(cross.nearB.x - dirX * band, cross.nearB.y - dirY * band);
        cross.u     = wrap01(sample.u + phase); // 相位直接烘焙进 UV（流光/静止的区别就在这里）
    }

    return crosses;
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

    // 羽边宽度：**每边 1.5 个物理像素**（_FringeScale 恰好是"1 个物理像素"的边长）。
    // 1.0 太薄：像素中心常落在渐变的两端，实测整条圆角上只有 1~2 个像素拿到部分覆盖，
    // 斜边就是台阶；1.5 每边正好落进 2 个像素，圆看起来才顺。再宽（2.0）就发虚了。
    constexpr float kBandPhysicalPixels = 1.5f;
    const float band = kBandPhysicalPixels
        * (drawList->_FringeScale > 0.0f ? drawList->_FringeScale : 1.0f);

    // 段长：默认约 6px 一段；显式 segments 时按"总长 / segments"作为上限，
    // 保证每段的 U 跨度不超过 1/segments（tests/focus_ring_test.cpp 的接缝扫描依赖这条）。
    const float totalLength = perimeter(ring);
    const int segmentBudget = segmentCount(ring, style);
    const float step = std::max(0.5f,
        std::min(kTargetSegmentPixels, totalLength / (float)std::max(1, segmentBudget)));

    const std::vector<Sample> samples = buildSamples(ring, step, style.segments <= 0);
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

    // ---- 条带发射 ----
    //
    // 顶点按"每个横截面 4 个"共享（farA / nearA / nearB / farB），索引按"每段 3 条带 × 6 索引"
    // 铺开：外羽边(farA→nearA)、实心(nearA→nearB)、内羽边(nearB→farB)。
    // 全程手写 PrimWriteVtx/PrimWriteIdx —— 不能用 AddImageQuad：它内部是
    // `PushTexture -> 写 4 个顶点 -> PopTexture`，返回时"当前命令"已经换回原纹理，
    // 之后写的顶点会落到别人的纹理上（现象：几何全对、屏幕全无）。贴图在这里 push 一次。
    std::vector<Cross> crosses = buildCrossSections(samples, half, band, segmentPhase);

    // 构造绘制单元。唯一需要"拆"的地方是 U 接缝：那一段的两侧 UV 必须分别是 1.0 和 u1，
    // 而一个横截面只有一个 U —— 所以复制一份（位置完全相同、只差 UV），拆成两段。
    // 注意这不是"段封口"：两段的几何完全重合、都是不透明的，不存在两条 alpha 斜坡叠加。
    std::vector<RibbonSpan> spans;
    const size_t crossCount = crosses.size(); // ⚠️ 循环里会往 crosses 追加接缝副本，
    spans.reserve(crossCount + 1);            //    所以边界必须先取好（否则 size() 一直长 → 死循环）
    for (size_t i = 0; i < crossCount; i++)
    {
        const size_t next = (i + 1) % crossCount;
        if (crosses[next].u >= crosses[i].u)
        {
            spans.push_back(RibbonSpan{ i, next });
            continue;
        }

        Cross seamEnd = crosses[next];
        seamEnd.u     = 1.0f;
        const size_t seamEndIndex = crosses.size();
        crosses.push_back(seamEnd);

        spans.push_back(RibbonSpan{ i, seamEndIndex });            // ... -> 1.0（接缝前）
        spans.push_back(RibbonSpan{ next, (next + 1) % crossCount }); // u1 -> ...（接缝后）
    }

    // 循环里可能追加了接缝副本，顶点数按**最终**的横截面数算（用循环前的数量会漏写那几个顶点，
    // 而索引已经指向它们 —— 越界索引会让 GPU 读到垃圾顶点）。
    const size_t totalCrosses = crosses.size();
    if (totalCrosses < 2 || spans.empty())
        return;

    const ImU32 tint  = colorWithAlpha(IM_COL32_WHITE, segmentAlpha);
    const ImU32 clear = tint & 0x00FFFFFFu; // 羽边外沿：alpha 0

    drawList->PushTexture(style.lut.texture);

    drawList->PrimReserve((int)spans.size() * 18, (int)totalCrosses * 4);
    const ImDrawIdx base = (ImDrawIdx)drawList->_VtxCurrentIdx;

    for (size_t i = 0; i < totalCrosses; i++)
    {
        const Cross& cross = crosses[i];
        const ImVec2 uv(cross.u, 0.0f);
        const ImVec2 uvB(cross.u, 1.0f);
        drawList->PrimWriteVtx(ImVec2(cross.farA.x, cross.farA.y), uv, clear);
        drawList->PrimWriteVtx(ImVec2(cross.nearA.x, cross.nearA.y), uv, tint);
        drawList->PrimWriteVtx(ImVec2(cross.nearB.x, cross.nearB.y), uvB, tint);
        drawList->PrimWriteVtx(ImVec2(cross.farB.x, cross.farB.y), uvB, clear);
    }

    for (const RibbonSpan& span : spans)
    {
        const ImDrawIdx a = (ImDrawIdx)(base + span.from * 4);
        const ImDrawIdx b = (ImDrawIdx)(base + span.to * 4);
        // 三条带各自的顶点偏移：0=farA 1=nearA 2=nearB 3=farB
        for (ImDrawIdx offset = 0; offset < 3; offset++)
        {
            drawList->PrimWriteIdx(a + offset);
            drawList->PrimWriteIdx(b + offset);
            drawList->PrimWriteIdx(b + offset + 1);
            drawList->PrimWriteIdx(a + offset);
            drawList->PrimWriteIdx(b + offset + 1);
            drawList->PrimWriteIdx(a + offset + 1);
        }
    }

    drawList->PopTexture();
}

} // namespace focus_ring
} // namespace bklv

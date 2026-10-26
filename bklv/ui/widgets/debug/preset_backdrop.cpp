#include "bklv/ui/widgets/debug/preset_backdrop.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include "bklv/ui/theme/theme_data.hpp"
#include "bklv/foundation/anim/time.hpp"
#include "bklv/foundation/core/ui_scale.hpp"

using namespace bklv;

namespace
{

inline ImU32 withAlpha(ImU32 color, int alpha)
{
    return (color & 0x00FFFFFFu) | ((ImU32)(alpha & 0xFF) << 24);
}

inline ImU32 mix(ImU32 a, ImU32 b, float t)
{
    const int ar = (int)(a & 0xFF), ag = (int)((a >> 8) & 0xFF), ab = (int)((a >> 16) & 0xFF);
    const int br = (int)(b & 0xFF), bg = (int)((b >> 8) & 0xFF), bb = (int)((b >> 16) & 0xFF);
    const int r  = (int)(ar + (br - ar) * t);
    const int g  = (int)(ag + (bg - ag) * t);
    const int bl = (int)(ab + (bb - ab) * t);
    return IM_COL32(r, g, bl, 255);
}

} // namespace

PresetBackdrop::PresetBackdrop(BackdropPreset preset)
    : preset(preset)
{
    this->setWidthPercentage(100.0f);
    this->setHeightPercentage(100.0f);
}

void PresetBackdrop::setPreset(BackdropPreset value)
{
    this->preset = value;
    this->items.clear();
    this->itemWidth  = 0.0f;
    this->itemHeight = 0.0f;
    this->invalidate();
}

void PresetBackdrop::ensureItems(float width, float height)
{
    if (!this->items.empty() && this->itemWidth == width && this->itemHeight == height)
        return;

    // 固定种子：同一次运行里布局稳定，截图可复现
    std::mt19937 rng(20261020u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    this->items.clear();
    this->items.resize(24);
    for (FloatingItem& item : this->items)
    {
        item.x           = unit(rng) * width;
        item.y           = unit(rng) * height;
        item.speedX      = (unit(rng) * 2.0f - 1.0f) * 8.0f * getUiScale();
        item.speedY      = -(8.0f + unit(rng) * 14.0f) * getUiScale();
        item.size        = (24.0f + unit(rng) * 36.0f) * getUiScale();
        item.rotation    = unit(rng) * 6.2831853f;
        item.rotateSpeed = (unit(rng) * 2.0f - 1.0f) * 0.6f;
        item.alpha       = 0.05f + unit(rng) * 0.11f;
        item.shape       = (int)(unit(rng) * 4.0f);
    }
    this->itemWidth  = width;
    this->itemHeight = height;
}

void PresetBackdrop::ensurePolygons(float width, float height)
{
    if (!this->polygons.empty() && this->polygonWidth == width
        && this->polygonHeight == height)
        return;

    // 固定种子：同一次运行里布局稳定，截图可复现。颜色取主题色板（换肤会跟着变）。
    static const char* const kColorKeys[] = {
        "color.common.red", "color.common.orange", "color.common.yellow",
        "color.common.green", "color.common.cyan", "color.common.blue",
        "color.common.purple", "color.common.pink",
    };
    static const ImU32 kFallback[] = {
        IM_COL32(239, 83, 80, 255), IM_COL32(255, 152, 0, 255),
        IM_COL32(255, 235, 59, 255), IM_COL32(102, 187, 106, 255),
        IM_COL32(38, 198, 218, 255), IM_COL32(66, 165, 245, 255),
        IM_COL32(171, 71, 188, 255), IM_COL32(236, 64, 122, 255),
    };

    std::mt19937 rng(20261107u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float scale = getUiScale();
    const float maxSize = std::min(width, height) * 0.18f;

    this->polygons.clear();
    this->polygons.resize(24);
    for (PolygonItem& item : this->polygons)
    {
        item.size        = std::min((26.0f + unit(rng) * 58.0f) * scale, maxSize);
        item.x           = unit(rng) * width;
        item.y           = unit(rng) * height;
        item.speedX      = (unit(rng) * 2.0f - 1.0f) * 42.0f * scale;
        item.speedY      = (unit(rng) * 2.0f - 1.0f) * 42.0f * scale;
        item.rotation    = unit(rng) * 6.2831853f;
        item.rotateSpeed = (unit(rng) * 2.0f - 1.0f) * 1.4f;
        item.sides       = 3 + (int)(unit(rng) * 4.0f); // 3..6
        const int colorIndex = (int)(unit(rng) * 8.0f) % 8;
        item.color = withAlpha(getTheme().color(kColorKeys[colorIndex], kFallback[colorIndex]),
            190 + (int)(unit(rng) * 50.0f));
    }
    this->polygonWidth  = width;
    this->polygonHeight = height;
}

void PresetBackdrop::update()
{
    View::update();

    const bool floating = this->preset == BackdropPreset::FLOATING;
    const bool polygons = this->preset == BackdropPreset::POLYGONS;
    if (!floating && !polygons)
        return;

    const float dt = getFrameDeltaSeconds();
    if (dt <= 0.0f)
        return;

    const float width  = this->getWidth();
    const float height = this->getHeight();
    if (width <= 0.0f || height <= 0.0f)
        return;

    if (floating)
    {
        this->ensureItems(width, height);
        this->time += dt;

        for (FloatingItem& item : this->items)
        {
            item.x += item.speedX * dt;
            item.y += item.speedY * dt;
            item.rotation += item.rotateSpeed * dt;

            if (item.y < -item.size)
            {
                item.y = height + item.size;
                item.x = (float)(std::rand() % 1000) / 1000.0f * width;
            }
            if (item.x < -item.size)
                item.x = width + item.size;
            if (item.x > width + item.size)
                item.x = -item.size;
        }
        return;
    }

    // POLYGONS：随机方向匀速飘、自转，撞到边就反弹（不飞出画面）
    this->ensurePolygons(width, height);
    for (PolygonItem& item : this->polygons)
    {
        item.x += item.speedX * dt;
        item.y += item.speedY * dt;
        item.rotation += item.rotateSpeed * dt;

        const float radius = item.size;
        if (item.x < radius)
        {
            item.x      = radius;
            item.speedX = std::abs(item.speedX);
        }
        else if (item.x > width - radius)
        {
            item.x      = width - radius;
            item.speedX = -std::abs(item.speedX);
        }
        if (item.y < radius)
        {
            item.y      = radius;
            item.speedY = std::abs(item.speedY);
        }
        else if (item.y > height - radius)
        {
            item.y      = height - radius;
            item.speedY = -std::abs(item.speedY);
        }
    }
}

void PresetBackdrop::drawGrid(ImDrawList* drawList, float x, float y, float width, float height)
{
    const ImU32 base   = getTheme().color("color.surface.card", IM_COL32(26, 28, 33, 255));
    const ImU32 accent = getTheme().color("color.accent.base", IM_COL32(0, 230, 180, 255));

    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), base);

    const float minor = 32.0f * getUiScale();
    const float major = minor * 4.0f;
    const ImU32 minorColor = withAlpha(accent, 16);
    const ImU32 majorColor = withAlpha(accent, 34);

    for (float gx = 0.0f; gx <= width; gx += minor)
    {
        const bool isMajor = std::fmod(gx, major) < 0.5f;
        drawList->AddLine(ImVec2(x + gx, y), ImVec2(x + gx, y + height),
            isMajor ? majorColor : minorColor, 1.0f);
    }
    for (float gy = 0.0f; gy <= height; gy += minor)
    {
        const bool isMajor = std::fmod(gy, major) < 0.5f;
        drawList->AddLine(ImVec2(x, y + gy), ImVec2(x + width, y + gy),
            isMajor ? majorColor : minorColor, 1.0f);
    }
}

void PresetBackdrop::drawFloating(ImDrawList* drawList, float x, float y, float width, float height)
{
    const ImU32 base    = getTheme().color("color.surface.card", IM_COL32(26, 28, 33, 255));
    const ImU32 accent  = getTheme().color("color.accent.base", IM_COL32(0, 230, 180, 255));
    const ImU32 top     = mix(base, accent, 0.05f); // 顶部带一点强调色，别死黑
    const ImU32 bottom  = mix(base, IM_COL32(0, 0, 0, 255), 0.45f);
    const ImU32 grey    = getTheme().color("color.common.grey", IM_COL32(158, 158, 158, 255));

    drawList->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + width, y + height),
        top, top, bottom, bottom);

    this->ensureItems(width, height);

    for (const FloatingItem& item : this->items)
    {
        const ImU32 color = withAlpha(grey, (int)(item.alpha * 255.0f));
        const float cx    = x + item.x;
        const float cy    = y + item.y;
        const float half  = item.size * 0.5f;
        const float c     = std::cos(item.rotation);
        const float s     = std::sin(item.rotation);

        switch (item.shape)
        {
            case 0: // 圆
                drawList->AddCircleFilled(ImVec2(cx, cy), half, color, 24);
                break;
            case 1: // 方（旋转）
            {
                const ImVec2 corners[4] = {
                    ImVec2(cx + (-half) * c - (-half) * s, cy + (-half) * s + (-half) * c),
                    ImVec2(cx + (half) * c - (-half) * s, cy + (half) * s + (-half) * c),
                    ImVec2(cx + (half) * c - (half) * s, cy + (half) * s + (half) * c),
                    ImVec2(cx + (-half) * c - (half) * s, cy + (-half) * s + (half) * c),
                };
                drawList->AddQuadFilled(corners[0], corners[1], corners[2], corners[3], color);
                break;
            }
            case 2: // 三角
            {
                const ImVec2 p1(cx + (-half) * c - (-half * 0.6f) * s, cy + (-half) * s + (-half * 0.6f) * c);
                const ImVec2 p2(cx + half * c - (-half * 0.6f) * s, cy + half * s + (-half * 0.6f) * c);
                const ImVec2 p3(cx - (half) * s, cy + (half) * c);
                drawList->AddTriangleFilled(p1, p2, p3, color);
                break;
            }
            default: // 十字
                drawList->AddLine(ImVec2(cx - half * c, cy - half * s),
                    ImVec2(cx + half * c, cy + half * s), color, 2.0f * getUiScale());
                drawList->AddLine(ImVec2(cx - half * s, cy + half * c),
                    ImVec2(cx + half * s, cy - half * c), color, 2.0f * getUiScale());
                break;
        }
    }
}

void PresetBackdrop::drawGradient(ImDrawList* drawList, float x, float y, float width, float height)
{
    const ImU32 base   = getTheme().color("color.surface.card", IM_COL32(26, 28, 33, 255));
    const ImU32 accent = getTheme().color("color.accent.base", IM_COL32(0, 230, 180, 255));

    // 单一两段渐变：分两段画会在接缝处出硬边；同心圆加光晕会看到一圈圈弧线，所以只留渐变
    const ImU32 dark   = mix(base, IM_COL32(0, 0, 0, 255), 0.60f);
    const ImU32 top    = mix(dark, accent, 0.30f);
    const ImU32 bottom = mix(base, IM_COL32(0, 0, 0, 255), 0.35f);

    drawList->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + width, y + height),
        top, top, bottom, bottom);
}

void PresetBackdrop::drawChecker(ImDrawList* drawList, float x, float y, float width,
    float height)
{
    // 高对比棋盘：玻璃的折射本质是"位移采样"，需要这种高频细节才看得出来。
    const ImU32 light = getTheme().color("color.surface.card", IM_COL32(242, 243, 245, 255));
    const ImU32 dark  = getTheme().color("color.common.black", IM_COL32(18, 20, 24, 255));

    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), light);

    const float cell = 32.0f * getUiScale();
    for (float cy = 0.0f; cy < height; cy += cell)
        for (float cx = 0.0f; cx < width; cx += cell)
            if (((int)(cx / cell) + (int)(cy / cell)) % 2 == 0)
                drawList->AddRectFilled(ImVec2(x + cx, y + cy),
                    ImVec2(x + cx + cell, y + cy + cell), dark);
}

void PresetBackdrop::drawPolygons(ImDrawList* drawList, float x, float y, float width,
    float height)
{
    const ImU32 base = getTheme().color("color.surface.card", IM_COL32(242, 243, 245, 255));
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), base);

    this->ensurePolygons(width, height);

    ImVec2 points[6];
    for (const PolygonItem& item : this->polygons)
    {
        for (int i = 0; i < item.sides; i++)
        {
            const float angle = item.rotation
                + 6.2831853f * (float)i / (float)item.sides;
            points[i] = ImVec2(x + item.x + std::cos(angle) * item.size,
                y + item.y + std::sin(angle) * item.size);
        }
        drawList->AddConvexPolyFilled(points, item.sides, item.color);
    }
}

void PresetBackdrop::draw(ImDrawList* drawList, float x, float y, float width, float height,
    FrameContext*)
{
    if (drawList == nullptr || width <= 0.0f || height <= 0.0f)
        return;

    switch (this->preset)
    {
        case BackdropPreset::GRID:     this->drawGrid(drawList, x, y, width, height); break;
        case BackdropPreset::FLOATING: this->drawFloating(drawList, x, y, width, height); break;
        case BackdropPreset::GRADIENT: this->drawGradient(drawList, x, y, width, height); break;
        case BackdropPreset::CHECKER:  this->drawChecker(drawList, x, y, width, height); break;
        case BackdropPreset::POLYGONS: this->drawPolygons(drawList, x, y, width, height); break;
        case BackdropPreset::NONE:
        default: break;
    }
}

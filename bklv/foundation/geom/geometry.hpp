#pragma once

// 几何与颜色基础类型，对应 borealis 的 core/geometry.hpp。
// 这一层不依赖 Yoga，也不依赖 ImGui，保持纯数据。

#include <imgui.h>

namespace bklv
{

// 二维点，对应 borealis 的 Point。
struct Point
{
    float x = 0.0f;
    float y = 0.0f;

    Point() = default;
    Point(float x, float y)
        : x(x)
        , y(y)
    {
    }

    Point operator+(const Point& other) const { return Point(x + other.x, y + other.y); }
    Point operator-(const Point& other) const { return Point(x - other.x, y - other.y); }
};

// 尺寸，对应 borealis 的 Size。
struct Size
{
    float width  = 0.0f;
    float height = 0.0f;

    Size() = default;
    Size(float width, float height)
        : width(width)
        , height(height)
    {
    }
};

// 四边内缩，对应 borealis 的 EdgeInsets。安全区、页头/底栏避让都用它。
// 字段顺序固定 top / right / bottom / left，与构造函数一致。
struct EdgeInsets
{
    float top    = 0.0f;
    float right  = 0.0f;
    float bottom = 0.0f;
    float left   = 0.0f;

    EdgeInsets() = default;
    EdgeInsets(float top, float right, float bottom, float left)
        : top(top)
        , right(right)
        , bottom(bottom)
        , left(left)
    {
    }

    // 四边同值，用于"整体内缩一圈"。
    static EdgeInsets all(float value) { return EdgeInsets(value, value, value, value); }
};

// 矩形，对应 borealis 的 Rect。坐标原点是左上角，y 轴向下。
struct Rect
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    Rect() = default;
    Rect(float x, float y, float width, float height)
        : x(x)
        , y(y)
        , width(width)
        , height(height)
    {
    }

    float getMinX() const { return x; }
    float getMinY() const { return y; }
    float getMaxX() const { return x + width; }
    float getMaxY() const { return y + height; }
    float getCenterX() const { return x + width * 0.5f; }
    float getCenterY() const { return y + height * 0.5f; }

    // 判断一个点是否落在矩形内，用于命中测试。
    bool contains(const Point& point) const
    {
        return point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height;
    }

    // 判断另一个矩形是否与本矩形相交，用于裁剪剔除。
    bool intersects(const Rect& other) const
    {
        return !(other.getMaxX() < x || other.x > getMaxX() || other.getMaxY() < y || other.y > getMaxY());
    }

    // 与另一个矩形求交，用于把裁剪区域限制在父节点范围内。
    Rect intersect(const Rect& other) const
    {
        const float min_x = x > other.x ? x : other.x;
        const float min_y = y > other.y ? y : other.y;
        const float max_x = getMaxX() < other.getMaxX() ? getMaxX() : other.getMaxX();
        const float max_y = getMaxY() < other.getMaxY() ? getMaxY() : other.getMaxY();
        if (max_x <= min_x || max_y <= min_y)
            return Rect(min_x, min_y, 0.0f, 0.0f);
        return Rect(min_x, min_y, max_x - min_x, max_y - min_y);
    }
};

// 颜色统一用 ImU32 表达，ImDrawList 的绘制函数直接接受它。
// 对应 borealis 里的 nvgRGB / nvgRGBA 系列。
inline ImU32 rgb(unsigned r, unsigned g, unsigned b)
{
    return IM_COL32(r, g, b, 255);
}

inline ImU32 rgba(unsigned r, unsigned g, unsigned b, unsigned a)
{
    return IM_COL32(r, g, b, a);
}

inline ImU32 rgbf(float r, float g, float b)
{
    return IM_COL32(static_cast<int>(r * 255.0f), static_cast<int>(g * 255.0f), static_cast<int>(b * 255.0f), 255);
}

inline ImU32 rgbaf(float r, float g, float b, float a)
{
    return IM_COL32(static_cast<int>(r * 255.0f), static_cast<int>(g * 255.0f), static_cast<int>(b * 255.0f),
        static_cast<int>(a * 255.0f));
}

// 把颜色整体乘以一个透明度系数，用于 View 的 alpha 继承。
//
// **唯一实现**：以前各图层绘制文件里还各写了一份 `scaleAlpha` lambda（6 处逐字重复），
// 而且那份是四舍五入、这份是截断 —— 同一个透明度能差 1/255。现在统一走这里，取更准的舍入。
inline ImU32 colorWithAlpha(ImU32 color, float alpha)
{
    if (alpha >= 1.0f)
        return color;
    if (alpha <= 0.0f)
        return color & ~IM_COL32_A_MASK;

    unsigned a = (color >> IM_COL32_A_SHIFT) & 0xFF;
    a          = static_cast<unsigned>((float)a * alpha + 0.5f);
    return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

} // namespace bklv

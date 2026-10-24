#pragma once

// 绘制期变换（缩放 / 旋转 / 斜切 / 位移）。
//
// 和 borealis 的差别：borealis 的 `View` 本身没有变换 API，它只在自己的 demo 里
// 用自定义视图手写（`TransformBox` 靠 NanoVG 的变换栈 nvgSave/nvgRotate/nvgSkew/nvgScale）。
// ImDrawList 没有变换栈，所以我们在 `View::frame` 一层统一实现：把本视图画出来的那段
// 顶点按矩阵变换一次。这样**任何控件**都能用，不用各自写自定义绘制。
//
// 语义：
// - **只管画，不改布局**：布局（Yoga）与命中测试仍按未变换的矩形，和 borealis 一致。
// - 变换中心默认是控件中心（`setOrigin` 可改）。
// - 顺序：先缩放，再斜切，最后旋转 + 位移；文字顶点也一起变换。
// - 文字的"只缩放字号"用 `Label::setFontScaleX/Y`（那是 Label 自己的事，见 views/label.hpp）。

#include <cmath>

#include "bklv/foundation/geom/geometry.hpp"

namespace bklv
{

struct Transform2D
{
    float translateX = 0.0f;
    float translateY = 0.0f;
    float scaleX     = 1.0f;
    float scaleY     = 1.0f;
    float skewX      = 0.0f; // 弧度
    float skewY      = 0.0f; // 弧度
    float rotation   = 0.0f; // 弧度
    Point origin{ 0.0f, 0.0f };
    bool hasOrigin = false; // 没设过 origin 时用控件中心

    bool isIdentity() const
    {
        return this->translateX == 0.0f && this->translateY == 0.0f && this->scaleX == 1.0f
            && this->scaleY == 1.0f && this->skewX == 0.0f && this->skewY == 0.0f
            && this->rotation == 0.0f;
    }

    void setTranslationX(float value) { this->translateX = value; }
    void setTranslationY(float value) { this->translateY = value; }
    void setScaleX(float value) { this->scaleX = value; }
    void setScaleY(float value) { this->scaleY = value; }
    void setSkewX(float radians) { this->skewX = radians; }
    void setSkewY(float radians) { this->skewY = radians; }
    void setRotate(float radians) { this->rotation = radians; }

    // 所有分量都是有限数？（NaN/Inf 会让控件在绘制时整块消失）
    bool isFinite() const
    {
        return std::isfinite(this->translateX) && std::isfinite(this->translateY)
            && std::isfinite(this->scaleX) && std::isfinite(this->scaleY)
            && std::isfinite(this->skewX) && std::isfinite(this->skewY)
            && std::isfinite(this->rotation);
    }

    void reset()
    {
        this->translateX = 0.0f;
        this->translateY = 0.0f;
        this->scaleX     = 1.0f;
        this->scaleY     = 1.0f;
        this->skewX      = 0.0f;
        this->skewY      = 0.0f;
        this->rotation   = 0.0f;
        this->hasOrigin  = false;
    }

    // 以 center 为缺省中心做变换；返回变换后的点
    Point apply(float x, float y, const Point& center) const
    {
        const Point base = this->hasOrigin ? this->origin : center;

        float px = x - base.x;
        float py = y - base.y;

        px *= this->scaleX;
        py *= this->scaleY;

        if (this->skewX != 0.0f || this->skewY != 0.0f)
        {
            const float sx = px + std::tan(this->skewX) * py;
            const float sy = py + std::tan(this->skewY) * px;
            px             = sx;
            py             = sy;
        }

        if (this->rotation != 0.0f)
        {
            const float c = std::cos(this->rotation);
            const float s = std::sin(this->rotation);
            const float rx = c * px - s * py;
            const float ry = s * px + c * py;
            px             = rx;
            py             = ry;
        }

        return Point(px + base.x + this->translateX, py + base.y + this->translateY);
    }
};

} // namespace bklv

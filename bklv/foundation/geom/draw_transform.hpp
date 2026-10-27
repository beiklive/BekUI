#pragma once

// 绘制期变换的"当前作用域"：谁在画、它的变换是什么。
//
// 为什么需要它：`View::frame` 的画法是「**先把整棵子树画出来，再把这一段顶点按矩阵整体变形**」。
// 这只覆盖落到 ImDrawList 顶点里的东西。凡是把几何提交到**别处**的绘制 —— 玻璃 / 纯模糊把
// 矩形交给 GlassRequestQueue，将来别的层也可能这么做 —— 顶点变形管不到它，于是控件动了、
// 它留在原地（toast 从右边滑进来，目标位置却先出现一块玻璃，就是这么来的）。
//
// 用法：
//   · 画节点的一方（`View::frame`）在进入节点时构造 `TransformScope`，出节点时自动弹栈；
//   · 提交外部几何的一方调 `mapRect()` / `mapPoint()` 拿"和顶点完全一致"的位置。
//
// 嵌套时逐级 apply（先自己、再祖先），顺序与 `View::frame` 对顶点的处理一致，
// 所以不用自己拼矩阵；某层没有变换就根本不会入栈，也不会有精度损失。

#include <algorithm>
#include <vector>

#include "bklv/foundation/geom/transform.hpp"

namespace bklv::draw
{

namespace detail
{

struct TransformEntry
{
    Transform2D transform;
    Point center;
};

// 单线程（库是"一个进程一套界面"），所以就是一个普通静态表；深度通常 < 20。
inline std::vector<TransformEntry>& transformStack()
{
    static std::vector<TransformEntry> entries;
    return entries;
}

} // namespace detail

// 一个节点的变换作用域。构造即入栈、析构弹栈。
// transform 为空 / 单位矩阵 / 含 NaN 时**不入栈**（和 View::frame 跳过变形判断一致）。
class TransformScope
{
  public:
    TransformScope(const Transform2D* transform, const Point& center)
    {
        if (transform == nullptr || transform->isIdentity() || !transform->isFinite())
            return;

        detail::transformStack().push_back(detail::TransformEntry{ *transform, center });
        this->pushed = true;
    }

    ~TransformScope()
    {
        if (!this->pushed)
            return;
        std::vector<detail::TransformEntry>& stack = detail::transformStack();
        if (!stack.empty())
            stack.pop_back();
    }

    TransformScope(const TransformScope&)            = delete;
    TransformScope& operator=(const TransformScope&) = delete;

  private:
    bool pushed = false;
};

// 当前有没有待应用的变换（没有就可以整段跳过，省掉几次映射）
inline bool hasPendingTransforms()
{
    return !detail::transformStack().empty();
}

// 最内层（正在绘制的节点）的变换；没有就返回 nullptr。调试与"只关心平移"的调用方用。
inline const Transform2D* currentTransform()
{
    const auto& stack = detail::transformStack();
    return stack.empty() ? nullptr : &stack.back().transform;
}

// 把一个**屏幕坐标**点按当前作用域逐级映射（自己 → 祖先）
inline Point mapPoint(const Point& point)
{
    Point result      = point;
    const auto& stack = detail::transformStack();
    for (auto it = stack.rbegin(); it != stack.rend(); ++it)
        result = it->transform.apply(result.x, result.y, it->center);
    return result;
}

// 矩形 [minX, minY, maxX, maxY] 映射后的轴对齐包围盒（旋转/斜切只能取包围盒）
inline void mapRect(float bounds[4])
{
    if (!hasPendingTransforms())
        return;

    const Point corners[4] = {
        mapPoint(Point(bounds[0], bounds[1])),
        mapPoint(Point(bounds[2], bounds[1])),
        mapPoint(Point(bounds[2], bounds[3])),
        mapPoint(Point(bounds[0], bounds[3])),
    };

    bounds[0] = std::min(std::min(corners[0].x, corners[1].x), std::min(corners[2].x, corners[3].x));
    bounds[1] = std::min(std::min(corners[0].y, corners[1].y), std::min(corners[2].y, corners[3].y));
    bounds[2] = std::max(std::max(corners[0].x, corners[1].x), std::max(corners[2].x, corners[3].x));
    bounds[3] = std::max(std::max(corners[0].y, corners[1].y), std::max(corners[2].y, corners[3].y));
}

} // namespace bklv::draw

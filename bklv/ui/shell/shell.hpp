#pragma once

// 应用外壳：给宿主一套**固定顺序的层容器**，让"背景 / 内容 / 覆盖 / 浮层 / 调试"
// 不用宿主自己摆顺序、也不会互相盖错。
//
// 绘制顺序 = 子节点顺序（`Box::drawChildren` 按 addView 顺序画）：
//
//   BackdropLayer  背景层   绝对定位；图片背景、视频、按页底图
//   ContentLayer   内容层   正式内容摆这层（页面、页面栈）
//   OverlayLayer   覆盖层   页头 / 底栏 / 弹窗
//   HudLayer       浮层     不吃输入；toast、常驻指示、焦点框
//   DebugLayer     调试层   **在浮层之上**，永远最后绘制；日志窗口等调试 UI
//
// 契约（改这里之前先读一遍）：
//  - 五个层都是绝对定位 + 四边 0，铺满外壳，**不参与彼此的 flex 排布**；
//  - 只有内容层与覆盖层里的东西按正常布局排，其余层靠绝对定位摆；
//  - 焦点框归外壳所有（挂在浮层上，永远在内容/覆盖层之上），控件侧不用管；
//  - 过渡层（`getTransitionHost()`）**只在动画期间存在**，空了下一帧自动摘掉释放，
//    别把常驻东西挂进去；
//  - 安全区五层统一内缩（Switch 的圆角/刘海），用 `setSafeAreaInsets()` 设一次即可。
//
// 用法：
//   auto* shell = new bklv::Shell();
//   shell->getContentLayer()->addView(page);
//   shell->getOverlayLayer()->addView(header);
//   app.setRootView(shell);

#include "bklv/foundation/geom/geometry.hpp"
#include "bklv/ui/shell/focus_ring_view.hpp"
#include "bklv/ui/view/box.hpp"

namespace bklv
{

class Shell : public Box
{
  public:
    Shell();
    ~Shell() override;

    // 调试层按这个类型名显示（不重写就只剩基类的 "bklv::View"）。
    std::string getClassString() const override { return "bklv::Shell"; }

    // 五个层（都由外壳持有，宿主只往里 addView，不要自己 delete）
    Box* getBackdropLayer() const { return this->backdropLayer; }
    Box* getContentLayer() const { return this->contentLayer; }
    Box* getOverlayLayer() const { return this->overlayLayer; }

    // 弹层挂在覆盖层，浮层（焦点框）与调试层仍然在它之上。
    Box* getOverlayHost() override { return this->overlayLayer; }    Box* getHudLayer() const { return this->hudLayer; }
    Box* getDebugLayer() const { return this->debugLayer; }

    // ---- 焦点框（流光框）----
    // 外壳是焦点框的所有者：它挂在浮层上，永远画在最上层。
    // 开关 / 换类型 / 调参数都从这里走，控件侧完全不用管。
    void setFocusRingEnabled(bool enabled);
    bool isFocusRingEnabled() const;

    void setFocusRingType(focus_ring::Type type);
    focus_ring::Type getFocusRingType() const;

    // 细调（线宽/缝/流动速度/透明度/分段）；颜色永远来自 LUT 贴图，这里改不了。
    focus_ring::Style& getFocusRingStyle();
    const focus_ring::Style& getFocusRingStyle() const;

    // 需要自己画法时（`focus_ring::Type::CUSTOM`）直接拿这个视图挂渲染器
    FocusRingView* getFocusRingView() const { return this->focusRingView; }

    // ---- 安全区：五层统一内缩（Switch 的圆角 / 刘海）----
    void setSafeAreaInsets(float top, float right, float bottom, float left);
    void setSafeAreaInsets(const EdgeInsets& insets)
    {
        this->setSafeAreaInsets(insets.top, insets.right, insets.bottom, insets.left);
    }
    float getSafeAreaTop() const { return this->safeTop; }
    float getSafeAreaRight() const { return this->safeRight; }
    float getSafeAreaBottom() const { return this->safeBottom; }
    float getSafeAreaLeft() const { return this->safeLeft; }

    // ---- 过渡层（**仅动画期间存在**）----
    // 跨层飞行元素挂这里，别挂浮层：浮层是常驻的，飞行元素混进去会污染它，
    // 而且动画结束没人负责清理。第一次调用时创建（挂在最上层 = 最后绘制），
    // 层里没有子节点后，下一次 update() 把它从树上摘掉并释放。
    Box* getTransitionHost();
    bool hasTransitionHost() const { return this->transitionHost != nullptr; }
    void clearTransitionHost();

    // 把某个视图铺满它所在的那一层（四边 0 的绝对定位）。
    void fillLayer(View* view);

    void update() override;

  protected:
    void applySafeAreaToLayers();

  private:
    // 过渡层空了就摘掉释放（仅动画期存在）
    void releaseTransitionHostIfEmpty();

    Box* backdropLayer = nullptr;
    Box* contentLayer  = nullptr;
    Box* overlayLayer  = nullptr;
    Box* hudLayer      = nullptr;
    Box* debugLayer    = nullptr;

    FocusRingView* focusRingView = nullptr;
    Box* transitionHost          = nullptr;

    float safeTop    = 0.0f;
    float safeRight  = 0.0f;
    float safeBottom = 0.0f;
    float safeLeft   = 0.0f;
};

} // namespace bklv

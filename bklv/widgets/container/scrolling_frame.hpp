#pragma once

// 纵向滚动容器，对应 borealis 的 views/scrolling_frame.hpp。
//
// 结构：
//   ScrollingFrame（自身 clipsToBounds，滚动时不参与 Yoga 重排）
//     └─ contentView（一个显式设定高度的 Box，靠 translation 上下移动）
//
// 关键点：
//   - 滚动只改 translation，不触发 Yoga 重算，所以长列表滚动不会每帧重排
//   - contentView 必须显式设置高度（这是可滚动范围），否则内容不知道自己有多高
//   - 焦点变化时自动把目标滚到可见区

#include <functional>

#include "bklv/view/box.hpp"

namespace bklv
{

// 滚动到焦点时的行为
enum class ScrollingBehavior
{
    // 焦点必须在可视区内，必要时才滚动
    FOCUS,
    // 焦点永远保持在中心，输入即滚动
    CENTER,
};

class ScrollingFrame : public Box
{
  public:
    ScrollingFrame();
    ~ScrollingFrame() override;

    // 设置内容视图，ScrollingFrame 接管其所有权（替换时旧内容会被销毁）。
    void setContentView(View* view);

    // 取走当前内容视图：从树上摘下但不销毁，所有权交回调用方。
    // 需要在不同容器之间搬同一个视图时必须用它，不能直接 setContentView 覆盖
    // ——那样旧内容会被销毁，而别处可能还持有它。
    View* evictContentView();
    View* getContentView() const { return this->contentView; }

    void setScrollingBehavior(ScrollingBehavior behavior) { this->behavior = behavior; }
    ScrollingBehavior getScrollingBehavior() const { return this->behavior; }

    // 当前滚动偏移（正值表示内容往上移）。
    float getContentOffsetY() const { return this->contentOffsetY; }
    void setContentOffsetY(float offset, bool animated = false);

    // 可滚动范围 = 内容高度 - 可视高度，最小为 0。
    float getScrollRange() const;

    // 是否显示右侧滚动条。
    void setShowScrollingIndicator(bool show) { this->showIndicator = show; }
    bool getShowScrollingIndicator() const { return this->showIndicator; }

    // 是否允许鼠标滚轮 / 拖拽。
    void setScrollingEnabled(bool enabled) { this->scrollingEnabled = enabled; }

    // 关闭惯性，方便调试与测试。
    void setInertiaEnabled(bool enabled) { this->inertiaEnabled = enabled; }

    // 每帧推进：惯性、拖拽、焦点跟随。
    void update() override;

    void onLayout() override;

    // 滚到指定视图可见；animated 为 true 时用缓动。
    void scrollToView(View* view, bool animated = true) override;

    // ensureFocusVisible() 靠它认出"最近的滚动容器"。
    bool isScrollable() const override { return true; }

    // 上下都走不动、但焦点还没完全露出来时接管：返回 this 表示焦点不动、
    // 由 ensureFocusVisible() 把它滚进视野（borealis 的自然滚动）。
    View* getParentNavigationDecision(View* from, View* newFocus, FocusDirection direction) override;

    std::string getClassString() const override { return "bklv::ScrollingFrame"; }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, FrameContext* ctx) override;

    // 每次布局后由 View::notifyLayout 触发。
    // 子类如果自己管理内容尺寸（比如 RecyclerFrame 按数据条数算高度），
    // 可以重写成空实现，避免这里再去改高度触发一次多余的重排。
    virtual void onContentSizeChanged();

    View* contentView = nullptr;

    float contentOffsetY = 0.0f;

    // 拖动状态
    bool dragging      = false;
    float dragStartY   = 0.0f;
    float dragStartOffset = 0.0f;

    // 惯性状态
    float velocity     = 0.0f;
    float lastDragTime = 0.0f;

    // 有目标偏移时朝目标缓动（滚到焦点用）
    bool animating     = false;
    float animTarget   = 0.0f;

    // 上一次自动滚动的目标，避免每帧把用户手动滚走的位置又拽回来
    View* lastAutoScrolledView = nullptr;

    bool scrollingEnabled = true;
    bool inertiaEnabled   = true;
    bool showIndicator    = true;

    ScrollingBehavior behavior = ScrollingBehavior::FOCUS;
};

} // namespace bklv

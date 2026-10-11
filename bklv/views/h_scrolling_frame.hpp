#pragma once

// 横向滚动容器，对应 borealis 的 views/h_scrolling_frame.hpp。
// 与 ScrollingFrame 同构，只是主轴换成水平方向。

#include "bklv/core/box.hpp"

namespace bklv
{

class HScrollingFrame : public Box
{
  public:
    HScrollingFrame();
    ~HScrollingFrame() override;

    void setContentView(View* view);
    View* getContentView() const { return this->contentView; }

    float getContentOffsetX() const { return this->contentOffsetX; }
    void setContentOffsetX(float offset, bool animated = false);

    // 可滚动范围 = 内容宽度 - 可视宽度。
    float getScrollRange() const;

    void setShowScrollingIndicator(bool show) { this->showIndicator = show; }
    void setScrollingEnabled(bool enabled) { this->scrollingEnabled = enabled; }
    void setInertiaEnabled(bool enabled) { this->inertiaEnabled = enabled; }

    void update() override;

    // 把指定视图横向滚到可见区。
    void scrollToView(View* view, bool animated = true);

    std::string getClassString() const override { return "bklv::HScrollingFrame"; }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, Style style,
        FrameContext* ctx) override;

    View* contentView = nullptr;

    float contentOffsetX = 0.0f;

    bool dragging         = false;
    float dragStartX      = 0.0f;
    float dragStartOffset = 0.0f;

    float velocity        = 0.0f;

    bool animating        = false;
    float animTarget      = 0.0f;

    bool scrollingEnabled = true;
    bool inertiaEnabled   = true;
    bool showIndicator    = true;

    View* lastAutoScrolledView = nullptr;
};

} // namespace bklv

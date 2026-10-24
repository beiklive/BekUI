#pragma once

// 自带滚动能力的 Box：省掉"手工建 ScrollingFrame + setContentView + 再 addView"的样板。
//
//   ScrollBox* list = new ScrollBox(Axis::COLUMN);   // 纵向滚动
//   list->addView(item1);                            // 直接加，自动进内容盒
//   list->addView(item2);
//
// 为什么不是 Box 自己的接口：`Box::getChildren()/addView()` 的语义是"直接子节点"，
// 改成隐式包一层滚动器会让所有遍历子节点的代码（焦点、布局、设计器）看到的树
// 和作者写的树不一致。ScrollBox 把这件事显式化：它**就是**一个容器，
// 只是把子节点收在自己的内容盒里，需要时用 getContentBox() 拿。
//
// 方向：纵向用 Axis::COLUMN、横向用 Axis::ROW（和 Box 的轴含义一致）。

#include "bklv/ui/view/box.hpp"

namespace bklv
{

class ScrollingFrame;
class HScrollingFrame;

class ScrollBox : public Box
{
  public:
    explicit ScrollBox(Axis direction = Axis::COLUMN);
    ~ScrollBox() override;

    // 子节点进内容盒（不是滚动器本身）
    void addView(View* view) override;

    Axis getScrollDirection() const { return this->direction; }
    Box* getContentBox() const { return this->content; }

    // 纵向：滚动偏移 / 范围；横向同理（取对应的那个）
    float getScrollOffset() const;
    float getScrollRange() const;
    void setScrollOffset(float offset, bool animated = false);

    // 是否显示滚动条（默认显示）
    void setShowScrollingIndicator(bool show);

    std::string getClassString() const override { return "bklv::ScrollBox"; }

  private:
    Axis direction = Axis::COLUMN;
    View* frame = nullptr;   // ScrollingFrame / HScrollingFrame
    Box* content = nullptr;  // 子节点真正挂的地方
};

} // namespace bklv

#pragma once

// 容器视图，对应 borealis 的 core/box.hpp。
//
// Box 承担三件事：
//   1. 视图树管理：增删子节点时，C++ 对象树与 Yoga 节点树保持严格同步
//   2. Flexbox 布局属性：padding / axis / direction / justifyContent / alignItems
//   3. 递归绘制子节点

#include <vector>

#include "bklv/core/input.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

class Box : public View
{
  public:
    explicit Box(Axis axis = Axis::COLUMN);
    ~Box() override;

    // ---- 视图树管理 ----

    // 追加子节点到末尾。
    void addView(View* view);

    // 插入子节点到指定位置。
    void addView(View* view, size_t position);

    // 从树中移除子节点，free 为 true 时同时删除对象。
    void removeView(View* view, bool free = true);

    // 移除并删除全部子节点。
    void clearViews();

    // 摘下全部子节点但不销毁，所有权交回调用方。
    // 需要在容器之间搬动视图、或容器不拥有子节点时使用。
    void removeAllViews();

    // 见 View::asBox 的说明。
    Box* asBox() override { return this; }

    std::vector<View*>& getChildren() { return this->children; }
    const std::vector<View*>& getChildren() const { return this->children; }
    size_t getChildrenCount() const { return this->children.size(); }

    // 按 id 在整棵子树里查找。
    View* getView(const std::string& id) override;

    // 按顺序找到第一个"可聚焦"的后代。
    View* getDefaultFocus() override;

    // 方向导航：在当前正在导航的容器里选择几何上最合适的目标。
    View* getNextFocus(FocusDirection direction, View* currentView) override;

    // 命中测试：从最上层（最后添加）的子节点开始。
    View* hitTest(Point point) override;

    // ---- Flexbox 布局属性 ----

    // 主轴方向：ROW 横向、COLUMN 纵向。
    void setAxis(Axis axis);
    Axis getAxis() const { return this->axis; }

    // 主轴排列方向（LTR / RTL）。
    void setDirection(Direction direction);

    // 主轴上的分布方式。
    void setJustifyContent(JustifyContent justify);

    // 交叉轴上的对齐方式。
    void setAlignItems(AlignItems alignment);

    // ---- 内边距 ----

    void setPadding(float padding);
    void setPadding(float top, float right, float bottom, float left);
    void setPaddingTop(float top);
    void setPaddingRight(float right);
    void setPaddingBottom(float bottom);
    void setPaddingLeft(float left);

    float getPaddingTop() const;
    float getPaddingRight() const;
    float getPaddingBottom() const;
    float getPaddingLeft() const;

    // ---- 间距 ----

    // 相邻子节点之间的间隔，等价于 CSS gap。
    void setGap(float gap);
    void setGapColumn(float gap);
    void setGapRow(float gap);

    // ---- 尺寸约束 ----

    // 子节点是否换行（flex-wrap）。
    void setFlexWrap(bool wrap);

    // ---- 类型信息 ----

    // 递归推进子节点的每帧逻辑。
    void update() override;

    std::string getClassString() const override { return "bklv::Box"; }

    const bklv::AttributeTable& attributes() const override;

  protected:
    void drawChildren(ImDrawList* drawList, float x, float y, float width, float height, FrameContext* ctx) override;

    // Box 自身不画内容，背景与边框由基类的 frame 统一处理。
    void draw(ImDrawList*, float, float, float, float, FrameContext*) override { }

  private:
    Axis axis = Axis::COLUMN;
    std::vector<View*> children;
};

} // namespace bklv

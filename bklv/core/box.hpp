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

    // ---- 焦点（默认焦点三级 + 顺序导航）----

    // 默认焦点（照 borealis B2 的 Box::getDefaultFocus）：
    //   ① 自己可聚焦 → 自己
    //   ② 上次焦点记忆（lastFocusedView，焦点变化时由 InputManager 沿父链写入）
    //   ③ setDefaultFocusedIndex 指定的索引
    //   ④ 顺序扫第一个能聚焦的子节点
    View* getDefaultFocus() override;

    // 方向导航（照 borealis B3）：**顺序索引扫描 ±1 + 父链冒泡**，不是几何最近邻。
    // 轴向不符（row 按 UP/DOWN、column 按 LEFT/RIGHT）立刻上交父容器；
    // 越界返回 nullptr 继续冒泡。顺序扫描可预期、和子节点顺序一致，实现与测试都简单。
    View* getNextFocus(FocusDirection direction, View* currentView) override;

    // 焦点落到自己的直接子节点上时，由 InputManager 沿焦点路径逐级调用。
    // Box 用它维护"上次焦点记忆"。
    void onChildFocusGained(View* directChild, View* focusedView) override;

    // 本层扫不到候选、准备上交父容器之前的最后决定机会。
    // 默认返回 newFocus（没意见）；返回其他视图 = 改写这次移动；
    // 返回 this = 焦点留在原地，容器自己负责把它滚进视野（滚动容器用）。
    virtual View* getParentNavigationDecision(View* from, View* newFocus, FocusDirection direction);

    // 上次焦点记忆。摘除子节点时会自动清掉，不会悬空。
    View* getLastFocusedView() const { return this->lastFocusedView; }
    void setLastFocusedView(View* view) { this->lastFocusedView = view; }

    // 默认焦点索引；-1 表示"没指定"。
    // 注意 borealis 这一项默认是 0，那会让第 ③ 级和"顺序扫第一个"完全等价
    // （都是 children[0]），所以这里用 -1 把"没指定"和"指定了第 0 个"分开。
    void setDefaultFocusedIndex(int index) { this->defaultFocusedIndex = index; }
    int getDefaultFocusedIndex() const { return this->defaultFocusedIndex; }

    // 这个 box（含其子树，内层可覆盖）内导航是否**循环**。默认不循环。
    // wrap 优先于跨 box 策略：能绕回本 box 内的候选时就不算"要出去"。
    void setNavigationWrap(bool wrap);
    bool isNavigationWrap() const;

    // 这个 box 的跨 box 策略，只在本 box 的边界上生效（默认 ALLOW）。
    // 不做继承：外层设了 NONE 不会把子树的内部导航也冻住。
    void setCrossBoxPolicy(CrossBoxPolicy policy) { this->crossBoxPolicy = policy; }
    CrossBoxPolicy getCrossBoxPolicy() const { return this->crossBoxPolicy; }

    // 命中测试：从最上层（最后添加）的子节点开始。
    View* hitTest(Point point) override;

    // ---- Flexbox 布局属性 ----

    // 主轴方向：ROW 横向、COLUMN 纵向。
    void setAxis(Axis axis);
    Axis getAxis() const { return this->axis; }
    JustifyContent getJustifyContent() const { return this->justifyContent; }
    AlignItems getAlignItems() const { return this->alignItems; }

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
    float getGap() const;
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
    // 在本层子节点里按顺序 ±1 找候选（不冒泡、不看跨 box 策略）。
    // from 必须是自己的直接子节点；找不到返回 nullptr。
    View* scanChildrenForFocus(FocusDirection direction, View* from);
    // 第 index 个子节点里的焦点候选（不可见/不可聚焦/被模态挡住/落在 from 子树里都不算）。
    View* focusCandidateAt(size_t index, View* from);
    // 本层扫完之后的去向：按跨 box 策略决定"拦下"还是"交给父容器"。
    View* leaveThisBox(FocusDirection direction);

    Axis axis = Axis::COLUMN;
    // 记一份，getter / 诊断要用（Yoga 那边也设了，这里是镜像）
    JustifyContent justifyContent = JustifyContent::FLEX_START;
    AlignItems alignItems        = AlignItems::STRETCH;
    std::vector<View*> children;

    View* lastFocusedView   = nullptr;
    int defaultFocusedIndex = -1;

    bool navigationWrap    = false; // 显式设过的值
    bool navigationWrapSet = false; // 是否显式设过（没设过就跟着祖先）

    CrossBoxPolicy crossBoxPolicy = CrossBoxPolicy::ALLOW;
};

} // namespace bklv

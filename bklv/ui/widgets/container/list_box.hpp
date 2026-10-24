#pragma once

// 列表容器（纵向滚动）。
//
// 结构（结构在代码里，样式在 JSON 里）：
//   ListBox  AppearanceView("base/box/list")
//     └─ scroller  ScrollBox(Axis::COLUMN)：纵向滚动器（滚动条由 ScrollingFrame 用代码画）
//         └─ content  ← addItem / addItemText 往这里挂项
//
// 它只是容器：不认数据源、不做分页、不做复用。内容一律是调用方给的视图，
// 所以"项长什么样"由项自己的外观决定，ListBox 只管：
//   · 纵向滚动（滚轮 / 拖拽 / 惯性 / 滚动条都在 ScrollingFrame 里）
//   · 焦点移动时把聚焦项滚进可视区（手柄、键盘要用这条）
//   · 空列表提示（setEmptyText）
//
// 上千条、需要分页与 cell 复用的列表用 RecyclerFrame（bklv/ui/widgets/list/recycler.hpp），
// 不要用 ListBox 硬堆 —— 那边才有 RecyclerDataSource 与 cell 池。

#include <string>

#include "bklv/ui/widgets/appearance/appearance_view.hpp"

namespace bklv
{

class Box;
class Label;
class ScrollBox;
class View;

class ListBox : public AppearanceView
{
  public:
    explicit ListBox(const std::string& variant = "");
    ~ListBox() override;

    // ---- 项 ----
    void addItem(View* view);
    // 便捷：加一条纯文字项，返回它（想再改字号/颜色自己 set）。
    Label* addItemText(const std::string& text);
    void clearItems();
    size_t getItemCount() const;

    // 项容器：想直接 addView 也行（等价于 addItem）。
    Box* getItemBox() const { return this->content; }

    // ---- 滚动 ----
    void scrollToTop(bool animated = false);
    void scrollToItem(View* item, bool animated = true);
    void setShowScrollbar(bool show);
    float getScrollOffset() const;
    float getScrollRange() const;

    // ---- 空态 ----
    // 没有项时显示的一句提示；空串表示不显示。
    void setEmptyText(const std::string& text);

    void applyTheme() override;
    void update() override;
    // 焦点进到列表里时把那一项滚进可视区。
    void onChildFocusGained(View* directChild, View* focusedView) override;

    std::string getClassString() const override { return "bklv::ListBox"; }

  private:
    void refreshEmptyVisibility();

    ScrollBox* scroller = nullptr;
    Box* content        = nullptr;
    Label* empty         = nullptr;
    std::string emptyText;
};

} // namespace bklv

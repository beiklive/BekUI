#pragma once

// TabFrame：横向 tab 栏 + 内容区（对应 borealis 的 views/tab_frame）。
//
// 和 demo 里那个手写 TabBar 的区别：这是库级实现，页面切换、选中态、
// tab 的外观（含选中/聚焦动效）都在这里，使用方只管 addTab()。
//
// tab 格子的外观来自主题的 `widgets/tab_cell.json`；内容区只放当前选中的页面，
// 其他页面**只脱离不销毁**（切回来还在，状态不丢）。

#include <functional>
#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

// 单个 tab 格子：外观驱动（底 + 文字），带 selected / focused 两个状态。
class TabCell : public AppearanceView
{
  public:
    using SelectHandler = std::function<void()>;

    TabCell();

    void setTitle(const std::string& title);
    void setSelected(bool selected);
    bool isSelected() const { return this->selected; }

    void setOnSelect(SelectHandler handler) { this->onSelect = std::move(handler); }

    // 聚焦到本项时触发（侧栏靠它实现"焦点到哪个 tab 就切哪个子页面"）
    void setOnFocus(std::function<void()> handler) { this->onFocus = std::move(handler); }

    void trigger();

    std::string getClassString() const override { return "bklv::TabCell"; }

  protected:
    void onFocusGained() override;
    void onFocusLost() override;

  private:
    bool selected = false;
    SelectHandler onSelect;
    std::function<void()> onFocus;
};

class TabFrame : public Box
{
  public:
    TabFrame();

    // content 的所有权交给 TabFrame（切换时只脱离不销毁，析构时统一释放）
    void addTab(const std::string& title, View* content);

    void setSelectedIndex(size_t index);
    size_t getSelectedIndex() const { return this->selected; }
    size_t getTabCount() const { return this->cells.size(); }

    // 换肤时同步 tab 标题（文案在控件构造时解析，不会自动重译）
    void setTabTitle(size_t index, const std::string& title);

    Box* getTabBar() const { return this->tabBar; }
    Box* getContentBox() const { return this->contentBox; }

    std::string getClassString() const override { return "bklv::TabFrame"; }

  private:
    Box* tabBar       = nullptr;
    Box* contentBox   = nullptr;
    size_t selected   = 0;
    std::vector<TabCell*> cells;
    std::vector<View*> contents;
};

} // namespace bklv

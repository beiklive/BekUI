#pragma once

// TabBar：底部导航条（横排 tab，条目多了横向滚动）。
//
// 和 TabFrame 的分工：TabFrame 自己管"tab 栏 + 内容区"；TabBar **只管那条栏**，
// 内容由宿主负责（demo 里是 Pager）—— 苹果风的底部导航就是这个形状。
//
// 外观：条本身用 `widget/tab_bar`（两套主题共用同一份 JSON：条底来自统一的
// `base/glass_surface.chrome`，所以 default 是实心、ios 是玻璃）；
// 条目复用 `base/tab_cell`（ios 主题里选中态是圆角胶囊）。
//
// 交互语义：**A / 点击才切**（和 iOS 底部 tab 一致），焦点只是移过去；
// 想要"焦点到哪就切哪"（Switch 侧栏那种）用 Sidebar，别用这个。

#include <functional>
#include <string>
#include <vector>

#include "bklv/views/h_scrolling_frame.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "bklv/widgets/tab_frame.hpp" // TabCell

namespace bklv
{

class TabBar : public AppearanceView
{
  public:
    TabBar();

    // 加一项。onSelect：选中时触发（index 从 0 起）；
    // onFocus：焦点移到这一项时触发（可选）。
    void addItem(const std::string& title, std::function<void()> onSelect,
        std::function<void()> onFocus = nullptr);
    void clearItems();

    void setSelectedIndex(size_t index);
    size_t getSelectedIndex() const { return this->selected; }
    size_t getItemCount() const { return this->cells.size(); }

    // 第 index 个条目（`TabCell`；越界返回 nullptr）
    View* getItemView(size_t index) const;

    // 条目容器（横排、可滚动；演示/测试用）
    Box* getItemBox() const { return this->itemBox; }

    void applyTheme() override;
    std::string getClassString() const override { return "bklv::TabBar"; }

  private:
    HScrollingFrame* itemScroller = nullptr;
    Box* itemBox                  = nullptr;
    std::vector<TabCell*> cells;
    size_t selected = 0;
};

} // namespace bklv

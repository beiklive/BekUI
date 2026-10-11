#pragma once

// Tab 栏。
//
// 结构：HScrollingFrame（横向滚动）
//         └─ 内容 Box（ROW）：一串 TabCell
//
// 两个要点（都是踩过坑才写对的）：
//   1. TabCell 继承 RecyclerCell 只是为了复用"可复用控件"那套接口，
//      必须传 absolutePosition=false —— 否则会被绝对定位到同一个原点，全部叠在一起。
//   2. 内容宽度要在 onLayout() 里按实测宽度算。加 tab 的当下还没布局，
//      量到的宽度是 0，只能用默认值兜底，结果就是 tab 之间空一大截。

#include <functional>
#include <string>
#include <vector>

#include "bklv/views/h_scrolling_frame.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/recycler.hpp"

namespace demo
{

// 单个 tab 格子。
class TabCell : public bklv::RecyclerCell
{
  public:
    TabCell();

    void setTab(const std::string& name, bool selected);
    void setSelected(bool selected);

    bool isFocusable() const override { return true; }
    void onFocusGained() override;
    void onFocusLost() override;

  private:
    bklv::Label* label = nullptr;
    bool selected = false;
};

// tab 栏本体。
class TabBar : public bklv::HScrollingFrame
{
  public:
    TabBar();

    // onSelect：点击或按确认键时触发（切页）
    // onFocus：焦点移到这个 tab 上时触发（tab 栏的直觉是"焦点即选中"）
    void addTab(const std::string& name, const std::function<void()>& onSelect,
        const std::function<void()>& onFocus);

    void setSelectedIndex(size_t index);

    // 内容宽度按实测值算
    void onLayout() override;

    // 焦点落在哪个 tab 上就通知哪个
    void update() override;

  private:
    bklv::Box* content = nullptr;
    std::vector<TabCell*> cells;
    std::vector<std::function<void()>> focusHooks;
    size_t lastFocusedIndex = static_cast<size_t>(-1);
};

} // namespace demo

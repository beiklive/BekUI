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

class Label;

// 单个 tab 格子：外观驱动（底 + 文字），带 selected / focused 两个状态。
// Tab 条目。方向决定用哪套外观（`base/frame/tab_button.horizontal` / `.vertical`），
// 交互逻辑只有一份 —— 样式差异全在 JSON 里。
enum class TabOrientation
{
    HORIZONTAL,
    VERTICAL,
};

class TabCell : public AppearanceView
{
  public:
    using SelectHandler = std::function<void()>;

    TabCell();

    void setTitle(const std::string& title);

    // 方向 → 外观变体；构造后默认按宿主 TabFrame 的方向设置
    void setOrientation(TabOrientation orientation);
    TabOrientation getOrientation() const { return this->orientation; }
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
    TabOrientation orientation = TabOrientation::HORIZONTAL;
    bool selected = false;
    SelectHandler onSelect;
    std::function<void()> onFocus;
};

// 触发模式（用户拍板）：
//   CLICK —— 点击 / A 才切页，切完**焦点留在 tab 上**
//   FOCUS —— 焦点移到哪个 tab 就切哪个；点击 / A 后**焦点进入子页面默认焦点**
enum class TabTriggerMode
{
    CLICK,
    FOCUS,
};

// 栏的位置：START = 横向在顶部 / 纵向在左侧（默认）；END = 交换到另一端
enum class TabBarPlacement
{
    START,
    END,
};

class TabFrame : public Box
{
  public:
    explicit TabFrame(TabOrientation orientation = TabOrientation::HORIZONTAL,
        TabTriggerMode trigger = TabTriggerMode::CLICK);

    // content 的所有权交给 TabFrame（切换时只脱离不销毁，析构时统一释放）
    size_t addTab(const std::string& title, View* content);

    void setSelectedIndex(size_t index);
    size_t getSelectedIndex() const { return this->selected; }
    size_t getTabCount() const { return this->cells.size(); }

    // 换肤时同步 tab 标题（文案在控件构造时解析，不会自动重译）
    void setTabTitle(size_t index, const std::string& title);
    void setTabIcon(size_t index, const std::string& icon);

    // ---- 方向 / 位置 / 触发 ----
    void setOrientation(TabOrientation value);
    TabOrientation getOrientation() const { return this->orientation; }
    void setTriggerMode(TabTriggerMode value);
    TabTriggerMode getTriggerMode() const { return this->trigger; }
    void setBarPlacement(TabBarPlacement value);
    TabBarPlacement getBarPlacement() const { return this->placement; }

    // 纵向分组线；横向忽略（打一条日志）
    void addSeparator();
    // 栏标题（纵向侧栏常见；横向时显示在栏起始处）
    void setTitle(const std::string& title);
    const std::string& getTitle() const { return this->barTitle; }

    // 带图标的 tab（图标层由外观声明）
    size_t addTab(const std::string& title, const std::string& icon, View* content);

    // 自定义 tab 按钮：传进来的 TabCell（或其子类）会被接管，宿主不要再 delete
    size_t addTab(TabCell* button, View* content);

    // ---- 部件样式替换（TabFrame 是聚合体，四个部件各自可换）----
    // 栏容器 / 内容框 / 按钮：默认就是 base/frame/tab_bar、base/frame/tab_content、
    // base/button/tab_button；换名字后按方向自动取 <name>.horizontal / <name>.vertical。
    void setBarAppearance(const std::string& name);
    void setContentAppearance(const std::string& name);
    void setTabButtonAppearance(const std::string& name);
    const std::string& getBarAppearance() const { return this->barAppearance; }
    const std::string& getContentAppearance() const { return this->contentAppearance; }
    const std::string& getTabButtonAppearance() const { return this->buttonAppearance; }

    Box* getTabBar() const { return this->itemBox; }
    // 栏容器（含外观）：测试/调试用，看它在布局里的位置
    View* getBarContainer() const { return this->barFrame; }
    Box* getContentBox() const { return this->contentBox; }

    std::string getClassString() const override { return "bklv::TabFrame"; }

  private:
    void rebuildLayout();          // 按方向/位置重建"栏 + 内容框"的结构
    void applyBarAppearance();     // 栏容器按方向切外观变体
    void applyCellTrigger(TabCell* cell, size_t index);
    void selectFromInput(size_t index, bool focusContent);
    static View* firstFocusableIn(View* root);

    TabOrientation orientation = TabOrientation::HORIZONTAL;
    TabTriggerMode trigger      = TabTriggerMode::CLICK;
    TabBarPlacement placement   = TabBarPlacement::START;

    // 三个部件的外观名（默认值在构造函数里给；TabFrame 本身不进视图 JSON）
    std::string barAppearance     = "base/frame/tab_bar";
    std::string contentAppearance = "base/frame/tab_content";
    std::string buttonAppearance  = "base/button/tab_button";

    AppearanceView* barFrame = nullptr;   // 栏容器的外观（<barAppearance>.<方向>）
    Box* barScroller         = nullptr;   // HScrollingFrame / ScrollingFrame（按轴）
    Box* itemBox             = nullptr;   // 条目容器
    AppearanceView* contentBox = nullptr;
    size_t selected          = 0;
    std::string barTitle;
    Label* barTitleLabel = nullptr;
    std::vector<TabCell*> cells;
    std::vector<View*> contents;
};

} // namespace bklv

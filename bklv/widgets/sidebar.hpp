#pragma once

// Sidebar：竖排导航（对应 borealis 的 views/sidebar）。
// Switch 上竖排 tab 用得比横排多，所以它和 TabFrame 共享同一套"格子 + 选中态"逻辑，
// 只是排列方向不同、且带标题栏。
//
// 外观全部来自主题：条目用 `widgets/tab_cell.json`（同一个格子外观，竖排复用），
// 面板底/标题栏用 `widgets/sidebar.json`。

#include <functional>
#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/scrolling_frame.hpp"
#include "bklv/widgets/tab_frame.hpp" // TabCell

namespace bklv
{

class Sidebar : public Box
{
  public:
    explicit Sidebar(const std::string& title = "");

    // 加一项；点击/回车时回调（index 从 0 起）
    // onActivate：**聚焦到这一项**时触发（borealis 的 SidebarItem::onFocusGained →
    // group->setActive(this)，所以"焦点到哪个 tab 就切到哪个子页面"）；
    // onConfirm：按 A / 点击确认时触发（再去把焦点交给子页面的默认控件）。
    // 老写法只传一个回调时，它同时承担这两个时机。
    void addItem(const std::string& title, std::function<void()> onActivate,
        std::function<void()> onConfirm = nullptr);
    // 带图标的版本：icon 是图标字体的字形（`icon::glyphByName("palette")`）。
    // 图标层由主题的条目外观声明（`base/tab_cell.sidebar.json` 里的 "icon" 层）。
    void addItem(const std::string& title, const std::string& icon,
        std::function<void()> onActivate, std::function<void()> onConfirm = nullptr);

    // 分组用的分隔（不可聚焦，不进条目索引）：条目多了靠它分组，别塞一条假 tab
    void addSeparator();
    void clearItems();

    void setSelectedIndex(size_t index);
    size_t getSelectedIndex() const { return this->selected; }
    size_t getItemCount() const { return this->cells.size(); }

    // 取第 index 个条目的视图（要给它焦点时用；越界返回 nullptr）
    View* getItemView(size_t index) const;

    void setItemTitle(size_t index, const std::string& title);
    std::string getItemTitle(size_t index) const;
    // 侧栏自己的标题
    void setTitle(const std::string& value);
    const std::string& getTitle() const { return this->title; }

    // 面板宽度（默认取主题 metric.sidebar.width）
    void setPanelWidth(float width);

    // 条目容器（可滚动，条目多了滚动条会跟随焦点）
    Box* getItemBox() const { return this->itemBox; }
    bklv::ScrollingFrame* getItemScroller() const { return this->itemScroller; }

    std::string getClassString() const override { return "bklv::Sidebar"; }

    // 换肤时刷新面板底色/圆角/内边距（标题的字体颜色由 Label::applyTheme 处理）
    void applyTheme() override { this->applyPanelLook(); }

    // 面板投影：面板底/边框由 View::drawBackground 画（观感来自 box 段），
    // 投影没有对应的 box 字段，所以在这里补一层（cutout = 只画外圈，不会把面板压暗）
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        FrameContext* ctx) override;

    // 面板外观（底/标题）由主题描述；这里声明它用哪份外观
    const AttributeTable& attributes() const override;

  private:
    void applyPanelLook();

    std::string title;
    Label* titleLabel = nullptr;
    Box* headerBox = nullptr;
    bklv::ScrollingFrame* itemScroller = nullptr;
    Box* itemBox   = nullptr;
    size_t selected = 0;
    std::vector<TabCell*> cells;
    std::vector<View*> separators; // 只为了 clearItems 时一并摘掉
};

} // namespace bklv

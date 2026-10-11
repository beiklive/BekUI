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
#include "bklv/widgets/tab_frame.hpp" // TabCell

namespace bklv
{

class Sidebar : public Box
{
  public:
    explicit Sidebar(const std::string& title = "");

    // 加一项；点击/回车时回调（index 从 0 起）
    void addItem(const std::string& title, std::function<void()> onSelect);
    void clearItems();

    void setSelectedIndex(size_t index);
    size_t getSelectedIndex() const { return this->selected; }
    size_t getItemCount() const { return this->cells.size(); }

    void setItemTitle(size_t index, const std::string& title);
    std::string getItemTitle(size_t index) const;
    // 侧栏自己的标题
    void setTitle(const std::string& value);
    const std::string& getTitle() const { return this->title; }

    // 面板宽度（默认取主题 metric.sidebar.width）
    void setPanelWidth(float width);

    Box* getItemBox() const { return this->itemBox; }

    std::string getClassString() const override { return "bklv::Sidebar"; }

    // 换肤时刷新面板底色/圆角/内边距（标题的字体颜色由 Label::applyTheme 处理）
    void applyTheme() override { this->applyPanelLook(); }

    // 面板外观（底/标题）由主题描述；这里声明它用哪份外观
    const AttributeTable& attributes() const override;

  private:
    void applyPanelLook();

    std::string title;
    Label* titleLabel = nullptr;
    Box* headerBox = nullptr;
    Box* itemBox   = nullptr;
    size_t selected = 0;
    std::vector<TabCell*> cells;
};

} // namespace bklv

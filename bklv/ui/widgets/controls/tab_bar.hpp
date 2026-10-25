#pragma once

// 标签条：**顶部水平**与**左侧竖直**两套排列（同一套代码，靠外观变体区分）。
//
//   TabBar(horizontal)  "base/tabbar"           + 每项 "base/tabitem"
//   TabBar(vertical)    "base/tabbar.vertical"  + 每项 "base/tabitem.vertical"
//
// 职责边界：
//   TabBar    只负责"哪一项是选中的"和把点击/确认转成 setActiveIndex()
//   TabView   负责真正的页面切换、焦点与滚动记忆
//   宿主动手  把 onTabSelected 接到 TabView::setActiveTab()，再把 TabView 的
//             onTabChanged 接回 TabBar::setActiveIndex()（双向，见 demo）
//
// TabBar 自身不可聚焦；可聚焦的是每一项 `TabItem`（左右/上下键在条内走，
// 确认后由宿主把焦点交给内容页）。

#include <functional>
#include <string>
#include <vector>

#include "bklv/ui/widgets/appearance/appearance_view.hpp"

namespace bklv
{

class TabItem : public AppearanceView
{
  public:
    TabItem(const std::string& label, const std::string& glyph, bool vertical);
    ~TabItem() override;

    void setSelected(bool selected);
    bool isSelected() const { return this->selected; }

    // 点一下 / 按确认时执行（TabBar 用它切页）
    void setOnActivate(std::function<void()> callback) { this->onActivate = std::move(callback); }

    // 排列变了：换外观变体（水平 "base/tabitem"、竖直 "base/tabitem.vertical"）
    void setVertical(bool vertical);

    std::string getClassString() const override { return "bklv::TabItem"; }

  private:
    void applyLayout();

    std::string label;
    std::string glyph;
    bool vertical  = false;
    bool selected  = false;
    std::function<void()> onActivate;
};

class TabBar : public AppearanceView
{
  public:
    enum class Orientation
    {
        HORIZONTAL, // 顶部：横排
        VERTICAL,   // 左侧：竖排
    };

    explicit TabBar(Orientation orientation = Orientation::HORIZONTAL);
    ~TabBar() override;

    int addItem(const std::string& label, const std::string& glyph, const std::string& id = std::string());

    void setOrientation(Orientation orientation);
    Orientation getOrientation() const { return this->orientation; }
    bool isVertical() const { return this->orientation == Orientation::VERTICAL; }

    // notify = true 时触发 onTabSelected（宿主接 TabView 用）
    void setActiveIndex(int index, bool notify = true);
    int getActiveIndex() const { return this->active; }

    void setOnTabSelected(std::function<void(int)> callback) { this->onTabSelected = std::move(callback); }

    int getItemCount() const { return (int)this->items.size(); }
    TabItem* getItem(int index) const;

    std::string getClassString() const override { return "bklv::TabBar"; }

  private:
    void applyLayout();

    std::vector<TabItem*> items;
    std::vector<std::string> ids;
    Orientation orientation = Orientation::HORIZONTAL;
    int active              = -1;
    std::function<void(int)> onTabSelected;
};

} // namespace bklv

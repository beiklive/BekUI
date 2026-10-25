#pragma once

// 标签页的内容宿主：持有 N 个页面，只显示当前那个，其余 `GONE`。
//
// 为什么不是"切换就重建"：tab 页要保留状态（滚动位置、输入框内容、焦点）。
// 所以非当前页只是**隐藏**（`GONE`：不参与布局、不进焦点链），视图对象与内部
// 状态都还在；切回来时把焦点与滚动写回去。
//
// 三种"页面级切换"的分工：
//   TabView          同级切换、**常驻**、保留状态（本文件）
//   ContentStack     层级下钻、出场播完**释放**（`ui/shell/content_stack.hpp`）
//   Page + 重建      最简单、状态全丢（demo 现在的主页面栈）
//
// 记忆的 key：`"tab:" + 页面 id`，按 id 而不是下标记（中间插一个 tab 也不会错位）。
// 焦点用 `InputManager::rememberFor/loadFocusFor`（存路径），滚动用
// `View::hasRestorableState/getRestorableState/setRestorableState`。

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "bklv/ui/widgets/appearance/appearance_view.hpp"

namespace bklv
{

class TabView : public AppearanceView
{
  public:
    explicit TabView(const std::string& variant = std::string());
    ~TabView() override;

    // 加一个页面（接管所有权）。id 用于记忆，留空则用序号。
    int addTab(const std::string& id, View* page);

    // 切到第 index 个。animated 暂时未实现（下一版接 ViewTransition）。
    void setActiveTab(int index, bool animated = false);
    void setActiveTabId(const std::string& id, bool animated = false);

    int getActiveIndex() const { return this->active; }
    View* getActivePage() const;
    View* getPage(int index) const;
    int getTabCount() const { return (int)this->tabs.size(); }

    // 切换后回调（做 tab 条联动用；index 从 0 开始）
    void setOnTabChanged(std::function<void(int, View*)> callback) { this->onTabChanged = std::move(callback); }

    std::string getClassString() const override { return "bklv::TabView"; }

  private:
    struct Entry
    {
        std::string id;
        View* page     = nullptr;
        bool hasScroll = false;
        float scroll   = 0.0f;
    };

    static View* findRestorable(View* view);
    static void giveFocusTo(View* view);

    std::string memoryKey(const Entry& entry) const;
    void rememberCurrent();
    void restoreCurrent();

    std::vector<Entry> tabs;
    int active = -1;
    std::function<void(int, View*)> onTabChanged;
};

} // namespace bklv

#pragma once

// demo 的应用外壳（推倒重做的版本）：
//
//   AppletFrame（页头 = 当前页名 + 说明，底栏 = 提示栏 [按键图标] 说明 …）
//     └─ Box(ROW)
//          ├─ Sidebar（页面列表）
//          └─ Box(COLUMN)  ← 右侧子页面
//               ├─ 右上角：日志窗口开关
//               └─ 内容区（长页面进 ScrollingFrame）
//
// 侧栏是**菜单语义**：上下移动只移动焦点，按 A（或鼠标点）才切页；
// 切页之后焦点会自动进到新页面的第一个可聚焦控件（和 borealis 的 TabFrame 一致）。

#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/views/debug_window.hpp"
#include "bklv/views/scrolling_frame.hpp"
#include "bklv/widgets/applet_frame.hpp"
#include "bklv/widgets/sidebar.hpp"

namespace demo
{

class DemoApp
{
  public:
    DemoApp();

    bklv::View* getView() { return this->root; }

    void update();

    void drawWindows() { this->debugWindow.draw(); }
    void setLogWindowEnabled(bool enabled) { this->debugWindow.setEnabled(enabled); }
    bool isLogWindowEnabled() const { return this->debugWindow.isEnabled(); }

    // 供外部统计展示
    bklv::View* getMainList() const { return this->mainList; }

    // 测试/演示用：当前页下标与页数
    size_t getCurrentPage() const { return this->currentPage; }
    size_t getPageCount() const { return this->pages.size(); }

  private:
    struct Page
    {
        std::string name;
        std::string subtitle;
        bklv::Box* view      = nullptr;
        bool selfScrolling   = false;
    };

    void addPage(const std::string& name, const std::string& subtitle, bklv::Box* view,
        bool selfScrolling);
    void selectPage(size_t index);
    void sizeSlotToContent();

    bklv::AppletFrame* root   = nullptr;
    bklv::Sidebar* sidebar    = nullptr;

    // 内容区两个互斥容器：不自管滚动的页放 scroller，自管滚动的页直接放 pageSlot
    // （共用一个容器时，切页会让容器高度塌成上一页的高度）
    bklv::Box* contentHost    = nullptr;
    bklv::ScrollingFrame* scroller = nullptr;
    bklv::Box* pageSlot       = nullptr;

    std::vector<Page> pages;
    size_t currentPage        = static_cast<size_t>(-1);
    bklv::View* mainList      = nullptr;

    bklv::DebugWindow debugWindow;
};

} // namespace demo

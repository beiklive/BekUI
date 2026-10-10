#pragma once

// 页面宿主：标题 + tab 栏 + 内容区，负责切页。
//
// 切页最容易踩的两个坑（都在下面实现了）：
//   1. 视图在"内容区"和"ScrollingFrame"之间搬动时，必须先 evict 再 add。
//      ScrollingFrame::setContentView 会销毁旧内容，直接覆盖会让另一方留下悬空指针。
//   2. 回调会活到程序结束，所以捕获的 pages 等局部变量必须**按值**拷贝，
//      捕获引用会悬空。

#include <string>
#include <vector>

#include "app/pages/pages.hpp"
#include "app/tab_bar.hpp"
#include "bklv/views/debug_window.hpp"
#include "bklv/core/app.hpp"
#include "bklv/core/box.hpp"
#include "bklv/views/scrolling_frame.hpp"

namespace demo
{

// 一个可聚焦的按钮。用 Box + Label 手绘，跟 tab 格子同一套写法。
class TextButton : public bklv::Box
{
  public:
    explicit TextButton(const std::string& text);

    void setText(const std::string& text) { this->label->setText(text); }

    bool isFocusable() const override { return true; }
    void onFocusGained() override;
    void onFocusLost() override;

  private:
    bklv::Label* label = nullptr;
};

class PageHost
{
  public:
    PageHost();

    // 交给 App 的根视图
    bklv::View* getView() { return this->root; }

    // 每帧同步一次自滚动页的高度
    void update();

    // 画 ImGui 调试窗口（日志窗口）。必须在 ImGui::Render() 之前调用。
    void drawWindows() { this->debugWindow.draw(); }

    // 日志窗口开关，供界面上的按钮切换
    void setLogWindowEnabled(bool enabled) { this->debugWindow.setEnabled(enabled); }
    bool isLogWindowEnabled() const { return this->debugWindow.isEnabled(); }
    void toggleLogWindow() { this->debugWindow.toggle(); }

    // 供外部拿来做统计展示
    bklv::RecyclerFrame* getMainList() const { return this->mainList; }

  private:
    void addPage(const std::string& name, bklv::Box* view, bool selfScrolling);

    // 自滚动页要有一个确定高度才能撑满内容区（height:100% 在 Yoga 里依赖父高度）。
    void sizeSlotToContent();
    void selectPage(size_t index);

    bklv::Box* root = nullptr;
    TabBar* tabBar = nullptr;

    // 内容区用两个互斥的容器，而不是一个容器里来回换：
    //   scroller —— 容纳不自管滚动的页面（页面高度由内容决定）
    //   pageSlot —— 容纳自滚动页面（页面自己撑满）
    // 共用一个容器时，切换会让容器高度塌成上一页的高度（实测 h=0，页面全空）。
    bklv::Box* contentHost = nullptr; // 舞台：下面两个容器二选一挂进来
    bklv::ScrollingFrame* scroller = nullptr;
    bklv::Box* pageSlot = nullptr;

    std::vector<bklv::Box*> pages;
    std::vector<bool> selfScrolling;
    size_t currentPage = static_cast<size_t>(-1);

    bklv::DebugWindow debugWindow;
    bklv::RecyclerFrame* mainList = nullptr;
};

} // namespace demo

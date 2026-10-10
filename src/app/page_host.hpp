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
#include "bklv/core/app.hpp"
#include "bklv/core/box.hpp"
#include "bklv/views/scrolling_frame.hpp"

namespace demo
{

class PageHost
{
  public:
    PageHost();

    // 交给 App 的根视图
    bklv::View* getView() { return this->root; }

    // 每帧刷新（目前只有调试页的统计文字）
    void update();

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

    bklv::RecyclerFrame* mainList = nullptr;
    bklv::Label* statsLabel = nullptr;
    float statsTimer        = 0.0f;
};

} // namespace demo

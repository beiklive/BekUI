#pragma once

// demo 的应用外壳（Shell + Pager 版）：
//
//   Shell（四层结构，见 bklv/widgets/shell.hpp）
//     ├─ BackdropLayer  背景（本期不放东西）
//     ├─ ContentLayer   ← **显式留白**：padding top/bottom = 页头/底栏高度
//     │                    ├─ 基础内容（下钻栈的栈底）：body(ROW)
//     │                    │    ├─ Sidebar（页面列表）
//     │                    │    └─ right(COLUMN)[ 顶部按钮行 | scroller ]
//     │                    │         scroller = Pager 的槽（同级换页在这里换内容）
//     │                    └─ 下钻出来的详情页（Shell::pushContentView）
//     ├─ OverlayLayer   header | Padding(grow) | footer（**纯显示，不参与焦点**）
//     └─ HudLayer       （空；浮层留给定层提示）
//
// 页面是**工厂**：切页时 Pager 现建新页、删旧页，所以页面不能跨切页保存内部指针。
//
// 侧栏是**菜单语义**：上下移动只移动焦点，按 A（或鼠标点）才把焦点交给子页面；
// 页头显示当前页名。

#include <string>
#include <vector>

#include "bklv/bklv.hpp"

#include "app/pages.hpp"

namespace demo
{

class DemoApp
{
  public:
    DemoApp();
    ~DemoApp();

    bklv::View* getView() { return this->shell; }

    void update();

    // 日志窗口：视图树里的 bklv::DebugLogView，挂在 Shell 的**调试层**（HudLayer 之上）
    void setLogWindowEnabled(bool enabled);
    bool isLogWindowEnabled() const;

    // 供外部统计展示（列表页没建过时为 nullptr —— 页面是懒加载的）
    bklv::View* getMainList() const { return this->mainList; }

    // 测试/演示用：当前页下标与页数
    size_t getCurrentPage() const { return this->currentPage; }
    size_t getPageCount() const { return this->pages.size(); }

    // ---- 页头 / 底栏（设置页会通过这些回调操作它们）----
    void setHeaderVisible(bool visible);
    void setFooterVisible(bool visible);
    bool isHeaderVisible() const { return this->headerVisible; }
    bool isFooterVisible() const { return this->footerVisible; }
    void setBarsAnimated(bool animated) { this->barsAnimated = animated; }
    void setBarAnimationDuration(float seconds) { this->barDuration = seconds; }

  private:
    struct Page
    {
        std::string id;
        std::string name;
        std::string subtitle;
        bool selfScrolling = false;
    };

    void addPage(const std::string& id, const std::string& name, const std::string& subtitle,
        bool selfScrolling, const char* iconName);
    // 按页下标分派到 pages.hpp 的构建函数
    bklv::Box* buildPage(size_t index);
    // 页面外面套一层内容内边距（对标 borealis 的 tab_frame/content_padding_*）
    bklv::View* wrapPage(bklv::Box* page, bool selfScrolling);

    // focusContent：切页之后要不要把焦点交给子页面的默认控件。
    // 侧栏聚焦过来时是 false（只切页），按 A 确认时是 true（borealis 的语义）
    void selectPage(size_t index, bool focusContent);

    // 内容层按页头/底栏高度留白（insets 用**目标值**，见方案 5.4）
    void applyContentInsets();

    // 栏折叠补间（照 AppletFrame 那套：动画期间每帧改高度 + 请求重排）
    void startBarAnimation();
    void updateBars();
    void finishBarAnimation();

    // B 路由的第 3 级：内容层还有下钻的层就退回上一层
    bool handleBackRequest();

    bklv::Shell* shell         = nullptr;
    bklv::Header* header       = nullptr;
    bklv::BottomBar* footer    = nullptr;
    bklv::Box* contentRoot     = nullptr;
    bklv::Sidebar* sidebar     = nullptr;
    bklv::ScrollingFrame* scroller = nullptr;
    bklv::Pager* pager         = nullptr;

    std::vector<Page> pages;
    size_t currentPage = static_cast<size_t>(-1);
    bklv::View* mainList = nullptr;

    bool headerVisible = true;
    bool footerVisible = true;
    bool barsAnimated  = true;
    float barDuration  = 0.18f; // 秒
    bklv::Animatable headerAnim{ 1.0f };
    bklv::Animatable footerAnim{ 1.0f };
    bool animating = false;

    // 起始页在**首帧**才选（构造期选页会拿到"字体还没装载"的空图标表；而且
    // App::setRootView 会把焦点给侧栏第 0 项，侧栏又是"聚焦即切页"，构造期设的页会被它覆盖）
    int pendingStartPage = 0;

    bklv::DebugLogView* logView = nullptr;
};

} // namespace demo

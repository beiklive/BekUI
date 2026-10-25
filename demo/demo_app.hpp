#pragma once

// demo 的宿主外壳（最小框架）：把"窗口 + 顶栏 + 内容区 + 底栏"和"页面栈"搭好，
// 页面只负责往内容区里塞视图。
//
// 层次：
//   demo/main.cpp        命令行参数、构造 DemoApp、run
//   demo/demo_app.*      外壳与页面栈（本文件）
//   demo/page.hpp        页面接口
//   demo/pages/*         具体页面
//
// 想加一个页面：继承 demo::Page 实现 build()，再从某个按钮的回调里
// `app.pushPage(new MyPage())` 即可，不用改这里。
//
// 注意：页面里的按钮回调**不能直接**改页面栈 —— 回调执行时正在用那棵子树，
// 里面一旦 clearViews() 就把自己删了。所以导航统一走 goBack() / pushPage()，
// 它们把动作推迟到下一帧开头执行。

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "bklv/ui/app/app.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/controls/button.hpp"
#include "bklv/ui/widgets/controls/hint_bar.hpp"
#include "bklv/ui/shell/shell.hpp"

namespace demo
{

class Page;

class DemoApp
{
  public:
    DemoApp();
    ~DemoApp();

    DemoApp(const DemoApp&)            = delete;
    DemoApp& operator=(const DemoApp&) = delete;

    bklv::App& app() { return this->application; }

    // 外壳（五层容器 + 焦点框 + 内容层层级栈）。页面做“下钻”时用 pushContentView。
    bklv::Shell* shell() const { return this->root; }

    // 压一层列表页到内容层层级栈（demo 的'层级页'按钮与 --page stack 都用它）
    void openStackList();
    void openStackDetail(int index);

    // ---- 导航（都推迟到下一帧执行，见文件头说明）----

    // 压入一个页面（接管所有权）。
    void pushPage(Page* page);

    // 返回上一页；栈里只剩一页时什么都不做。
    void goBack();

    // demo 自带的两个页面入口（页面之间不互相 include，集中在这里）。
    void openSettings();

    // 压一层"标签页"演示（TabView + TabBar）
    void openTabs(bool vertical = false);

    // ---- 主题 ----

    // 在可用主题之间轮换（default ↔ glass），切完弹一条通知。
    void cycleTheme();

    // 显式指定主题；不存在的主题返回 false 并保持原样。
    bool setTheme(const std::string& name);

    const std::string& getThemeName() const { return this->themeName; }

    // 起窗口并进入帧循环。root 由 App 接管，退出时释放。
    int run(const std::string& title, int width, int height);

  private:
    void buildShell();
    void showCurrentPage();
    void refreshHeader();
    // 换肤后重新取一遍底色与文字色（视图树不动，只改颜色）。
    void applyThemeColors();

    // 把一次导航动作排到下一帧开头。
    void defer(std::function<void()> action);

    Page* currentPage() const;

    bklv::App application;

    bklv::Shell* root         = nullptr;
    bklv::Box* content        = nullptr;
    bklv::Label* titleLabel   = nullptr;
    bklv::BottomBar* bottomBar    = nullptr;
    bklv::TextButton* backButton  = nullptr;

    std::vector<std::unique_ptr<Page>> pages;
    // 待执行的导航动作（按插入顺序在下一帧开头依次执行）。
    std::vector<std::function<void()>> pendingNavigation;
    std::string themeName = "default";
};

} // namespace demo

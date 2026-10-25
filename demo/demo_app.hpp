#pragma once

// demo 的外壳：顶栏（标题）+ 标签条 + 标签页内容。
//
// 主体是"tab + 子页面"：
//   第 1 页「按钮」—— 按钮家族，以及"切换标签条排列"的开关
//   第 2 页「布局」—— Box 的轴 / 间距 / 对齐 / 拉伸（用彩色矩形看几何）
//
// 两个标签页**常驻**（TabView 负责），切走再切回来状态还在；
// 这是"同级切换"，跟内容层下钻（Shell 的 ContentStack）不是一回事。
//
// 层次：
//   demo/main.cpp          命令行参数、构造 DemoApp、run
//   demo/demo_app.*        外壳 + 标签页装配（本文件）
//   demo/tabs/*.cpp        两个标签页的内容
//   demo/style.hpp         取色与文字规格
//   demo/layout_kit.hpp    "一节"与"彩色矩形"两个排版小工具

#include <string>

#include "bklv/ui/app/app.hpp"

namespace bklv
{
class BottomBar;
class Box;
class Label;
class Shell;
class TabBar;
class TabView;
} // namespace bklv

namespace demo
{

class DemoApp
{
  public:
    DemoApp();
    ~DemoApp();

    DemoApp(const DemoApp&)            = delete;
    DemoApp& operator=(const DemoApp&) = delete;

    bklv::App& app() { return this->application; }

    // 外壳（五层容器 + 焦点框 + 内容层层级栈）
    bklv::Shell* shell() const { return this->root; }

    // 便捷通知（走 App::notify，落在外壳的 HUD 层）
    void notify(const std::string& text) { this->application.notify(text); }

    // ---- 主题 ----

    // 在可用主题之间轮换（default ↔ glass）。
    void cycleTheme();

    // 显式指定主题；不存在的主题返回 false 并保持原样。
    bool setTheme(const std::string& name);

    const std::string& getThemeName() const { return this->themeName; }

    // ---- 标签条 ----

    // 标签条排列：false = 顶部水平，true = 左侧竖直
    bool isTabVertical() const { return this->tabVertical; }
    void setTabOrientation(bool vertical);

    // 启动时选哪个标签页（0 = 按钮，1 = 布局）
    void setInitialTab(int index, bool vertical);

    // 起窗口并进入帧循环。root 由 App 接管，退出时释放。
    int run(const std::string& title, int width, int height);

  private:
    void buildShell();
    void applyThemeColors();

    bklv::App application;

    bklv::Shell* root        = nullptr;
    bklv::Label* titleLabel  = nullptr;
    bklv::TabBar* tabBar     = nullptr;
    bklv::TabView* tabs      = nullptr;
    bklv::Box* tabLayout     = nullptr;
    bklv::BottomBar* bottomBar = nullptr;

    std::string themeName = "default";
    bool tabVertical      = false;
    int initialTab        = 0;
    bool initialVertical  = false;
};

} // namespace demo

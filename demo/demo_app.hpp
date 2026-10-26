#pragma once

// demo 的外壳：顶栏（标题）+ 标签条 + 标签页内容。
//
// 主体是"tab + 子页面"，五个标签页：
//   1. 按钮 —— 按钮家族、点击反馈、标签条排列
//   2. 布局 —— Box 的轴 / 间距 / 对齐 / 拉伸（彩色矩形看几何）
//   3. 列表 —— 虚拟化长列表 / 虚拟化网格 / 横向滚动 / 点条目下钻新页面
//   4. 容器 —— 对话框 / 列表容器 / 通知三种形态
//   5. 调试 —— 应用内日志 / FPS / 调试边框 / 按页背景 / 安全区 / 玻璃档位
//
// 五个标签页**常驻**（TabView 负责），切走再切回来状态还在；
// 这是"同级切换"，跟内容层下钻（Shell 的 ContentStack）不是一回事 ——
// 「列表」页里点条目走的就是下钻。
//
// 层次：
//   demo/main.cpp          命令行参数、构造 DemoApp、run
//   demo/demo_app.*        外壳 + 标签页装配（本文件）
//   demo/tabs/*.cpp        五个标签页的内容
//   demo/style.hpp         取色与文字规格
//   demo/layout_kit.hpp    "一节"与"彩色矩形"两个排版小工具
//
// 外观归属：系统级外观在 `resources/theme/default/base/`，
// 只有 demo 用到的（list_row / grid_tile / card_wide）在 `resources/theme/default/demo/`。

#include <memory>
#include <string>

#include "bklv/ui/app/app.hpp"

namespace bklv
{
class BottomBar;
class Box;
class DebugLogView;
class FpsOverlay;
class Label;
class Dialog;
class PresetBackdrop;
class RecyclerDataSource;
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
    void notifyImage(const std::string& text);
    void notifyBurst(int count);

    // ---- 列表页用的数据源（生命周期归 DemoApp，RecyclerFrame 只借指针）----
    bklv::RecyclerDataSource* getLongListSource();
    bklv::RecyclerDataSource* getGridSource();

    // 列表页：点条目 → 往内容层压一层详情页
    void openListDetail(size_t index);
    void openGridDetail(size_t index);

    // 容器页：打开示例对话框
    void openDemoDialog();

    // ---- 调试页 ----
    void toggleLogView();
    void toggleFpsOverlay();
    void toggleDebugLayer();
    void cycleBackdrop();
    // 0=none 1=grid 2=floating 3=gradient 4=checker；未知名字返回 false。
    bool setBackdrop(const std::string& name);
    void cycleSafeArea();
    void cycleGlass();
    std::string describeDebugState() const;

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

    // 启动时选哪个标签页（0=按钮 1=布局 2=列表 3=容器 4=调试）
    void setInitialTab(int index, bool vertical);

    // 起窗口并进入帧循环。root 由 App 接管，退出时释放。
    int run(const std::string& title, int width, int height);

  private:
    void buildShell();
    void buildDebugWidgets();
    void applyThemeColors();

    bklv::App application;

    bklv::Shell* root            = nullptr;
    bklv::Label* titleLabel      = nullptr;
    bklv::TabBar* tabBar         = nullptr;
    bklv::TabView* tabs          = nullptr;
    bklv::Box* tabLayout         = nullptr;
    bklv::BottomBar* bottomBar   = nullptr;

    // 调试页的三个视图（日志 / FPS / 按页背景）
    bklv::DebugLogView* logView  = nullptr;
    bklv::FpsOverlay* fpsOverlay = nullptr;
    bklv::PresetBackdrop* backdrop = nullptr;

    // 示例对话框：只建一次，反复 open()/close() 复用（避免每次开都漏一个）
    bklv::Dialog* demoDialog = nullptr;

    // 列表页的数据源（RecyclerFrame 只借指针，这里持有）
    std::unique_ptr<bklv::RecyclerDataSource> longListSource;
    std::unique_ptr<bklv::RecyclerDataSource> gridSource;

    std::string themeName = "default";
    bool tabVertical      = false;
    int initialTab        = 0;
    bool initialVertical  = false;

    int backdropIndex = 0; // 0=NONE 1=GRID 2=FLOATING 3=GRADIENT 4=CHECKER 5=POLYGONS
    int safeAreaIndex = 0; // 0 / 24 / 48
    int glassIndex    = 1; // 0=关 1=BALANCED 2=FULL 3=MINIMAL
};

} // namespace demo

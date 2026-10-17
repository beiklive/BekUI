#pragma once

// 应用入口与帧循环，对应 borealis 的 core/application.cpp 中与本项目相关的那部分。
//
// 与 borealis 的差异：
//   - 只有一个 SDL3 窗口，没有平台抽象层
//   - 没有 Activity 栈、通知、音频、光标等
//   - 整棵 View 树画到 ImGui 的绘制列表上，不创建 ImGui 窗口
//
// 渲染后端：App 本身不碰任何图形 API，全部通过 RenderBackend 接口完成。
// 平台与图形后端都是编译期确定的，App 不做运行时选择。

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/font.hpp"
#include "bklv/core/frame_stats.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/core/platform.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

class NotificationManager;
class DebugLayer;

class App
{
  public:
    App();
    ~App();

    // 设置根视图，App 接管其所有权。
    void setRootView(View* view);

    // 清屏色（0..1）：浅色主题的工具/编辑器宿主需要它；默认深色。必须在 run() 前设置。
    void setClearColor(float r, float g, float b);
    View* getRootView() const { return this->root; }
    Box* getRootBox() const { return this->root != nullptr ? this->root->asBox() : nullptr; }

    // ---- 平台 ----
    // 平台与图形后端都是编译期确定的，App 不做运行时选择。
    // 默认平台：桌面构建是 DESKTOP_SDL3，Switch 构建是 SWITCH（见 core/platform.cpp）。

    void setPlatform(PlatformType type) { this->platformType = type; }
    PlatformType getPlatformType() const { return this->platformType; }

    // 当前编入的图形后端名，日志用。
    static const char* getGraphicsBackendName();

    // 当前帧循环用的图形后端（`run()` 期间有效，退出时置空）。
    // 给需要后端可选能力的宿主用：UI 通道钩子（设计器的玻璃）、
    // getGlassBackend()（库的玻璃服务由 App::run 转交，不需要宿主操心）。
    RenderBackend* getRenderBackend() const { return this->renderBackend; }

    // 创建窗口并进入帧循环，窗口关闭后返回退出码。
    int run(const std::string& title, int width = 1280, int height = 800);

    // 每帧绘制前调用；默认实现是空的。
    std::function<void()> onUpdate;

    // 视图树画完之后调用，用来下发 ImGui 窗口（比如调试窗口、日志窗口）。
    // 必须在 ImGui::Render() 之前，所以单独给一个回调，不混在 onUpdate 里。
    std::function<void()> onDrawWindows;

    // ImGui 的绘制数据已经生成、还没交给后端渲染时的钩子（在 ImGui::Render() 之后、
    // backend->renderDrawData() 之前）。给"要往绘制命令里插东西"的宿主用：
    // 需要额外注入绘制命令的宿主（比如带自己玻璃后端的工具）就是在这里做的：
    // `App::frame` 之后、ImGui::Render() 之前把命令插进 draw data。
    std::function<void(ImDrawData*)> onPrepareDrawData;

    // 触发一次布局重算时用的画布尺寸。
    // invalidate() 从根节点发起重算时需要它：传 Undefined 会让百分比尺寸全部失效。
    static float getLayoutWidth() { return lastLayoutWidth; }
    static float getLayoutHeight() { return lastLayoutHeight; }
    static bool hasLayoutSize() { return lastLayoutWidth > 0.0f && lastLayoutHeight > 0.0f; }

    // 请求下一次 frame 重新布局。
    //
    // 什么时候需要手动调：改的是"布局之外但影响布局的全局状态"，比如运行时切主题
    // 改了主题里的度量值。改视图自己的属性不用调——那些 setter 内部已经 invalidate()
    // 了，会冒泡到根立刻重算一次。
    static void requestLayout();

    // 当前进程里的 App 实例（弹窗这类控件要挂到根视图上，就得找到它）。
    // 由 App 构造时登记、析构时清掉；没有实例时返回 nullptr。
    static App* getCurrent();

    // ---- 弹层（Dialog / Dropdown / applet 面板都用它）----
    //
    // 挂到根视图最上层（绝对定位铺满画布），并把输入层切到模态：
    // 指针只命中弹层内部，方向导航不会跑出弹层，关闭后焦点自动回到打开前的视图。
    // 弹一条通知（右上角浮层，不吃焦点）。没有通知层就懒创建一个。
    // 对应 borealis 的 NotificationManager::notify，但这里是实例而不是全局单例。
    void notify(const std::string& text, float duration = 3.0f);

    // 通知层（想改位置/上限/样式就自己拿它调）
    NotificationManager* getNotifications();

    // 离屏截图：再渲染 delayFrames 帧之后把这一帧读出来存成 PNG，然后退出。
    // 不依赖系统录屏权限（macOS 的 screencapture 没权限时只能拍到壁纸），CI 也能用。
    // 返回 false 表示当前后端不支持帧读回。
    bool requestScreenshot(const std::string& path, int delayFrames = 3);

    // 调试层：把所有视图的边界框出来（对标 borealis 的 debug view / wireframe）
    void setDebugLayerEnabled(bool enabled);
    bool isDebugLayerEnabled() const { return this->debugLayer != nullptr; }
    DebugLayer* getDebugLayer() const { return this->debugLayer; }

    bool pushOverlay(View* overlay, bool closeOnBack = true);
    void popOverlay(View* overlay, bool free = false);

    // 运行时换肤：重新装载主题、让整棵视图树重新套用外观、并重排一次。
    // 已建好的视图会更新（applyTheme），但**文案不会重译**（那是 i18n 的事）。
    bool setTheme(const std::string& name);

    // 每次真正执行 YGNodeCalculateLayout 后由内部调用。用来验证"静态页面不再每帧
    // 白算一次布局"（见 tests 与调试日志面板头上的统计）。
    static void noteLayoutPerformed();
    static unsigned int getLayoutCount();

    // 是否允许 ImGui 自动读写 imgui.ini（保存窗口位置/尺寸/列宽等）。
    // bklv 的绘制全部走 viewport 绘制列表，不依赖 ImGui 窗口布局，所以默认关闭，
    // 避免在运行目录里生成 imgui.ini。需要时再打开，或用 BKLV_USE_INI=1 环境变量。
    void setIniPersistenceEnabled(bool enabled) { this->iniPersistenceEnabled = enabled; }
    bool isIniPersistenceEnabled() const { return this->iniPersistenceEnabled; }

    // ---- 帧预算与帧时间统计（"界面永不卡，任务可以晚"）----

    // 每帧留给**可延后工作**的时间预算（毫秒）：后台任务投回主线程的回调、纹理上传、
    // 动图换帧、SVG 栅格化、纹理缓存淘汰。用完之后这些活自动推迟到下一帧，
    // 布局 / update / 输入 / 绘制**不吃预算**（它们是这一帧必须做的）。
    // 默认 4ms；传 0 表示不限制（回到老行为）。细节见 core/frame_budget.hpp。
    void setFrameBudgetMs(double milliseconds) { this->frameBudgetMs = milliseconds; }
    double getFrameBudgetMs() const { return this->frameBudgetMs; }

    // 最近若干帧的时间样本（整循环迭代，含 present）：p50 / p95 / p99 / 最坏帧 / 掉帧数。
    // 见 core/frame_stats.hpp；调试窗口与验收用它，而不是靠"看着不卡"。
    const FrameStats& getFrameStats() const { return this->frameStats; }
    FrameStats& getFrameStats() { return this->frameStats; }

  private:
    void saveScreenshot(class RenderBackend* backend, float width, float height);

    void frame();

    View* root = nullptr;
    NotificationManager* notifications = nullptr;
    DebugLayer* debugLayer = nullptr;
    // 帧循环里的图形后端（非拥有：由平台持有）。run() 期间有效，退出前置空。
    RenderBackend* renderBackend = nullptr;
    std::string screenshotPath;      // 非空表示要截图
    int screenshotDelay = 0;         // 还要等几帧
    bool quitRequested  = false;
    float clearColor[3] = { 0.10f, 0.10f, 0.12f }; // 默认深色，工具可改
    bool debugLayerRequested = false; // 根视图还没就绪时先记下（setRootView 后再开）
    // 还没有根视图时先排队（比如窗口建好前就 notify），setRootView 之后补发
    std::vector<std::pair<std::string, float>> pendingNotifications;

#if defined(BKLV_SWITCH)
    PlatformType platformType = PlatformType::SWITCH;
#else
    PlatformType platformType = PlatformType::DESKTOP_SDL3;
#endif

    // 上一次布局所用的字体版本号；字体加载完成的那一帧会触发一次全树重算。
    unsigned int lastFontGeneration = 0;
    // 上一帧的画布尺寸：变了就必须重算（窗口 resize / 分辨率变化不经过 invalidate）
    float lastCanvasWidth  = 0.0f;
    float lastCanvasHeight = 0.0f;

    static float lastLayoutWidth;
    static float lastLayoutHeight;
    // 默认关闭：io.IniFilename 置空后 ImGui 不会加载也不会保存 .ini
    bool iniPersistenceEnabled = false;

    // 可延后工作的每帧预算（毫秒，0 = 不限制）与帧时间样本（见 core/frame_budget.hpp）
    double frameBudgetMs = 4.0;
    FrameStats frameStats;
};

// 最简用法：创建窗口，把 root 塞进 App 跑起来。
int RunDemo(const std::string& title, View* root, int width = 1280, int height = 800);

} // namespace bklv

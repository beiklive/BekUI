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
    View* getRootView() const { return this->root; }
    Box* getRootBox() const { return this->root != nullptr ? this->root->asBox() : nullptr; }

    // ---- 平台 ----
    // 平台与图形后端都是编译期确定的，App 不做运行时选择。
    // 默认平台：桌面构建是 DESKTOP_SDL3，Switch 构建是 SWITCH（见 core/platform.cpp）。

    void setPlatform(PlatformType type) { this->platformType = type; }
    PlatformType getPlatformType() const { return this->platformType; }

    // 当前编入的图形后端名，日志用。
    static const char* getGraphicsBackendName();

    // 创建窗口并进入帧循环，窗口关闭后返回退出码。
    int run(const std::string& title, int width = 1280, int height = 800);

    // 每帧绘制前调用；默认实现是空的。
    std::function<void()> onUpdate;

    // 视图树画完之后调用，用来下发 ImGui 窗口（比如调试窗口、日志窗口）。
    // 必须在 ImGui::Render() 之前，所以单独给一个回调，不混在 onUpdate 里。
    std::function<void()> onDrawWindows;

    // ImGui 的绘制数据已经生成、还没交给后端渲染时的钩子（在 ImGui::Render() 之后、
    // backend->renderDrawData() 之前）。给"要往绘制命令里插东西"的宿主用：
    // 设计器的液态玻璃就是在这里把玻璃命令注入 draw data（见 designer/glass/）。
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

  private:
    void saveScreenshot(class RenderBackend* backend, float width, float height);

    void frame();

    View* root = nullptr;
    NotificationManager* notifications = nullptr;
    DebugLayer* debugLayer = nullptr;
    std::string screenshotPath;      // 非空表示要截图
    int screenshotDelay = 0;         // 还要等几帧
    bool quitRequested  = false;
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
};

// 最简用法：创建窗口，把 root 塞进 App 跑起来。
int RunDemo(const std::string& title, View* root, int width = 1280, int height = 800);

} // namespace bklv

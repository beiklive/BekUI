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

    // 触发一次布局重算时用的画布尺寸。
    // invalidate() 从根节点发起重算时需要它：传 Undefined 会让百分比尺寸全部失效。
    static float getLayoutWidth() { return lastLayoutWidth; }
    static float getLayoutHeight() { return lastLayoutHeight; }
    static bool hasLayoutSize() { return lastLayoutWidth > 0.0f && lastLayoutHeight > 0.0f; }

    // 是否允许 ImGui 自动读写 imgui.ini（保存窗口位置/尺寸/列宽等）。
    // bklv 的绘制全部走 viewport 绘制列表，不依赖 ImGui 窗口布局，所以默认关闭，
    // 避免在运行目录里生成 imgui.ini。需要时再打开，或用 BKLV_USE_INI=1 环境变量。
    void setIniPersistenceEnabled(bool enabled) { this->iniPersistenceEnabled = enabled; }
    bool isIniPersistenceEnabled() const { return this->iniPersistenceEnabled; }

  private:
    void frame();

    View* root = nullptr;

#if defined(BKLV_SWITCH)
    PlatformType platformType = PlatformType::SWITCH;
#else
    PlatformType platformType = PlatformType::DESKTOP_SDL3;
#endif

    // 上一次布局所用的字体版本号；字体加载完成的那一帧会触发一次全树重算。
    unsigned int lastFontGeneration = 0;

    static float lastLayoutWidth;
    static float lastLayoutHeight;
    // 默认关闭：io.IniFilename 置空后 ImGui 不会加载也不会保存 .ini
    bool iniPersistenceEnabled = false;
};

// 最简用法：创建窗口，把 root 塞进 App 跑起来。
int RunDemo(const std::string& title, View* root, int width = 1280, int height = 800);

} // namespace bklv

#pragma once

// 平台层，对应 borealis 的 core/platform.hpp。
//
// 职责边界（与 borealis 一致）：
//   Platform       —— 窗口、事件循环、系统能力、各子系统工厂
//   RenderBackend  —— 图形 API：上下文、ImGui 渲染后端、纹理（见 core/graphics.hpp）
//
// 加平台 = 新写一个 Platform 子类 + 一个 RenderBackend 子类，
// 上层的 View / Label / Recycler / Image 一行都不用改。
//
// 目标平台与实现方式：
//   macOS / Linux / Windows —— SDL3（PlatformSDL3）
//   Switch                  —— libnx + EGL/GLES3（SDL3 上游不支持 Switch）
//   Android / iOS           —— 待定（SDL3 支持，但需要各自的工程骨架）

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>

#include "bklv/graphics/render/graphics.hpp"

namespace bklv
{

class InputManager;

enum class PlatformType
{
    DESKTOP_SDL3, // macOS / Linux / Windows
    SWITCH,       // Nintendo Switch (libnx)
};

// 电源/网络等能力的状态。桌面端如实返回，不要假装有。
struct BatteryStatus
{
    bool supported  = false;
    int level       = -1; // 0~100，未知为 -1
    bool charging   = false;
};

struct NetworkStatus
{
    bool wireless    = false;
    int wirelessLevel = -1; // 0~3，未知为 -1
    bool ethernet    = false;
    std::string ipAddress;
};

class Platform
{
  public:
    virtual ~Platform() = default;

    // ---- 基本信息 ----

    virtual PlatformType getType() const = 0;
    virtual const char* getName() const = 0;

    // ---- 窗口 ----

    // 创建窗口。返回 false 表示失败，App 会直接退出。
    // 位置传 NAN 表示交给系统决定（Switch 上无效）。
    virtual bool createWindow(const std::string& title, uint32_t width, uint32_t height, float x = NAN,
        float y = NAN)
        = 0;

    virtual void setWindowSize(uint32_t, uint32_t) { }
    virtual void setWindowSizeLimits(uint32_t, uint32_t, uint32_t, uint32_t) { }
    virtual void setWindowPosition(int, int) { }
    virtual void setWindowAlwaysOnTop(bool) { }

    // 窗口状态变化后（比如 Switch 从手持切到底座）重新读取尺寸。
    virtual void restoreWindow() { }

    // ---- 事件循环 ----

    // 处理一帧的事件。返回值含义与 borealis 一致：
    //   true  = 继续运行
    //   false = 收到退出请求，App 退出循环
    virtual bool mainLoopIteration() = 0;

    // 平台自己的循环包装。桌面端直接跑实现，Switch 需要先做一次 applet 初始化。
    virtual bool runLoop(const std::function<bool()>& runLoopImpl) { return runLoopImpl(); }

    // 关闭平台的 ImGui 后端。必须在 ImGui::DestroyContext() 之前调用。
    virtual void shutdownImGuiPlatformBackend() { }

    // ---- 子系统 ----

    virtual RenderBackend* getGraphicsBackend() = 0;
    virtual InputManager* getInputManager()       = 0;

    // ---- 系统能力 ----
    // 上层先问再调：不支持就不要显示对应 UI。

    virtual bool canShowBatteryLevel() { return false; }
    virtual BatteryStatus getBatteryStatus() { return BatteryStatus(); }

    virtual bool canShowWirelessLevel() { return false; }
    virtual NetworkStatus getNetworkStatus() { return NetworkStatus(); }

    // 息屏抑制（Switch 上很关键：玩的时候不能黑屏）
    virtual bool canDisableScreenDimming() { return false; }
    virtual void setScreenDimmingDisabled(bool /*disable*/, const std::string& /*reason*/ = "") { }
    virtual bool isScreenDimmingDisabled() { return false; }

    // 背光亮度 0~1
    virtual bool canSetBacklightBrightness() { return false; }
    virtual void setBacklightBrightness(float /*brightness*/) { }
    virtual float getBacklightBrightness() { return 1.0f; }

    // 主题跟随系统
    virtual bool isDarkTheme() { return true; }

    // 系统语言，返回 BCP-47 风格（zh-Hans / en-US / ja ...）
    virtual std::string getLocale() { return "en-US"; }

    // 剪贴板
    virtual bool canUseClipboard() { return false; }
    virtual void setClipboardText(const std::string& /*text*/) { }
    virtual std::string getClipboardText() { return ""; }

    // 用系统浏览器打开链接
    virtual bool canOpenBrowser() { return false; }
    virtual void openBrowser(const std::string& /*url*/) { }

    // 是否处于"应用模式"（Switch 上可以退回 HOME 菜单，桌面端无意义）
    virtual bool isApplicationMode() { return true; }
};

// 平台名字，用于日志与调试层。
const char* getPlatformName(PlatformType type);

// 尽早打开"平台侧"的日志通道。
//
// Switch 上没有终端，桌面上的 stdout 又可能没人看，所以这里给平台一个机会把日志
// 落到别处（Switch 上是 sdmc:/bklv.log）。**必须在任何其它初始化之前调用**，
// 否则早期崩溃就什么线索都没有——这个函数存在的唯一原因就是这点。
// 桌面/无特殊通道的平台是空实现。
void enablePlatformLog();

// 创建平台。返回 nullptr 表示当前构建不含该平台。
// 平台与图形后端都是编译期确定的：
//   平台      —— 桌面构建编 sdl3_platform，Switch 构建编 switch_platform
//   图形后端  —— 由平台自己在 createBackend 里选（OpenGL / Vulkan / OpenGL ES 3）
// 所以这里没有"运行时挑一个可用后端"的逻辑。
Platform* createPlatform(PlatformType type);

} // namespace bklv

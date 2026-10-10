#pragma once

// Nintendo Switch 平台实现，基于 libnx + EGL/GLES3。
//
// 为什么不用 SDL3：SDL3 上游没有 Switch 后端（src/video/ 下没有 switch，
// CMakeLists 也没有对应分支），devkitPro 的 portlibs 里只有 SDL2。
// 所以 Switch 上直接走原生：
//   窗口  —— libnx 的 NWindow
//   图形  —— EGL + OpenGL ES 3
//   输入  —— libnx 的 pad（手柄）+ 触摸屏
//   系统  —— libnx 的 applet / 电源 / 网络 API

#include <memory>

#include "bklv/core/platform.hpp"

#if defined(BKLV_SWITCH)

namespace bklv
{

class PlatformSwitch : public Platform
{
  public:
    explicit PlatformSwitch(GraphicsBackendType graphics = GraphicsBackendType::OPENGL);
    ~PlatformSwitch() override;

    PlatformType getType() const override { return PlatformType::SWITCH; }
    const char* getName() const override { return "Switch (libnx)"; }

    // ---- 窗口 ----
    // Switch 没有"创建窗口"这一步：默认的 NWindow 就是全屏。
    // 这里负责设置分辨率与像素格式。
    bool createWindow(const std::string& title, uint32_t width, uint32_t height, float x, float y) override;
    void restoreWindow() override;
    void setWindowSize(uint32_t width, uint32_t height) override;

    // ---- 事件循环 ----
    bool mainLoopIteration() override;
    bool runLoop(const std::function<bool()>& runLoopImpl) override { return runLoopImpl(); }
    void shutdownImGuiPlatformBackend() override;

    // ---- 子系统 ----
    RenderBackend* getGraphicsBackend() override;
    InputManager* getInputManager() override;

    // ---- 系统能力 ----
    bool canShowBatteryLevel() override { return true; }
    BatteryStatus getBatteryStatus() override;

    bool canShowWirelessLevel() override { return true; }
    NetworkStatus getNetworkStatus() override;

    bool canDisableScreenDimming() override { return true; }
    void disableScreenDimming(bool disable, const std::string& reason = "") override;
    bool isScreenDimmingDisabled() override { return this->dimmingDisabled; }

    bool canSetBacklightBrightness() override { return true; }
    void setBacklightBrightness(float brightness) override;
    float getBacklightBrightness() override;

    bool isDarkTheme() override;
    std::string getLocale() override;

    bool canOpenBrowser() override { return true; }
    void openBrowser(const std::string& url) override;

    // Switch 上可以退回 HOME 菜单
    bool isApplicationMode() override { return true; }

  private:
    GraphicsBackendType graphicsType = GraphicsBackendType::OPENGL;
    bool dimmingDisabled             = false;

    // 图形上下文相关（定义在 cpp 里，避免头文件包含 EGL）
    struct Impl;
    Impl* impl = nullptr;
};

} // namespace bklv

#endif // BKLV_SWITCH

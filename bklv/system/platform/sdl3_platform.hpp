#pragma once

// macOS / Linux / Windows 的平台实现，基于 SDL3。
//
// 它持有一个 GraphicsBackend（默认 OpenGL），负责：
//   - SDL 窗口创建与销毁
//   - 窗口事件（含 ImGui 平台后端的初始化与事件转发）
//   - 事件循环的一帧
//   - SDL3 手柄接入 InputManager
//   - 桌面端能查到的系统信息（剪贴板、主题、语言、开浏览器）

#include <SDL3/SDL.h>

#include <memory>

// Switch 用 switch_platform / switch_gamepad，不含 SDL3 实现。
#if !defined(BKLV_SWITCH)

#include "bklv/foundation/core/platform.hpp"
#include "bklv/system/platform/sdl3_gamepad.hpp"

namespace bklv
{

class PlatformSDL3 : public Platform
{
  public:
    PlatformSDL3();
    ~PlatformSDL3() override;

    PlatformType getType() const override { return PlatformType::DESKTOP_SDL3; }
    const char* getName() const override { return "SDL3"; }

    // ---- 窗口 ----
    bool createWindow(const std::string& title, uint32_t width, uint32_t height, float x, float y) override;
    void setWindowSize(uint32_t width, uint32_t height) override;
    void setWindowSizeLimits(uint32_t minWidth, uint32_t minHeight, uint32_t maxWidth,
        uint32_t maxHeight) override;
    void setWindowPosition(int x, int y) override;
    void setWindowAlwaysOnTop(bool enable) override;

    // ---- 事件循环 ----
    bool mainLoopIteration() override;
    void shutdownImGuiPlatformBackend() override;

    // ---- 子系统 ----
    RenderBackend* getGraphicsBackend() override { return this->graphicsBackend.get(); }
    InputManager* getInputManager() override;

    // ---- 系统能力 ----
    bool isDarkTheme() override;
    std::string getLocale() override;

    bool canUseClipboard() override { return true; }
    void setClipboardText(const std::string& text) override;
    std::string getClipboardText() override;

    bool canOpenBrowser() override { return true; }
    void openBrowser(const std::string& url) override;

    bool isApplicationMode() override { return true; }

  private:
    // 创建图形后端。编译期决定：默认 OpenGL，-DBKLV_VULKAN 时 Vulkan。
    void createBackend();

    SDL_Window* window  = nullptr;
    bool sdlReady       = false;
    std::unique_ptr<RenderBackend> graphicsBackend;
    std::unique_ptr<SDL3GamepadSource> gamepadSource;
};

} // namespace bklv

#endif // !BKLV_SWITCH

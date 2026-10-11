#include "bklv/platforms/sdl3_platform.hpp"

#if !defined(BKLV_SWITCH)

#include <cstdlib>

#include "backends/imgui_impl_sdl3.h"
#include "imgui.h"

#include <memory>

#include "bklv/backends/opengl_backend.hpp"
#include "bklv/backends/vulkan_backend.hpp"
#include "bklv/core/gamepad.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"

namespace bklv
{

PlatformSDL3::PlatformSDL3()
{
    // 这里只准备图形后端。
    // SDL_Init 不能放在构造函数里：平台对象是在 ImGui::CreateContext() 之前创建的，
    // 而 SDL 的初始化和后续的 ImGui 平台后端初始化都必须在那之后。
    this->createBackend();
}

void PlatformSDL3::createBackend()
{
#if defined(BKLV_VULKAN)
    this->graphicsBackend = std::make_unique<VulkanBackend>();
#else
    this->graphicsBackend = std::make_unique<OpenGLBackend>();
#endif

    Logger::info(std::string("图形后端：") + this->graphicsBackend->getName());
}

// ---------------------------------------------------------------------------
// 窗口
// ---------------------------------------------------------------------------

bool PlatformSDL3::createWindow(const std::string& title, uint32_t width, uint32_t height, float x, float y)
{
    if (this->graphicsBackend == nullptr)
    {
        Logger::error("图形后端未创建");
        return false;
    }

    if (!this->sdlReady)
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        {
            Logger::error(std::string("SDL 初始化失败：") + SDL_GetError());
            return false;
        }
        this->sdlReady = true;
    }

    // 窗口标记由图形后端决定：OpenGL 要 SDL_WINDOW_OPENGL，Vulkan 要 SDL_WINDOW_VULKAN。
    const SDL_WindowFlags flags =
        this->graphicsBackend->getWindowFlags() | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    this->window = SDL_CreateWindow(title.c_str(), (int)width, (int)height, flags);
    if (this->window == nullptr)
    {
        Logger::error(std::string("创建窗口失败：") + SDL_GetError());
        return false;
    }

    if (!std::isnan(x) && !std::isnan(y))
        SDL_SetWindowPosition(this->window, (int)x, (int)y);

    // 图形上下文（GL context / Vulkan instance + swapchain）
    if (!this->graphicsBackend->initContext((void*)this->window))
    {
        Logger::error("图形上下文初始化失败");
        SDL_DestroyWindow(this->window);
        this->window = nullptr;
        return false;
    }

    // ImGui 的平台后端：键鼠与窗口事件。与图形 API 无关。
    ImGui_ImplSDL3_InitForOther(this->window);

    // 手柄是可选的，没有手柄时键鼠照常工作。
    this->gamepadSource = std::make_unique<SDL3GamepadSource>();
    setGamepadSource(this->gamepadSource.get());

    return true;
}

void PlatformSDL3::setWindowSize(uint32_t width, uint32_t height)
{
    if (this->window != nullptr)
        SDL_SetWindowSize(this->window, (int)width, (int)height);
}

void PlatformSDL3::setWindowSizeLimits(uint32_t minWidth, uint32_t minHeight, uint32_t /*maxWidth*/,
    uint32_t /*maxHeight*/)
{
    // SDL3 没有单独的上限设置接口，只有最小尺寸。
    if (this->window != nullptr)
        SDL_SetWindowMinimumSize(this->window, (int)minWidth, (int)minHeight);
}

void PlatformSDL3::setWindowPosition(int x, int y)
{
    if (this->window != nullptr)
        SDL_SetWindowPosition(this->window, x, y);
}

void PlatformSDL3::setWindowAlwaysOnTop(bool enable)
{
    if (this->window != nullptr)
        SDL_SetWindowAlwaysOnTop(this->window, enable);
}

// ---------------------------------------------------------------------------
// 事件循环
// ---------------------------------------------------------------------------

bool PlatformSDL3::mainLoopIteration()
{
    // ImGui 平台后端的每帧准备（读键鼠状态）。图形后端的那一半由 App 调用。
    ImGui_ImplSDL3_NewFrame();

    bool running = true;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);

        if (event.type == SDL_EVENT_QUIT)
            running = false;

        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && this->window != nullptr
            && event.window.windowID == SDL_GetWindowID(this->window))
            running = false;
    }

    return running;
}

PlatformSDL3::~PlatformSDL3()
{
    // 先摘掉手柄源，避免 InputManager 持有已经析构的对象。
    setGamepadSource(nullptr);
    this->gamepadSource.reset();

    if (this->window != nullptr)
    {
        SDL_DestroyWindow(this->window);
        this->window = nullptr;
    }

    if (this->sdlReady)
    {
        SDL_Quit();
        this->sdlReady = false;
    }
}

void PlatformSDL3::shutdownImGuiPlatformBackend()
{
    if (this->window != nullptr && ImGui::GetCurrentContext() != nullptr)
        ImGui_ImplSDL3_Shutdown();
}

InputManager* PlatformSDL3::getInputManager()
{
    // 全局限量限定，避免和本方法同名导致递归。
    return &bklv::getInputManager();
}

// ---------------------------------------------------------------------------
// 系统能力
// ---------------------------------------------------------------------------

bool PlatformSDL3::isDarkTheme()
{
    // SDL 3.2+ 提供了系统主题查询；拿不到时按深色处理（本项目的默认主题就是深色）。
    SDL_SystemTheme theme = SDL_GetSystemTheme();
    return theme != SDL_SYSTEM_THEME_LIGHT;
}

std::string PlatformSDL3::getLocale()
{
    // 返回的是 SDL_Locale**，需要自己释放。
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    if (locales == nullptr || count == 0 || locales[0]->language == nullptr)
    {
        if (locales != nullptr)
            SDL_free(locales);
        return "en-US";
    }

    std::string locale = locales[0]->language;
    if (locales[0]->country != nullptr)
    {
        locale += "-";
        locale += locales[0]->country;
    }

    SDL_free(locales);
    return locale;
}

void PlatformSDL3::setClipboardText(const std::string& text)
{
    SDL_SetClipboardText(text.c_str());
}

std::string PlatformSDL3::getClipboardText()
{
    char* text = SDL_GetClipboardText();
    if (text == nullptr)
        return "";

    std::string result = text;
    SDL_free(text);
    return result;
}

void PlatformSDL3::openBrowser(const std::string& url)
{
    SDL_OpenURL(url.c_str());
}

} // namespace bklv

#endif // !BKLV_SWITCH

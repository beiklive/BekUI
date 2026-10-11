#include "bklv/backends/imgui_impl_libnx.hpp"

#if defined(BKLV_SWITCH)

#include <switch.h>

#include "imgui.h"

#include "bklv/core/logger.hpp"

namespace bklv
{

namespace
{

// 屏幕尺寸不写死：从默认 NWindow 查，和平台层/渲染后端保持同一个来源。
// Switch 手持与底座都是 1280x720，但写死三份容易在改动时对不上。
void getScreenSize(float& width, float& height)
{
    u32 w = 0, h = 0;
    nwindowGetDimensions(nwindowGetDefault(), &w, &h);
    width  = w > 0 ? (float)w : 1280.0f;
    height = h > 0 ? (float)h : 720.0f;
}

PadState g_pad {};
HidTouchScreenState g_touchState {};
u64 g_time = 0;
u64 g_lastTime = 0;
bool g_initialized = false;

// 把 Switch 的按键位映射到 ImGuiKey。只在状态变化时发事件。
void keyDown(ImGuiKey key, bool down, bool& last)
{
    if (down != last)
    {
        ImGui::GetIO().AddKeyEvent(key, down);
        last = down;
    }
}

} // namespace

bool ImGui_ImplLibnx_Init()
{
    if (g_initialized)
        return true;

    // 手柄初始化（跟 GamepadSource 用的是两套状态，互不影响：
    // 这里只负责把按键翻译成 ImGui 的键盘事件）。
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);

    // 触摸屏
    hidInitializeTouchScreen();

    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "bklv_libnx";

    float screenWidth = 0.0f, screenHeight = 0.0f;
    getScreenSize(screenWidth, screenHeight);
    io.DisplaySize = ImVec2(screenWidth, screenHeight);

    g_time        = armGetSystemTick();
    g_lastTime    = g_time;
    g_initialized = true;

    Logger::info("ImGui libnx 平台后端已初始化");
    return true;
}

void ImGui_ImplLibnx_Shutdown()
{
    g_initialized = false;
}

void ImGui_ImplLibnx_NewFrame()
{
    if (!g_initialized)
        return;

    // 时间：用 ARM 的系统 tick 换成秒。
    constexpr double TICK_TO_SEC = 1.0 / 19200000.0; // armGetSystemTickFreq 是 19.2MHz
    g_time                        = armGetSystemTick();
    const float deltaTime         = (float)((double)(g_time - g_lastTime) * TICK_TO_SEC);
    g_lastTime                    = g_time;

    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = deltaTime > 0.0f ? deltaTime : (1.0f / 60.0f);

    // 每帧刷新：手持/底座切换后尺寸可能变。
    float screenWidth = 0.0f, screenHeight = 0.0f;
    getScreenSize(screenWidth, screenHeight);
    io.DisplaySize = ImVec2(screenWidth, screenHeight);

    padUpdate(&g_pad);
    const u64 buttons = padGetButtons(&g_pad);

    // 每个按键维护一个"上一帧状态"，只在变化时发事件。
    static bool stUp = false, stDown = false, stLeft = false, stRight = false;
    static bool stA = false, stB = false, stX = false, stY = false;
    static bool stPlus = false, stMinus = false, stL = false, stR = false;

    keyDown(ImGuiKey_UpArrow, (buttons & (HidNpadButton_Up | HidNpadButton_StickLUp)) != 0, stUp);
    keyDown(ImGuiKey_DownArrow, (buttons & (HidNpadButton_Down | HidNpadButton_StickLDown)) != 0, stDown);
    keyDown(ImGuiKey_LeftArrow, (buttons & (HidNpadButton_Left | HidNpadButton_StickLLeft)) != 0, stLeft);
    keyDown(ImGuiKey_RightArrow, (buttons & (HidNpadButton_Right | HidNpadButton_StickLRight)) != 0, stRight);

    // A 当回车（确认），B 当 Esc（返回），跟 core/input.cpp 里的键盘映射对上。
    keyDown(ImGuiKey_Enter, (buttons & HidNpadButton_A) != 0, stA);
    keyDown(ImGuiKey_Escape, (buttons & HidNpadButton_B) != 0, stB);
    keyDown(ImGuiKey_X, (buttons & HidNpadButton_X) != 0, stX);
    keyDown(ImGuiKey_Y, (buttons & HidNpadButton_Y) != 0, stY);
    keyDown(ImGuiKey_Enter, (buttons & HidNpadButton_Plus) != 0, stPlus);
    keyDown(ImGuiKey_Escape, (buttons & HidNpadButton_Minus) != 0, stMinus);
    keyDown(ImGuiKey_LeftBracket, (buttons & HidNpadButton_L) != 0, stL);
    keyDown(ImGuiKey_RightBracket, (buttons & HidNpadButton_R) != 0, stR);

    // 触摸屏当作鼠标：有触摸时设置指针位置并按下左键。
    hidGetTouchScreenStates(&g_touchState, 1);

    if (g_touchState.count > 0)
    {
        const HidTouchState& touch = g_touchState.touches[0];
        io.AddMousePosEvent((float)touch.x, (float)touch.y);
        io.AddMouseButtonEvent(0, true);
    }
    else
    {
        io.AddMouseButtonEvent(0, false);
    }
}

} // namespace bklv

#endif // BKLV_SWITCH

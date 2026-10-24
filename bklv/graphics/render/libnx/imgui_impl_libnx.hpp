#pragma once

// ImGui 的 libnx 平台后端。
//
// 为什么需要自己写：ImGui 只有 SDL2/SDL3 的平台后端，没有 libnx 的。
// 这个文件负责把 Switch 的按键与触摸喂进 ImGuiIO，
// 这样 core/input.cpp 里读 ImGuiIO 的那套键盘逻辑在 Switch 上不用改就能用。
//
// 只需要实现 ImGui 平台后端契约的四个函数，图形部分由 ImGui 的 OpenGL3 后端负责。

#if defined(BKLV_SWITCH)

struct ImGuiIO;

namespace bklv
{

bool ImGui_ImplLibnx_Init();
void ImGui_ImplLibnx_Shutdown();
void ImGui_ImplLibnx_NewFrame();

} // namespace bklv

#endif // BKLV_SWITCH

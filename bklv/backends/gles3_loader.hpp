#pragma once

// OpenGL ES 3 加载器，给 Switch 用。
//
// 桌面端 ImGui 自带 imgui_impl_opengl3_loader.h；Switch 上我们直接包含 devkitPro
// 提供的 GLES3 头（portlibs/switch/include/GLES3/gl3.h），它已经把需要的函数都声明了，
// 不需要额外加载器。配合 -DIMGUI_IMPL_OPENGL_LOADER_CUSTOM=<这个头> 使用。
//
// 只在 BKLV_GLES3 下编译。

#if defined(BKLV_GLES3)

#include <GLES3/gl3.h>

// GLES3 里 glClearDepth 改名为 glClearDepthf，ImGui 用不到，忽略即可。

#endif // BKLV_GLES3

#pragma once

// 桌面 OpenGL 的跨平台加载方式。
//
// macOS：系统提供 OpenGL.framework，<OpenGL/gl3.h> 直接给出 GL 3.2 Core 的声明，
//        链接 -framework OpenGL 即可，不需要任何加载器。
// Linux：libGL 的 <GL/glcorearb.h> 提供 GL 3+ 声明；定义 GL_GLEXT_PROTOTYPES
//        就会导出原型，链接 -lGL 即可，同样不需要生成式加载器（glad/gl3w）。
//
// Windows：opengl32.dll 只导出 GL 1.1，GL 3.2 Core 的函数（glCreateShader、
//        glGenVertexArrays、glBindSampler...）必须运行时取地址。这里复用 ImGui
//        自带的 imgl3w（3rd/imgui/backends/imgui_impl_opengl3_loader.h）：
//        它随仓库、跨平台，并且已经把 gl* 名字重定向到函数指针。
//        它只为 imgui 自己用到的符号而生，少两个东西要我们补，见下面注释。
//
// 三个平台都不引入额外的 loader 依赖或生成步骤。
// ImGui 的 OpenGL3 后端也复用这个头：通过 -DIMGUI_IMPL_OPENGL_LOADER_CUSTOM

#if defined(__APPLE__)

#include <OpenGL/gl3.h>

#elif defined(_WIN32)

// imgl3w 是第三方生成代码，在 -Wextra 下会报 cast-function-type
// （GetProcAddress 的 FARPROC 强转）。子模块不能改，所以只在这里把该警告压掉，
// 项目其余代码仍然保持零警告。
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

#include "backends/imgui_impl_opengl3_loader.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

// 补丁一：imgl3w 只挑 imgui 用到的常量，GL_NO_ERROR 不在其中。
// 这个值来自 Khronos registry，glcorearb.h 与 GL ES 头里都是 0。
#ifndef GL_NO_ERROR
#define GL_NO_ERROR 0
#endif

// 补丁二：MinGW 的 wingdi.h 里有 `#define ERROR 0`，会把 bklv 的
// `LogLevel::ERROR` 直接替换成 `LogLevel::0`。日志级别用枚举表达更清楚，
// 所以这里把宏去掉。windows.h 一被包含就会带上它，去完即止，不影响别的符号。
#ifdef ERROR
#undef ERROR
#endif

namespace bklv
{

// 填充 imgl3w 的函数指针表。必须在 GL 上下文创建并 MakeCurrent 之后调用，
// 且在首次使用任何 gl* 函数之前（包括 ImGui_ImplOpenGL3_Init）。
// 返回 false 表示加载失败，调用方应放弃初始化而不是带着空指针继续跑。
bool loadGLFunctions();

} // namespace bklv

#else

// glcorearb.h 需要自己声明 APIENTRY/APIENTRYP
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY*
#endif
#ifndef GLAPI
#define GLAPI extern
#endif

#define GL_GLEXT_PROTOTYPES 1
#include <GL/glcorearb.h>

namespace bklv
{

// 原型平台不需要运行时加载，保留同名函数让调用点保持平台无关。
inline bool loadGLFunctions()
{
    return true;
}

} // namespace bklv

#endif

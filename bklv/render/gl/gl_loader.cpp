// Windows 的 GL 运行时加载实现。
//
// imgl3w 是头文件里的 loader，本体要靠定义 IMGL3W_IMPL 在某个编译单元里展开；
// 这里就是那个编译单元。其他平台只用到 gl_loader.hpp 里的 inline 空实现，
// 所以整个文件在非 Windows 上不产生任何符号。

// Windows 上无论是哪个图形后端都要编：opengl_backend.cpp 与 ImGui 的 OpenGL3
// 后端都在库内部，它们无条件调用 loadGLFunctions()，符号必须始终存在。
#if defined(_WIN32) && !defined(BKLV_GLES3) && !defined(BKLV_SWITCH)

// imgl3w 的实现段用了 sscanf 解析 GL_VERSION，但它自己没带 <cstdio>。
#include <cstdio>

#define IMGL3W_IMPL
#include "bklv/render/gl/gl_loader.hpp"

#include <SDL3/SDL.h>

namespace bklv
{

bool loadGLFunctions()
{
    // 表已填好就直接复用：SDL_GL 上下文可能被重建，重复加载没有副作用。
    if (glGetIntegerv == nullptr)
    {
        // imgl3w 默认走 wglGetProcAddress，这里换成 SDL 的实现，
        // 免得和 SDL 自己管理的 GL 上下文/DLL 生命周期分叉。
        if (imgl3wInit2(reinterpret_cast<GL3WGetProcAddressProc>(SDL_GL_GetProcAddress)) != GL3W_OK)
            return false;
    }

    return glGetIntegerv != nullptr;
}

} // namespace bklv

#endif // _WIN32 && !BKLV_GLES3 && !BKLV_SWITCH

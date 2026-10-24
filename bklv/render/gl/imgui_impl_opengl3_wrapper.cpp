// ImGui 的 OpenGL3 后端编译单元。
//
// ImGui 1.93 里 IMGUI_IMPL_OPENGL_LOADER_CUSTOM 只表示"不要加载它自带的 loader"，
// 并不会帮我们包含自定义头。所以这里先引入我们的 GL 头，再包含 ImGui 的实现文件。
//
// 这样桌面（macOS framework / Linux libGL）与 Switch（GLES3）都用同一个约定：
//   -DBKLV_GLES3            -> 走 GLES3/gl3.h
//   否则                     -> 走 bklv/render/gl/gl_loader.hpp

#if defined(BKLV_GLES3)
#include "bklv/render/gles3/gles3_loader.hpp"
#else
#include "bklv/render/gl/gl_loader.hpp"
#endif

#include "backends/imgui_impl_opengl3.cpp"

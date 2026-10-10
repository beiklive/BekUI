#pragma once

// 桌面 OpenGL 的跨平台加载方式。
//
// macOS：系统提供 OpenGL.framework，<OpenGL/gl3.h> 直接给出 GL 3.2 Core 的声明，
//        链接 -framework OpenGL 即可，不需要任何加载器。
// Linux：libGL 的 <GL/glcorearb.h> 提供 GL 3+ 声明；定义 GL_GLEXT_PROTOTYPES
//        就会导出原型，链接 -lGL 即可，同样不需要生成式加载器（glad/gl3w）。
//
// 这样两个平台都不引入额外的 loader 依赖或生成步骤。
// ImGui 的 OpenGL3 后端也复用这个头：通过 -DIMGUI_IMPL_OPENGL_LOADER_CUSTOM=<bklv/backends/gl_loader.hpp>

#if defined(__APPLE__)

#include <OpenGL/gl3.h>

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

#endif

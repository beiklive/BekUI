#pragma once

// 设计器玻璃用到的桌面 GL 入口。
//
// 为什么不直接用 bklv 的那套：bklv 用的是 ImGui 自带的**精简** loader
// （imgui_impl_opengl3_loader.h），只覆盖 ImGui 自己要用的入口 —— 没有 FBO / blit /
// VAO / uniform 数组这些。玻璃要的入口更多，所以在设计器里自己加载一套：
//   · 跨平台：一律走 SDL_GL_GetProcAddress（Windows 的 opengl32.dll 只有 GL 1.1）
//   · 不和 ImGui 的 loader 混：**这个头只在设计器的 GL 编译单元里用**，
//     不 include bklv/bklv.hpp，避免 typedef / 枚举冲突
//
// 用法：`GlProcs& gl = glProcs(); gl.genFramebuffers(1, &fb);`

#include <cstddef>

// ---- GL 类型与常量（只定义用到的那部分；带 #ifndef 防冲突）----
#ifndef GL_TEXTURE_2D
// ES3（Switch）直接用 <GLES3/gl3.h> 的类型与枚举，这里不再重复定义
#if !defined(BKLV_GLES3)
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef char GLchar;
typedef unsigned int GLbitfield;

#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_RGBA 0x1908
#endif // !BKLV_GLES3

#define GL_RGBA8 0x8058
#define GL_UNSIGNED_BYTE 0x1401
#define GL_BACK 0x0405
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_TRUE 1
#define GL_FALSE 0
#define GL_TRIANGLES 0x0004
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_SCISSOR_TEST 0x0C11
#define GL_BLEND 0x0BE2
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_FUNC_ADD 0x8006
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_NO_ERROR 0
#endif

namespace bklv::glass
{

// 需要的一组 GL 入口。X(返回类型, 名字, 参数表)
#define BKLV_DESIGNER_GL_FUNCTIONS(X)                                              \
    X(void, GenTextures, (GLsizei, GLuint*))                                       \
    X(void, DeleteTextures, (GLsizei, const GLuint*))                              \
    X(void, BindTexture, (GLenum, GLuint))                                         \
    X(void, TexParameteri, (GLenum, GLenum, GLint))                                \
    X(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
    X(void, ActiveTexture, (GLenum))                                               \
    X(void, GenFramebuffers, (GLsizei, GLuint*))                                   \
    X(void, DeleteFramebuffers, (GLsizei, const GLuint*))                          \
    X(void, BindFramebuffer, (GLenum, GLuint))                                     \
    X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))         \
    X(GLenum, CheckFramebufferStatus, (GLenum))                                    \
    X(void, ReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*))  \
    X(void, BlitFramebuffer, (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
    X(void, ReadBuffer, (GLenum))                                                  \
    X(void, GenVertexArrays, (GLsizei, GLuint*))                                   \
    X(void, DeleteVertexArrays, (GLsizei, const GLuint*))                          \
    X(void, BindVertexArray, (GLuint))                                             \
    X(GLenum, GetError, ())                                                        \
    X(void, GetIntegerv, (GLenum, GLint*))                                         \
    X(void, Viewport, (GLint, GLint, GLsizei, GLsizei))                            \
    X(void, Scissor, (GLint, GLint, GLsizei, GLsizei))                             \
    X(void, Enable, (GLenum))                                                      \
    X(void, Disable, (GLenum))                                                     \
    X(void, ColorMask, (GLboolean, GLboolean, GLboolean, GLboolean))               \
    X(void, BlendEquation, (GLenum))                                               \
    X(void, BlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum))                   \
    X(void, UseProgram, (GLuint))                                                  \
    X(GLuint, CreateShader, (GLenum))                                              \
    X(void, ShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*))    \
    X(void, CompileShader, (GLuint))                                               \
    X(void, GetShaderiv, (GLuint, GLenum, GLint*))                                 \
    X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))                \
    X(void, DeleteShader, (GLuint))                                                \
    X(GLuint, CreateProgram, ())                                                   \
    X(void, AttachShader, (GLuint, GLuint))                                        \
    X(void, LinkProgram, (GLuint))                                                 \
    X(void, DeleteProgram, (GLuint))                                               \
    X(void, GetProgramiv, (GLuint, GLenum, GLint*))                                \
    X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))               \
    X(GLint, GetUniformLocation, (GLuint, const GLchar*))                          \
    X(void, Uniform1i, (GLint, GLint))                                             \
    X(void, Uniform1f, (GLint, GLfloat))                                           \
    X(void, Uniform2f, (GLint, GLfloat, GLfloat))                                  \
    X(void, Uniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))                \
    X(void, Uniform1fv, (GLint, GLsizei, const GLfloat*))                         \
    X(void, Uniform4fv, (GLint, GLsizei, const GLfloat*))                          \
    X(void, DrawArrays, (GLenum, GLint, GLsizei))

struct GlProcs
{
#define BKLV_DESIGNER_DECLARE(ret, name, args) ret(*name) args = nullptr;
    BKLV_DESIGNER_GL_FUNCTIONS(BKLV_DESIGNER_DECLARE)
#undef BKLV_DESIGNER_DECLARE
};

// 懒加载一次；失败时返回 false（玻璃整体不可用，设计器回退成普通面板）
bool loadGlProcs();
GlProcs& glProcs();
bool isGlProcsReady();

} // namespace bklv::glass

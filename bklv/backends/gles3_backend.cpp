#include "bklv/backends/gles3_backend.hpp"

#if defined(BKLV_SWITCH)

// switch.h 必须在 EGL 之前：Switch 的 eglplatform.h 靠它的类型定义原生窗口。
#include <switch.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include "backends/imgui_impl_opengl3.h"

#include "bklv/core/logger.hpp"

namespace bklv
{

struct GLES3Backend::Impl
{
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;

    int width  = 1280;
    int height = 720;
};

GLES3Backend::GLES3Backend()
{
    this->impl = new Impl();
}

GLES3Backend::~GLES3Backend()
{
    if (this->impl != nullptr)
    {
        if (this->impl->display != EGL_NO_DISPLAY)
        {
            eglMakeCurrent(this->impl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (this->impl->context != EGL_NO_CONTEXT)
                eglDestroyContext(this->impl->display, this->impl->context);
            if (this->impl->surface != EGL_NO_SURFACE)
                eglDestroySurface(this->impl->display, this->impl->surface);

            eglTerminate(this->impl->display);
        }

        delete this->impl;
        this->impl = nullptr;
    }

    this->ready = false;
}

bool GLES3Backend::initContext(void* /*nativeWindow*/)
{
    // Switch 的"窗口"是默认的 NWindow，全屏，不需要创建。
    NWindow* window = nwindowGetDefault();

    // 双缓冲；层数要跟像素格式一起设置。
    nwindowSetDimensions(window, this->impl->width, this->impl->height);

    this->impl->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (this->impl->display == EGL_NO_DISPLAY)
    {
        Logger::error("Switch: eglGetDisplay 失败");
        return false;
    }

    if (!eglInitialize(this->impl->display, nullptr, nullptr))
    {
        Logger::error("Switch: eglInitialize 失败");
        return false;
    }

    // 用 OpenGL ES 这一套 API（不是桌面 EGL_OPENGL_API）。
    eglBindAPI(EGL_OPENGL_ES_API);

    const EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE,
    };

    EGLConfig config;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(this->impl->display, configAttributes, &config, 1, &numConfigs) || numConfigs == 0)
    {
        Logger::error("Switch: eglChooseConfig 没有可用配置");
        return false;
    }

    // 把 NWindow 交给 EGL 当原生窗口。
    this->impl->surface =
        eglCreateWindowSurface(this->impl->display, config, (EGLNativeWindowType)window, nullptr);
    if (this->impl->surface == EGL_NO_SURFACE)
    {
        Logger::error("Switch: eglCreateWindowSurface 失败");
        return false;
    }

    const EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE,
    };

    this->impl->context = eglCreateContext(this->impl->display, config, EGL_NO_CONTEXT, contextAttributes);
    if (this->impl->context == EGL_NO_CONTEXT)
    {
        Logger::error("Switch: eglCreateContext 失败");
        return false;
    }

    if (!eglMakeCurrent(this->impl->display, this->impl->surface, this->impl->surface, this->impl->context))
    {
        Logger::error("Switch: eglMakeCurrent 失败");
        return false;
    }

    // 开垂直同步，避免把 GPU 跑满导致降频。
    eglSwapInterval(this->impl->display, 1);

    this->ready = true;
    Logger::info("Switch: EGL + OpenGL ES 3 上下文已就绪");
    return true;
}

bool GLES3Backend::initImGuiRenderer()
{
    if (!this->ready)
        return false;

    // GLES3 用 "#version 300 es"。
    return ImGui_ImplOpenGL3_Init("#version 300 es");
}

void GLES3Backend::shutdownImGuiRenderer()
{
    if (this->ready)
        ImGui_ImplOpenGL3_Shutdown();

    this->ready = false;
}

void GLES3Backend::newFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
}

void GLES3Backend::clear(const float rgba[4])
{
    glViewport(0, 0, this->impl->width, this->impl->height);
    glClearColor(rgba[0], rgba[1], rgba[2], rgba[3]);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GLES3Backend::renderDrawData(ImDrawData* drawData)
{
    ImGui_ImplOpenGL3_RenderDrawData(drawData);
}

void GLES3Backend::present()
{
    if (this->impl->display != EGL_NO_DISPLAY)
        eglSwapBuffers(this->impl->display, this->impl->surface);
}

TextureHandle GLES3Backend::createTexture(const unsigned char* pixels, int width, int height)
{
    TextureHandle handle;

    if (!this->ready || pixels == nullptr || width <= 0 || height <= 0)
        return handle;

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (glGetError() != GL_NO_ERROR)
    {
        Logger::error("Switch: 纹理上传失败");
        glDeleteTextures(1, &texture);
        return handle;
    }

    handle.texture = (ImTextureID)(intptr_t)texture;
    handle.width   = width;
    handle.height  = height;
    return handle;
}

void GLES3Backend::destroyTexture(TextureHandle& handle)
{
    if (handle.texture == ImTextureID_Invalid)
        return;

    if (this->ready)
    {
        GLuint texture = (GLuint)(intptr_t)handle.texture;
        glDeleteTextures(1, &texture);
    }

    handle = TextureHandle();
}

} // namespace bklv

#endif // BKLV_SWITCH

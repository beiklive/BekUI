#include "bklv/backends/opengl_backend.hpp"

#if !defined(BKLV_SWITCH)

#include <SDL3/SDL.h>

#include "bklv/backends/gl_loader.hpp"

#include "backends/imgui_impl_opengl3.h"

#include "bklv/core/logger.hpp"

namespace bklv
{

OpenGLBackend::~OpenGLBackend()
{
    // 玻璃后端持有 GL 资源（程序、纹理、FBO）：必须在上下文还活着时先释放
    this->glassBackend.reset();

    // 正常路径下 App 已经调过 shutdownImGuiRenderer，
    // 这里只兜底销毁上下文，避免异常退出时泄漏。
    if (this->glContext != nullptr)
    {
        SDL_GL_DestroyContext(this->glContext);
        this->glContext = nullptr;
    }
}

glass::GlassBackend* OpenGLBackend::getGlassBackend()
{
    if (this->glassBackend == nullptr)
        this->glassBackend = std::make_unique<glass::GlGlassBackend>();
    return this->glassBackend.get();
}

unsigned long long OpenGLBackend::getWindowFlags() const
{
    return SDL_WINDOW_OPENGL;
}

bool OpenGLBackend::initContext(void* nativeWindow)
{
    if (nativeWindow == nullptr)
        return false;

    SDL_Window* target = static_cast<SDL_Window*>(nativeWindow);
    this->window = target;

    // macOS 上要显式请求 Core Profile 3.2，否则拿到的可能是 2.1 兼容上下文。
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    this->glContext = SDL_GL_CreateContext(target);
    if (this->glContext == nullptr)
    {
        Logger::error(std::string("OpenGL 上下文创建失败：") + SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(target, this->glContext);
    // 开垂直同步，避免无上限渲染把 GPU 跑满。
    SDL_GL_SetSwapInterval(1);

    // Windows 上 opengl32.dll 只导出 GL 1.1，3.2 Core 的函数要在这里取地址。
    // 其余平台是空实现，调用点不必分平台。
    if (!loadGLFunctions())
    {
        Logger::error(std::string("OpenGL 函数加载失败：") + SDL_GetError());
        SDL_GL_DestroyContext(this->glContext);
        this->glContext = nullptr;
        return false;
    }

    this->ready = true;
    return true;
}

bool OpenGLBackend::initImGuiRenderer()
{
    if (!this->ready)
        return false;

    return ImGui_ImplOpenGL3_Init("#version 150");
}

void OpenGLBackend::shutdownImGuiRenderer()
{
    if (this->ready)
        ImGui_ImplOpenGL3_Shutdown();

    this->ready = false;
}

void OpenGLBackend::newFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
}

void OpenGLBackend::clear(const float rgba[4])
{
    glViewport(0, 0, this->getDrawableWidth(), this->getDrawableHeight());
    glClearColor(rgba[0], rgba[1], rgba[2], rgba[3]);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLBackend::renderDrawData(ImDrawData* drawData)
{
    ImGui_ImplOpenGL3_RenderDrawData(drawData);
}

bool OpenGLBackend::readPixels(int x, int y, int width, int height,
    std::vector<unsigned char>& outRgba)
{
    if (width <= 0 || height <= 0)
        return false;

    outRgba.assign((size_t)width * (size_t)height * 4u, 0);

    // glReadPixels 的原点在左下角，读回来要上下翻一下才是"左上角为原点"
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, outRgba.data());

    const size_t stride = (size_t)width * 4u;
    std::vector<unsigned char> row(stride);
    for (int i = 0; i < height / 2; i++)
    {
        unsigned char* top    = outRgba.data() + (size_t)i * stride;
        unsigned char* bottom = outRgba.data() + (size_t)(height - 1 - i) * stride;
        std::copy(top, top + stride, row.data());
        std::copy(bottom, bottom + stride, top);
        std::copy(row.begin(), row.end(), bottom);
    }

    return true;
}

void OpenGLBackend::present()
{
    if (this->window != nullptr)
        SDL_GL_SwapWindow(this->window);
}

int OpenGLBackend::getDrawableWidth() const
{
    if (this->window == nullptr)
        return 0;
    int width = 0, height = 0;
    SDL_GetWindowSizeInPixels(this->window, &width, &height);
    return width;
}

int OpenGLBackend::getDrawableHeight() const
{
    if (this->window == nullptr)
        return 0;
    int width = 0, height = 0;
    SDL_GetWindowSizeInPixels(this->window, &width, &height);
    return height;
}

// ---------------------------------------------------------------------------
// 纹理
// ---------------------------------------------------------------------------

TextureHandle OpenGLBackend::createTexture(const unsigned char* pixels, int width, int height)
{
    TextureHandle handle;

    if (!this->ready)
    {
        // 没有上下文时 glGenTextures 会失败，在 macOS Core Profile 下甚至可能直接崩。
        Logger::warning("OpenGL 上下文未就绪，纹理创建被跳过");
        return handle;
    }

    if (pixels == nullptr || width <= 0 || height <= 0)
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
        Logger::error("纹理上传失败");
        glDeleteTextures(1, &texture);
        return handle;
    }

    // ImTextureID 在本后端里就是 GL 纹理名。
    handle.texture = (ImTextureID)(intptr_t)texture;
    handle.width   = width;
    handle.height  = height;
    return handle;
}

bool OpenGLBackend::updateTexture(TextureHandle& handle, const unsigned char* pixels, int width,
    int height)
{
    if (!this->ready || pixels == nullptr || width <= 0 || height <= 0)
        return false;

    if (handle.texture == ImTextureID_Invalid || handle.width != width || handle.height != height)
        return false;

    const GLuint texture = (GLuint)(intptr_t)handle.texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (glGetError() != GL_NO_ERROR)
    {
        Logger::error("纹理就地更新失败");
        return false;
    }

    return true;
}

void OpenGLBackend::destroyTexture(TextureHandle& handle)
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

#endif // !BKLV_SWITCH

#pragma once

// OpenGL 渲染后端。这是当前唯一实现的图形后端。
//
// 它把 app.cpp 里原来散落的 GL 上下文创建、imgui_impl_opengl3 调用、
// 以及 texture.cpp 里的 glGenTextures/glTexImage2D 全部收进这一个类。

#include "bklv/core/graphics.hpp"
#include "bklv/backends/gl_glass.hpp"

// 桌面 OpenGL 后端（依赖 SDL3）。Switch 用 gles3_backend。
#if !defined(BKLV_SWITCH)

#include <SDL3/SDL.h>

#include <memory>

namespace bklv
{

class OpenGLBackend : public RenderBackend
{
  public:
    ~OpenGLBackend() override;

    // 这个名字必须和 core/graphics.cpp 的 getGraphicsBackendName() 一致：
    // 后者在图形上下文就绪之前（App 构造期就会打日志）返回的就是这里的字面量。
    // 改这里要同步改那一处，tests/backend_name_test.cpp 会钉住这条一致性。
    const char* getName() const override { return "OpenGL 3.2 Core"; }

    // ---- 窗口 ----
    unsigned long long getWindowFlags() const override;
    bool initContext(void* nativeWindow) override;

    // ---- ImGui 渲染后端 ----
    bool initImGuiRenderer() override;
    void shutdownImGuiRenderer() override;

    void newFrame() override;
    void clear(const float rgba[4]) override;
    void renderDrawData(ImDrawData* drawData) override;
    void present() override;
    bool readPixels(int x, int y, int width, int height,
        std::vector<unsigned char>& outRgba) override;

    // ---- 可选能力：玻璃 ----
    // GL 3.2 Core 有玻璃实现（core 的 GlassBackend 抽象接口，实现见 gl_glass.*）。
    // 惰性创建：服务不用玻璃就不建任何 GL 资源。
    glass::GlassBackend* getGlassBackend() override;

    // ---- 纹理 ----
    TextureHandle createTexture(const unsigned char* pixels, int width, int height) override;
    bool updateTexture(TextureHandle& handle, const unsigned char* pixels, int width,
        int height) override;
    void destroyTexture(TextureHandle& handle) override;

  private:
    // 高 DPI 下的实际帧缓冲尺寸。
    int getDrawableWidth() const;
    int getDrawableHeight() const;

    SDL_Window* window       = nullptr;
    SDL_GLContext glContext  = nullptr;
    // GL 玻璃后端持有 GL 资源，必须在上下文销毁之前 reset（见 ~OpenGLBackend）
    std::unique_ptr<glass::GlGlassBackend> glassBackend;
};

} // namespace bklv

#endif // !BKLV_SWITCH

#pragma once

// OpenGL 渲染后端。这是当前唯一实现的图形后端。
//
// 它把 app.cpp 里原来散落的 GL 上下文创建、imgui_impl_opengl3 调用、
// 以及 texture.cpp 里的 glGenTextures/glTexImage2D 全部收进这一个类。

#include "bklv/core/graphics.hpp"

namespace bklv
{

class OpenGLBackend : public RenderBackend
{
  public:
    ~OpenGLBackend() override;

    GraphicsBackendType getType() const override { return GraphicsBackendType::OPENGL; }

    // ---- 窗口 ----
    SDL_WindowFlags getWindowFlags() const override;
    bool initContext(SDL_Window* window) override;

    // ---- ImGui 渲染后端 ----
    bool initImGuiRenderer() override;
    void shutdownImGuiRenderer() override;

    void newFrame() override;
    void clear(const float rgba[4]) override;
    void renderDrawData(ImDrawData* drawData) override;
    void present() override;

    // ---- 纹理 ----
    TextureHandle createTexture(const unsigned char* pixels, int width, int height) override;
    void destroyTexture(TextureHandle& handle) override;

  private:
    // 高 DPI 下的实际帧缓冲尺寸，count>1 时返回 1（macOS 不涉及多视图）。
    int getDrawableWidth() const;
    int getDrawableHeight() const;

    SDL_Window* window       = nullptr;
    SDL_GLContext glContext  = nullptr;
};

} // namespace bklv

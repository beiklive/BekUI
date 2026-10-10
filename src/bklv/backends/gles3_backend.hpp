#pragma once

// Switch 的 OpenGL ES 3 渲染后端。
//
// 与桌面 OpenGL 后端的区别：上下文来自 EGL + libnx 的 NWindow，
// 不是 SDL_GLContext；着色器版本是 GLES3 而不是 GLSL 150。
// 其余职责（ImGui 渲染后端、纹理创建与销毁）完全一致。

#include "bklv/core/graphics.hpp"

#if defined(BKLV_SWITCH)

namespace bklv
{

class GLES3Backend : public RenderBackend
{
  public:
    GLES3Backend();
    ~GLES3Backend() override;

    const char* getName() const override { return "OpenGL ES 3"; }

    // Switch 不走 SDL 建窗口，这两个接口留空。
    unsigned long long getWindowFlags() const override { return 0; }
    bool initContext(void* nativeWindow) override;

    bool initImGuiRenderer() override;
    void shutdownImGuiRenderer() override;

    void newFrame() override;
    void clear(const float rgba[4]) override;
    void renderDrawData(ImDrawData* drawData) override;
    void present() override;

    TextureHandle createTexture(const unsigned char* pixels, int width, int height) override;
    void destroyTexture(TextureHandle& handle) override;

  private:
    struct Impl;
    Impl* impl = nullptr;
};

} // namespace bklv

#endif // BKLV_SWITCH

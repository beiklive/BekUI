#include "bklv/backends/null_backend.hpp"

#include "imgui.h"

#include "bklv/core/logger.hpp"


namespace bklv
{

bool NullBackend::initContext(void*)
{
    this->ready = true;
    Logger::warning("使用空渲染后端：窗口会打开，但不会有任何绘制输出");
    return true;
}

bool NullBackend::initImGuiRenderer()
{
    // ImGui 1.92+ 的字体图集改由渲染后端按需创建。
    // 空后端什么都不画，但必须把"支持纹理"这个标记打上，
    // 并在 newFrame 里把纹理请求全部确认掉，否则 ImGui 会断言字体图集未构建。
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.BackendRendererName = "bklv_null";
    return true;
}

void NullBackend::shutdownImGuiRenderer()
{
    this->ready = false;
}

void NullBackend::newFrame()
{
    // 把字体图集等纹理请求全部"确认完成"（其实什么都没做）。
    ImGuiIO& io = ImGui::GetIO();
    for (ImTextureData* texture : io.Fonts->TexList)
    {
        if (texture->Status == ImTextureStatus_WantCreate || texture->Status == ImTextureStatus_WantUpdates)
            texture->SetStatus(ImTextureStatus_OK);
    }
}

void NullBackend::clear(const float[4]) { }

void NullBackend::renderDrawData(ImDrawData*) { }

void NullBackend::present() { }

TextureHandle NullBackend::createTexture(const unsigned char*, int, int)
{
    return TextureHandle();
}

void NullBackend::destroyTexture(TextureHandle& handle)
{
    handle = TextureHandle();
}

} // namespace bklv

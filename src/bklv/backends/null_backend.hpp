#pragma once

// 空渲染后端：照常开窗、照常跑帧循环，但什么都不画，纹理创建一律返回无效句柄。
//
// 用途：
//   1. -b none 启动时能在没有图形驱动的环境下验证上层逻辑（布局、焦点、滚动、回收）
//   2. 作为新后端的模板：照抄这个类，把 clear/renderDrawData/present/createTexture 换成真实实现

#include "bklv/core/graphics.hpp"

namespace bklv
{

class NullBackend : public RenderBackend
{
  public:
    const char* getName() const override { return "none"; }

    unsigned long long getWindowFlags() const override { return 0; }
    bool initContext(void*) override;

    bool initImGuiRenderer() override;
    void shutdownImGuiRenderer() override;

    void newFrame() override;
    void clear(const float rgba[4]) override;
    void renderDrawData(ImDrawData* drawData) override;
    void present() override;

    TextureHandle createTexture(const unsigned char* pixels, int width, int height) override;
    void destroyTexture(TextureHandle& handle) override;
};

} // namespace bklv

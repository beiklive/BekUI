#pragma once

// 一帧的玻璃请求队列：谁在什么地方要一块玻璃，先攒起来，绘制数据提交前统一注入。
//
// 库侧只收"声明了要玻璃的层"（opt-in）——不提供"禁区"这类裁剪机制：
// 没声明就没有玻璃，从根上不会糊到不该动的地方。宿主若确实要保护某块区域，
// 应在**提交之前**自己裁（设计器就是这么做的：预览区不入队）。

#include "bklv/glass/glass_request.hpp"

#include <vector>

namespace bklv::glass
{

class GlassRequestQueue
{
  public:
    void beginFrame(GlassBackdropStyle backdropStyle, float density);

    // 交一块玻璃。min/max 是屏幕坐标（像素），clipRect 是 ImGui 的裁剪矩形。
    void submit(GlassRequest request);

    // 收口：之后再 submit 会被忽略（避免同一帧改来改去）
    void seal();

    const std::vector<GlassRequest>& getRequests() const noexcept { return this->requests; }
    const GlassBackdropStyle& getBackdropStyle() const noexcept { return this->backdropStyle; }
    float getDensity() const noexcept { return this->density; }
    bool isEmpty() const noexcept { return this->requests.empty(); }

  private:
    std::vector<GlassRequest> requests;
    GlassBackdropStyle backdropStyle{};
    float density = 1.0f;
    bool sealed   = false;
};

} // namespace bklv::glass

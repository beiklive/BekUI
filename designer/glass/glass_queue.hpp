#pragma once

// 一帧的玻璃请求队列：谁在什么地方要一块玻璃，先攒起来，绘制数据提交前统一注入。
//
// 设计器和源项目（imgui-liquid-glass）的差别：设计器没有绘制变换栈，
// 所以省掉 applyTransforms；多出来的是"禁区"——预览区不允许被玻璃影响，
// 入队后统一按禁区裁一次（见 excludeRect）。

#include "designer/glass/glass_request.hpp"

#include <vector>

namespace designer::glass
{

class GlassRequestQueue
{
  public:
    void beginFrame(GlassBackdropStyle backdropStyle, float density);

    // 交一块玻璃。min/max 是屏幕坐标（像素），clipRect 是 ImGui 的裁剪矩形。
    void submit(GlassRequest request);

    // 禁区：任何玻璃都不许碰到这块矩形（设计器用它保护预览区）。
    // 必须在 seal() 之前调用；被禁区和请求相交时，请求会被裁到"剩下的更大那一块"。
    void excludeRect(const ImVec4& forbidden);
    // 禁区列表（Vulkan 那条路自己按它裁：它的矩形不走 draw call 注入）
    const std::vector<ImVec4>& getExcludedRects() const noexcept { return this->forbiddenRects; }

    // 收口：之后再 submit 会被忽略（避免同一帧改来改去）
    void seal();

    const std::vector<GlassRequest>& getRequests() const noexcept { return this->requests; }
    const GlassBackdropStyle& getBackdropStyle() const noexcept { return this->backdropStyle; }
    float getDensity() const noexcept { return this->density; }
    bool isEmpty() const noexcept { return this->requests.empty(); }

  private:
    std::vector<GlassRequest> requests;
    GlassBackdropStyle backdropStyle{};
    std::vector<ImVec4> forbiddenRects;
    float density = 1.0f;
    bool sealed   = false;
};

} // namespace designer::glass

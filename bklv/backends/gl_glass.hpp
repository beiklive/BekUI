#pragma once

// 玻璃的桌面 GL 后端（GLSL 150 / GL 3.2 Core）。
//
// 这个头**不含任何 GL 类型**：GL 相关内容全在 .cpp 里（pimpl），
// 这样设计器的界面代码 include 它不会和 ImGui 那套精简 GL loader 撞车。
//
// 一帧的顺序：
//   1. ImGui::Render() 之后 → inject() 把玻璃命令插进 draw data
//   2. 渲染到第一条玻璃命令时 → prepareBackdrop() 抓帧缓冲 + 模糊（每帧一次）
//   3. 每条玻璃命令 → composite() 合成那一块
//   4. 帧末 → finishFrame()

#include "bklv/glass/glass_blur_plan.hpp"
#include "bklv/glass/glass_geometry.hpp"
#include "bklv/glass/glass_request.hpp"
#include "bklv/glass/glass_queue.hpp"

#include <memory>

namespace bklv::glass
{

class GlGlassBackend
{
  public:
    GlGlassBackend();
    ~GlGlassBackend();

    GlGlassBackend(const GlGlassBackend&) = delete;
    GlGlassBackend& operator=(const GlGlassBackend&) = delete;

    // 尺寸变了会重建资源。失败返回 false 并记住"这个尺寸下不可用"（不每帧重试）。
    bool ensure(int width, int height);
    bool isReady() const;
    EffectCapabilities getCapabilities() const;

    // 抓背景 + 模糊（每帧第一次用玻璃时调用）
    bool prepareBackdrop(const GlassRequestQueue& queue);
    // 合成一块玻璃
    void composite(const GlassRequest& request);
    void finishFrame();
    void shutdown();

    // 往 ImGui 的绘制数据里插入玻璃命令（ImGui::Render() 之后调用）
    void inject(ImDrawData& drawData, const GlassRequestQueue& queue);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace bklv::glass

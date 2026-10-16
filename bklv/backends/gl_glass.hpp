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
//
// 它实现 core 的 `GlassBackend` 抽象接口：GL 系的图形后端（OpenGL / GLES3）
// 各自持有一个实例并通过 `RenderBackend::getGlassBackend()` 交出去 ——
// 玻璃服务因此不需要认识 backends（见 core/glass/glass_backend.hpp）。

#include "bklv/core/glass/glass_backend.hpp"
#include "bklv/core/glass/glass_blur_plan.hpp"
#include "bklv/core/glass/glass_geometry.hpp"
#include "bklv/core/glass/glass_request.hpp"
#include "bklv/core/glass/glass_queue.hpp"

#include <memory>

namespace bklv::glass
{

class GlGlassBackend : public GlassBackend
{
  public:
    GlGlassBackend();
    ~GlGlassBackend() override;

    GlGlassBackend(const GlGlassBackend&) = delete;
    GlGlassBackend& operator=(const GlGlassBackend&) = delete;

    // ---- GlassBackend ----

    // 尺寸变了会重建资源。失败返回 false 并记住"这个尺寸下不可用"（不每帧重试）。
    bool ensure(int width, int height) override;
    bool isReady() const override;
    EffectCapabilities getCapabilities() const override;
    // 模糊参数在 queue.beginFrame() 里已经定好，这里没有额外的事要做
    void beginFrame(const GlassTuning& tuning) override;
    // 排进队列：注入与合成发生在 prepareDrawData / 渲染期（Vulkan 那条路是当场处理）
    void submit(GlassRequestQueue& queue, const GlassRequest& request, const GlassTuning& tuning,
        int frameWidth, int frameHeight) override;
    void prepareDrawData(ImDrawData& drawData, const GlassRequestQueue& queue) override;
    void finishFrame() override;

    // ---- 实现细节（服务层不直接调，测试与调试用）----

    // 抓背景 + 模糊（每帧第一次用玻璃时调用）
    bool prepareBackdrop(const GlassRequestQueue& queue);
    // 合成一块玻璃
    void composite(const GlassRequest& request);
    void shutdown();

    // 往 ImGui 的绘制数据里插入玻璃命令（ImGui::Render() 之后调用）
    void inject(ImDrawData& drawData, const GlassRequestQueue& queue);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace bklv::glass

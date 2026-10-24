#pragma once

// 玻璃后端的抽象接口。
//
// 为什么要有它：玻璃服务在 `core/glass/`，而"怎么把背景糊掉"是图形 API 的事。
// 服务只认这个接口，具体实现（GL / Vulkan）在 `backends/` 里、由各自的后端对象持有，
// 通过 `RenderBackend::getGlassBackend()` 交出来（见 `core/graphics.hpp`）。
// 这样 core 不再反向 include backends，公开伞头 `bklv/bklv.hpp` 也就不会
// 顺着 `glass_service.hpp` 把 `<vulkan/vulkan.h>` 带进使用者的每个翻译单元。
//
// 一帧的调用顺序（由 `GlassService` 驱动）：
//   ensure(w, h) → beginFrame(tuning) → submit(...) × N
//   → prepareDrawData(drawData, queue) → finishFrame()
//
// 没有玻璃能力的后端（例如将来某个只做基础的实现）不实现这个接口，
// `getGlassBackend()` 返回 nullptr，玻璃整层静默关闭。

#include "bklv/graphics/render/glass/glass_geometry.hpp"
#include "bklv/graphics/render/glass/glass_queue.hpp"
#include "bklv/graphics/render/glass/glass_tuning.hpp"

namespace bklv::glass
{

class GlassBackend
{
  public:
    virtual ~GlassBackend() = default;

    // 尺寸变了就重建资源（失败会被记住，不在同一尺寸上每帧重试）。
    // 返回"资源是否就绪"；能力是否够用由调用方按 getCapabilities() 判断。
    virtual bool ensure(int framebufferWidth, int framebufferHeight) = 0;

    virtual bool isReady() const = 0;
    virtual EffectCapabilities getCapabilities() const = 0;

    // 帧首、第一块玻璃之前：按这一帧的 tuning 准备参数
    virtual void beginFrame(const GlassTuning& tuning) = 0;

    // 收下一块玻璃。queue 由服务持有、是唯一的请求清单：
    //   · GL 实现把它排进队列，等 prepareDrawData() 时注入绘制命令；
    //   · Vulkan 实现当场把矩形交给自己的通道（模糊只有形状，染色/描边由它叠 UI 图元）。
    // 坐标已是物理像素、density 已补齐；frameWidth/Height 用于兜住空的裁剪矩形。
    virtual void submit(GlassRequestQueue& queue, const GlassRequest& request,
        const GlassTuning& tuning, int frameWidth, int frameHeight)
        = 0;

    // `ImGui::Render()` 之后、`renderDrawData()` 之前
    virtual void prepareDrawData(ImDrawData& drawData, const GlassRequestQueue& queue) = 0;

    // 帧末清理
    virtual void finishFrame() = 0;
};

} // namespace bklv::glass

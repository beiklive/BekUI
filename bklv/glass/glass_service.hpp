#pragma once

// 玻璃服务：一帧里的"收集 → 注入"中枢。
//
// 为什么是进程级单例（和 `getInputManager()` / 纹理仓库同一套做法）：
//   **请求来自任意视图**（视图在自己的 draw 里声明"我这一层是玻璃"），
//   而**注入点在 `App::frame` 里**（`ImGui::Render()` 之后、`renderDrawData()` 之前）。
//   两边互不相识，中间需要一个大家都够得着的地方 —— 就是这里。
//
// 所有权与开关的语义（用户拍板）：
//   · Shell 是**面向使用者的所有者**（`Shell::setGlassEnabled/setGlassQuality/getGlassTuning`）
//   · 样式参数的优先级：**外观层 JSON 写了就用 JSON，没写才用这里的 GlassTuning**
//   · `enabled` / `quality` / 能力只由这里（也就是 Shell）决定 —— 是开关与性能，不是样式
//
// 默认**关闭**：库不主动改变宿主观感，宿主显式 `setEnabled(true)` 才生效。

#include "bklv/backends/gl_glass.hpp"
#include "bklv/glass/glass_queue.hpp"
#include "bklv/glass/glass_tuning.hpp"

namespace bklv::glass
{

class GlassService
{
  public:
    // 单例（进程级）。取名沿库里的约定：getInputManager() / getEventBus() 那一套。
    static GlassService& getInstance();

    // ---- 开关 / 档位 / 参数（Shell 转发到这里）----
    void setEnabled(bool value) { this->tuning.enabled = value; }
    bool isEnabled() const { return this->tuning.enabled; }

    void setQuality(GlassQuality value) { this->tuning.quality = (int)value; }
    GlassQuality getQuality() const { return (GlassQuality)this->tuning.quality; }

    GlassTuning& getTuning() { return this->tuning; }
    const GlassTuning& getTuning() const { return this->tuning; }

    // 后端能力（拿不到就别把玻璃开关暴露给用户）
    EffectCapabilities getCapabilities() const;
    // 后端是否可用（尺寸合法 + 资源就绪 + 能力非空）
    bool isSupported() const { return this->supported; }

    // ---- 每帧由 App::frame 驱动 ----
    // 帧首：清队列、记下帧缓冲尺寸与密度（物理像素）
    void beginFrame(int framebufferWidth, int framebufferHeight, float density);

    // 视图在自己的 draw 里调用：声明一块玻璃。请求坐标是**物理像素**。
    void submit(GlassRequest request);

    // 简便入口（推荐）：**玻璃画在我下面、我自己的内容画在玻璃上面**。
    // 内部会先在 drawList 上切一条新命令再入队 —— 直接手填 drawCommandOffset 会被
    // ImGui 的"连续图元合并进同一条命令"吃掉，导致自己的文字被玻璃盖住（踩过）。
    // pixelBounds：x,y=左上；z,w=右下（**物理像素**）。
    bool submitBackdrop(ImDrawList* drawList, const ImVec4& pixelBounds,
        const CornerRadii& radii, GlassShapeKind shape, float density);

    // ImGui::Render() 之后、renderDrawData() 之前：把玻璃命令注入绘制数据
    void prepareDrawData(ImDrawData& drawData);

    // 帧末清理
    void finishFrame();

    // 本帧收集到几块玻璃（测试与调试用）
    size_t getRequestCount() const { return this->queue.getRequests().size(); }

  private:
    GlassService() = default;
    GlassService(const GlassService&) = delete;
    GlassService& operator=(const GlassService&) = delete;

    bool ensureBackend();

    GlassTuning tuning{};
    GlassRequestQueue queue;
    GlGlassBackend backend;
    EffectCapabilities capabilities{};
    int frameWidth   = 0;
    int frameHeight  = 0;
    float density    = 1.0f;
    bool supported   = false;
    bool injected    = false;
};

// 服务入口（自由函数，避免到处写 getInstance()）
GlassService& getGlassService();

} // namespace bklv::glass

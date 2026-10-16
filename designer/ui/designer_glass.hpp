#pragma once

// 设计器的液态玻璃门面：把"要哪些玻璃面"和"GL 后端"隔开。
//
// 只服务设计器界面外壳（顶栏、菜单栏，之后是左右面板）；
// **预览区是禁区**：那里的像素一个都不许被玻璃影响（见 excludeRect）。
//
// 后端是 GL（GLSL 150）；能力不够（拿不到 GL 入口 / 着色器编不过 / 尺寸太小）
// 时整体关闭，面板回退普通绘制，设计器不会因此不能用。
// 开关：环境变量 BKLV_DESIGNER_GLASS=0 关掉（默认开）。

#include "designer/glass/glass_lib.hpp"
#include "bklv/core/glass/glass_queue.hpp"
#include "bklv/core/glass/glass_tuning.hpp"
#include "bklv/backends/gl_glass.hpp"

#include <string>
#include <vector>

#if defined(BKLV_VULKAN)
#include <memory>

#include "bklv/backends/vk_glass.hpp"
#endif

namespace designer
{

class DesignerGlass
{
  public:
    DesignerGlass();

    bool isEnabled() const { return this->tuning.enabled; }
    void setEnabled(bool value) { this->tuning.enabled = value; }

    // 调参数据（设计器界面上的滑块改它；也会存进 ui_state）
    glass::GlassTuning& getTuning() { return this->tuning; }
    const glass::GlassTuning& getTuning() const { return this->tuning; }

    // 环境变量覆盖（BKLV_DESIGNER_GLASS / _GLASS_PANEL）：必须在 loadUiState()
    // 之后调 —— 状态文件里存着一份参数，会盖掉构造时的默认值
    void applyEnvOverrides();

    // 每帧开头（帧缓冲尺寸 = 物理像素，density = framebufferScale）
    void beginFrame(int framebufferWidth, int framebufferHeight, float density);

    // 两块玻璃：横条（菜单栏/状态栏）与面板（左右栏/浮窗）。
    // rect 用 ImGui 的逻辑坐标，list/offset 是注入位置。
    void addBar(const ImVec4& rect, ImDrawList* list, int commandOffset);
    void addPanel(const ImVec4& rect, ImDrawList* list, int commandOffset);

    // 禁区（预览区）：玻璃一律不许碰到
    void excludeRect(const ImVec4& rect);

    // 收口（这一帧的玻璃面就这些了）
    void endFrame();

    // 当前帧有没有玻璃要画（没有就不必注入）
    bool hasWork() const { return this->tuning.enabled && !this->queue.isEmpty(); }

    // Vulkan 那条路：底由玻璃自己铺（上一帧画面 + mip 链模糊），
    // ImGui 就不要再画壁纸了，否则会把玻璃盖掉
    bool usesOwnWallpaper() const
    {
#if defined(BKLV_VULKAN)
        return this->vulkanActive;
#else
        return false;
#endif
    }

    // ImGui::Render() 之后、renderDrawData() 之前：注入玻璃命令
    void prepareDrawData(ImDrawData* drawData);

    // 帧末清理
    void finishFrame();

    const glass::EffectCapabilities& getCapabilities() const { return this->capabilities; }
    // 诊断信息（状态栏/日志用）
    const std::string& getStatus() const { return this->status; }

  private:
#if defined(BKLV_VULKAN)
    // Vulkan 路径：不建管线，只做 mip 链模糊 + 让 ImGui 叠一层圆角染色
    void addVulkanRect(const ImVec4& rect, ImDrawList* list, bool isBar);
    std::unique_ptr<glass::VkGlass> vk;
    bool vulkanActive  = false;
    float vulkanDensity = 1.0f;
#endif

    void submit(const ImVec4& rect, const glass::CornerRadii& radii,
        const glass::GlassStyle& style, ImDrawList* list, int commandOffset);

    // 禁区（预览区）由设计器自己维护：库侧的队列只有 opt-in，不含裁剪机制
    // （见 bklv/core/glass/glass_queue.hpp 的说明）。这里是**物理像素**坐标。
    bool clipAgainstForbidden(ImVec4& rect) const;

    std::vector<ImVec4> forbiddenRects;
    glass::GlassTuning tuning{};
    glass::GlGlassBackend backend;
    glass::GlassRequestQueue queue;
    glass::EffectCapabilities capabilities{};
    std::string status = "未初始化";
    bool backendChecked = false;
    bool injectedThisFrame = false;
    int width = 0;
    int height = 0;
};

} // namespace designer

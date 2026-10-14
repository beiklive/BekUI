#pragma once

// 设计器的液态玻璃门面：把"要哪些玻璃面"和"GL 后端"隔开。
//
// 只服务设计器界面外壳（顶栏、菜单栏，之后是左右面板）；
// **预览区是禁区**：那里的像素一个都不许被玻璃影响（见 excludeRect）。
//
// 后端是 GL（GLSL 150）；能力不够（拿不到 GL 入口 / 着色器编不过 / 尺寸太小）
// 时整体关闭，面板回退普通绘制，设计器不会因此不能用。
// 开关：环境变量 BKLV_DESIGNER_GLASS=0 关掉（默认开）。

#include "designer/glass/glass_queue.hpp"
#include "designer/glass/gl/gl_backend.hpp"

#include <string>

namespace designer
{

class DesignerGlass
{
  public:
    DesignerGlass();

    bool isEnabled() const { return this->enabled; }
    void setEnabled(bool value) { this->enabled = value; }

    // 每帧开头（帧缓冲尺寸 = 物理像素，density = framebufferScale）
    void beginFrame(int framebufferWidth, int framebufferHeight, float density);

    // 交一块玻璃：rect 是屏幕坐标（逻辑像素），list/offset 是注入位置
    void addPanel(const ImVec4& rect, const glass::CornerRadii& radii,
        const glass::GlassStyle& style, ImDrawList* list, int commandOffset);

    // 禁区（预览区）：玻璃一律不许碰到
    void excludeRect(const ImVec4& rect);

    // 收口（这一帧的玻璃面就这些了）
    void endFrame();

    // 当前帧有没有玻璃要画（没有就不必注入）
    bool hasWork() const { return this->enabled && !this->queue.isEmpty(); }

    // ImGui::Render() 之后、renderDrawData() 之前：注入玻璃命令
    void prepareDrawData(ImDrawData* drawData);

    // 帧末清理
    void finishFrame();

    const glass::EffectCapabilities& getCapabilities() const { return this->capabilities; }
    // 诊断信息（状态栏/日志用）
    const std::string& getStatus() const { return this->status; }

  private:
    glass::GlGlassBackend backend;
    glass::GlassRequestQueue queue;
    glass::EffectCapabilities capabilities{};
    std::string status = "未初始化";
    bool enabled = true;
    bool backendChecked = false;
    bool injectedThisFrame = false;
    int width = 0;
    int height = 0;
};

} // namespace designer

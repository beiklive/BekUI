#pragma once

// 调试窗口：用 ImGui 窗口显示日志与运行状态。
//
// 与 borealis 的 DebugLayer（views/debug_layer.cpp）对应，但实现方式不同：
//   borealis —— 每条日志 new 一个 Box + 3 个 Label 挂到 View 树上
//   bklv     —— 日志存在 Logger 的环形缓冲里，这里用 ImGui 窗口按需读取
//
// 后者不用为每条日志触发一次 Yoga 重排；窗口的拖动、缩放、滚动、裁剪都由 ImGui 提供。
// 日志行用 ImGuiListClipper 裁剪，缓冲里有多少条都只画可见的几行。

#include "bklv/core/logger.hpp"

namespace bklv
{

class DebugWindow
{
  public:
    void setEnabled(bool enabled) { this->enabled = enabled; }
    bool isEnabled() const { return this->enabled; }
    void toggle() { this->enabled = !this->enabled; }

    // 每帧画一次。必须在 ImGui::NewFrame() 之后、ImGui::Render() 之前调用。
    void draw();

  private:
    void drawToolbar();

    bool enabled = false;

    // 工具栏状态
    bool autoScroll = true;
    bool showDebug  = true;
    bool showInfo   = true;
    bool showWarning = true;
    bool showError   = true;
    bool showLocation = true;

    // 上一帧看到的日志版本号，用来判断是否需要滚到底
    uint64_t lastVersion = 0;
};

} // namespace bklv

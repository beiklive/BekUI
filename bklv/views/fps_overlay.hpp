#pragma once

// 顶层帧率覆盖：右上角显示"当前 FPS（一位小数）/ 目标帧率"，下面一行是帧时间统计
// （p99 与最坏帧，最近 4 秒窗口，和 `FrameStats` 同一口径）。
//
// 两条实现约定：
// 1) 挂在 `Shell::getDebugLayer()`（HUD 之上、永远最后绘制），绝对定位，不参与弹层布局；
// 2) 文本只在统计窗口到期时更新（0.5s 一次），且**不用 Label** —— 直接 draw 里 AddText，
//    避免每 0.5s 触发一次 Yoga 重排。

#include <string>

#include "bklv/core/frame_stats.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

class FpsOverlay : public View
{
  public:
    FpsOverlay();

    void update() override;
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        FrameContext* ctx) override;

    std::string getClassString() const override { return "bklv::FpsOverlay"; }

  private:
    FrameStats stats;       // 只用来算 p99 / 最坏（自采样本）
    int        warmup = 0; // 启动预热帧数，期间的帧不进统计
    float      window = 0.0f;
    int        frames = 0;
    float      fps    = 0.0f;
    float      p99Ms  = 0.0f;
    float      worstMs = 0.0f;
};

} // namespace bklv

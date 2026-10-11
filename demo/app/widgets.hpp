#pragma once

// 总览页里用到的自定义控件。
//
// 这些控件的存在意义是演示"怎么用 View + ImDrawList 自己画一个控件"：
// 不继承 ImGui 的控件体系，而是重写 draw() 直接下发绘制命令。

#include "bklv/core/view.hpp"

namespace demo
{

// 开关：InvisibleButton 式的交互 + ImDrawList 手绘轨道与滑块。
// 靠 onFocusGained/Lost 自己维护焦点高亮，靠 registerAction 处理确认键。
class ToggleSwitch : public bklv::View
{
  public:
    ToggleSwitch();

    void setValue(bool v) { this->value = v; }
    bool getValue() const { return this->value; }
    void toggle() { this->value = !this->value; }

    bool isFocusable() const override { return true; }
    void onFocusGained() override { this->focused = true; }
    void onFocusLost() override { this->focused = false; }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, bklv::Style style,
        bklv::FrameContext* ctx) override;

  private:
    bool value   = false;
    bool focused = false;
};

// 环形进度：用 PathArcTo + PathStroke 画弧，展示路径式绘制。
// update() 每帧推进一个内部相位，用来证明逐帧回调确实会走到。
class CircularProgress : public bklv::View
{
  public:
    CircularProgress();

    void setValue(float v);
    float getValue() const { return this->value; }

    void update() override;

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, bklv::Style style,
        bklv::FrameContext* ctx) override;

  private:
    float value = 0.0f;
    float phase = 0.0f; // 内部相位，仅仅为了演示 update()
};

// 横向进度条
class ProgressBar : public bklv::View
{
  public:
    ProgressBar();

    void setValue(float v);
    void setColor(ImU32 c) { this->color = c; }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, bklv::Style style,
        bklv::FrameContext* ctx) override;

  private:
    float value = 0.0f;
    ImU32 color = 0;
};

} // namespace demo

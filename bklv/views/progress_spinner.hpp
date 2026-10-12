#pragma once

// 加载指示器，对应 borealis 的 views/progress_spinner.hpp。
//
// ImGui 1.93 的 ImDrawList 没有旋转 API，所以这里自己算旋转后的四点，
// 用 AddQuadFilled 拼出一个由多段组成的转圈。

#include "bklv/core/time.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

class ProgressSpinner : public View
{
  public:
    ProgressSpinner();

    // 一圈需要多少秒。
    void setDuration(float seconds) { this->duration = seconds; }
    void setThickness(float thickness) { this->thickness = thickness; }
    void setColor(ImU32 color)
    {
        this->spinnerColor   = color;
        this->colorExplicit  = true;
    }

    // 跟随主题（color.spinner.bar，对应 brls/spinner/bar_color）；显式设过就不覆盖
    void applyTheme() override;

    // 每帧推进旋转角度。
    void update() override;

    std::string getClassString() const override { return "bklv::ProgressSpinner"; }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, Style style,
        FrameContext* ctx) override;

    float angle     = 0.0f;
    float duration  = 1.2f;
    float thickness = 3.0f;
    int segments    = 12;
    ImU32 spinnerColor  = IM_COL32(120, 190, 255, 255);
    bool colorExplicit  = false;
};

} // namespace bklv

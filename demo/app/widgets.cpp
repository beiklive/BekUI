#include "app/widgets.hpp"

#include <cmath>

#include "bklv/core/font.hpp"

using namespace bklv;

namespace demo
{

// ---------------------------------------------------------------------------
// ToggleSwitch
// ---------------------------------------------------------------------------

ToggleSwitch::ToggleSwitch()
{
    this->setDimensions(200.0f, 32.0f);
}

void ToggleSwitch::draw(ImDrawList* drawList, float x, float y, float /*width*/, float height, bklv::Style,
    bklv::FrameContext*)
{
    const float trackWidth  = 54.0f;
    const float trackHeight = 24.0f;
    const float trackY      = y + (height - trackHeight) * 0.5f;

    const ImU32 track = this->value ? IM_COL32(74, 144, 226, 255) : IM_COL32(70, 76, 86, 255);
    const ImU32 knob  = this->value ? IM_COL32(250, 252, 255, 255) : IM_COL32(150, 158, 170, 255);

    drawList->AddRectFilled(ImVec2(x, trackY), ImVec2(x + trackWidth, trackY + trackHeight), track,
        trackHeight * 0.5f);

    const float knobX = x + (this->value ? trackWidth - 17.0f : 17.0f);
    drawList->AddCircleFilled(ImVec2(knobX, trackY + trackHeight * 0.5f), 9.0f, knob);

    const ImU32 textColor = this->focused ? IM_COL32(235, 238, 242, 255) : IM_COL32(150, 158, 170, 255);
    drawList->AddText(ImVec2(x + trackWidth + 12.0f, y + (height - 20.0f) * 0.5f), textColor,
        this->value ? "已开启" : "已关闭");

    if (this->focused)
        drawList->AddRect(ImVec2(x - 3.0f, trackY - 3.0f),
            ImVec2(x + trackWidth + 3.0f, trackY + trackHeight + 3.0f), IM_COL32(120, 190, 255, 200), 6.0f, 0,
            2.0f);
}

// ---------------------------------------------------------------------------
// CircularProgress
// ---------------------------------------------------------------------------

CircularProgress::CircularProgress()
{
    this->setDimensions(72.0f, 72.0f);
}

void CircularProgress::setValue(float v)
{
    this->value = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

void CircularProgress::update()
{
    // 只是为了让 update() 有可见效果；真实控件这里会推进动画。
    this->phase += ImGui::GetIO().DeltaTime * 0.35f;
    if (this->phase > 1.0f)
        this->phase -= 1.0f;
}

void CircularProgress::draw(ImDrawList* drawList, float x, float y, float width, float height, bklv::Style,
    bklv::FrameContext*)
{
    constexpr float PI = 3.14159265f;

    const ImVec2 center(x + width * 0.5f, y + height * 0.5f);
    const float radius = (width < height ? width : height) * 0.5f - 6.0f;

    // 底环
    drawList->PathArcTo(center, radius, 0.0f, 2.0f * PI, 48);
    drawList->PathStroke(IM_COL32(60, 66, 78, 255), 0, 6.0f);

    // 进度弧：从 -90 度开始顺时针
    const float start = -PI * 0.5f;
    if (this->value > 0.001f)
    {
        drawList->PathArcTo(center, radius, start, start + 2.0f * PI * this->value, 48);
        drawList->PathStroke(IM_COL32(120, 190, 255, 255), 0, 6.0f);
    }

    // 中心百分比
    char text[16];
    std::snprintf(text, sizeof(text), "%.0f%%", this->value * 100.0f);

    ImFont* font = bklv::getDefaultFont();
    if (font != nullptr)
    {
        const ImVec2 size = font->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, text);
        drawList->AddText(font, 14.0f, ImVec2(center.x - size.x * 0.5f, center.y - 8.0f),
            IM_COL32(225, 230, 238, 255), text);
    }
}

// ---------------------------------------------------------------------------
// ProgressBar
// ---------------------------------------------------------------------------

ProgressBar::ProgressBar()
{
    this->setHeight(10.0f);
    this->color = IM_COL32(120, 190, 255, 255);
}

void ProgressBar::setValue(float v)
{
    this->value = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

void ProgressBar::draw(ImDrawList* drawList, float x, float y, float width, float height, bklv::Style,
    bklv::FrameContext*)
{
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), IM_COL32(52, 58, 70, 255),
        height * 0.5f);

    if (this->value > 0.001f)
        drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + width * this->value, y + height), this->color,
            height * 0.5f);
}

} // namespace demo

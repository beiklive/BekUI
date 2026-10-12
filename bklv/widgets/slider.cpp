#include "bklv/widgets/slider.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "bklv/core/attributes.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

namespace
{

void setSliderProgressAttr(View* v, const ResolvedValue& r)
{
    static_cast<Slider*>(v)->setProgress(r.number);
}

void setSliderVariantAttr(View* v, const ResolvedValue& r)
{
    static_cast<Slider*>(v)->setAppearanceVariant(r.text);
}

} // namespace

Slider::Slider(const std::string& variant)
    : AppearanceView(variant.empty() ? "slider" : "slider." + variant, true)
{
    // 宽高在 widgets/slider.json 的 box 段里
    this->onPointerAction = true;

    // 手柄上左右调整
    this->registerAction(ControllerButton::LEFT, "减小", [this](View*) {
        this->setProgress(this->progress - this->step);
        if (this->onChange)
            this->onChange(this->progress);
    });
    this->registerAction(ControllerButton::RIGHT, "增大", [this](View*) {
        this->setProgress(this->progress + this->step);
        if (this->onChange)
            this->onChange(this->progress);
    });

    this->applyTheme();
}

void Slider::setProgress(float value)
{
    const float clamped = std::max(0.0f, std::min(1.0f, value));
    if (std::fabs(clamped - this->progress) < 0.0005f)
        return;

    this->progress = clamped;
}

void Slider::update()
{
    AppearanceView::update();
    this->applyPointer();
}

void Slider::applyPointer()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return; // 无窗口测试没有指针输入

    ImGuiIO& io = ImGui::GetIO();
    if (!io.MouseDown[0] && !io.MouseDown[1])
    {
        if (this->dragging)
            this->dragging = false;
        return;
    }

    // 命中判定用控件自己的矩形；和 ScrollingFrame 的拖拽是同一套做法
    const Rect frame = this->getFrame();
    const bool inside = io.MousePos.x >= frame.x && io.MousePos.x <= frame.getMaxX()
        && io.MousePos.y >= frame.y && io.MousePos.y <= frame.getMaxY();

    if (!this->dragging && !inside)
        return;

    this->dragging = true;

    const float ratio = (frame.width > 0.0f) ? (io.MousePos.x - frame.x) / frame.width : 0.0f;
    const float before = this->progress;
    this->setProgress(ratio);

    if (before != this->progress && this->onChange)
        this->onChange(this->progress);
}

void Slider::drawCustomLayer(ImDrawList* drawList, const AppearanceLayer&, float x, float y,
    float width, float height, const AppearanceInstance& appearance)
{
    if (width <= 0.0f || height <= 0.0f)
        return;

    // 从外观里按 id 取三段的样式（颜色/圆角/粗细都在 JSON 里，这里只算几何）
    const int trackIndex = appearance.getAppearance().indexOfLayer("track");
    const int fillIndex  = appearance.getAppearance().indexOfLayer("fill");
    const int knobIndex  = appearance.getAppearance().indexOfLayer("knob");

    auto layerOf = [&](int index) -> const AppearanceLayer* {
        if (index < 0)
            return nullptr;
        return &appearance.getAppearance().getLayers()[(size_t)index];
    };

    const AppearanceLayer* track = layerOf(trackIndex);
    const AppearanceLayer* fill  = layerOf(fillIndex);
    const AppearanceLayer* knob  = layerOf(knobIndex);

    const float trackThickness = track != nullptr && track->height > 0.0f ? track->height : 6.0f;
    const float trackRadius = track != nullptr ? track->radius : trackThickness * 0.5f;
    const float knobSize    = knob != nullptr && knob->width > 0.0f ? knob->width : 16.0f;
    const float knobRadius  = knob != nullptr ? knob->radius : knobSize * 0.5f;

    const float trackY = y + (height - trackThickness) * 0.5f;
    // 滑块中心随进度移动，两端各留半个滑块，避免贴边时被裁掉
    const float knobCenter = x + knobSize * 0.5f
        + this->progress * std::max(0.0f, width - knobSize);
    const float knobY = y + (height - knobSize) * 0.5f;

    if (track != nullptr && trackIndex >= 0)
        drawList->AddRectFilled(ImVec2(x, trackY), ImVec2(x + width, trackY + trackThickness),
            appearance.getColor(trackIndex, AppearanceProp::FILL), trackRadius);

    if (fill != nullptr && fillIndex >= 0)
        drawList->AddRectFilled(ImVec2(x, trackY), ImVec2(knobCenter, trackY + trackThickness),
            appearance.getColor(fillIndex, AppearanceProp::FILL), trackRadius);

    if (knob != nullptr && knobIndex >= 0)
        drawList->AddRectFilled(ImVec2(knobCenter - knobSize * 0.5f, knobY),
            ImVec2(knobCenter + knobSize * 0.5f, knobY + knobSize),
            appearance.getColor(knobIndex, AppearanceProp::FILL), knobRadius);
}

const AttributeTable& Slider::attributes() const
{
    static const AttributeTable table = {
        { "progress", AttributeKind::NUMBER, setSliderProgressAttr, nullptr },
        { "step", AttributeKind::NUMBER, [](View* v, const ResolvedValue& r) {
             static_cast<Slider*>(v)->setStep(r.number);
         }, nullptr },
        { "variant", AttributeKind::STRING, setSliderVariantAttr, nullptr },
    };
    return table;
}

} // namespace bklv

#pragma once

// 滑块（对应 borealis 的 views/slider）。
//
// 外观 JSON 声明三段：`track`（轨道）/ `fill`（已选部分）/ `knob`（滑块），
// 以及它们各自的颜色、圆角、粗细，还有聚焦/禁用状态下的动效。
// **进度决定几何**这件事是语义性的，留在 C++：`custom` 层里按 progress 画出来。
// 换句话说：JSON 管"长什么样"，C++ 管"画在哪里"。

#include <functional>
#include <string>

#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

class Slider : public AppearanceView
{
  public:
    explicit Slider(const std::string& variant = "");

    void setProgress(float progress); // 0~1，会被夹取
    float getProgress() const { return this->progress; }

    // 方向键一次调整多少（0~1）
    void setStep(float step) { this->step = step; }

    void setOnChange(std::function<void(float)> callback) { this->onChange = std::move(callback); }

    void update() override;

    std::string getClassString() const override { return "bklv::Slider"; }
    const AttributeTable& attributes() const override;

  protected:
    void drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x, float y,
        float width, float height, const AppearanceInstance& appearance) override;

  private:
    void applyPointer();

    float progress = 0.0f;
    float step     = 0.05f;
    bool dragging  = false;
    std::function<void(float)> onChange;
};

} // namespace bklv

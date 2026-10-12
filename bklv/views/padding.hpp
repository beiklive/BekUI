#pragma once

// 空白占位：auto 尺寸 + grow=1 的空视图。
//
// 对标 borealis 的 `brls:Padding`（core/box.hpp）：
// "An empty view that has auto x auto and grow=1.0 to push all the next views
//  in its box to the right (or to the bottom)"。
// 用途就是占位挤压：`[标题][Padding][副标题]` 把副标题顶到右边，
// 比再套一层 "grow=1 + justifyContent=flex_end" 的 Box 更直白（borealis 的 header
// 就是这么写的）。绘制时什么都不画，也不吃焦点。

#include "bklv/core/view.hpp"

namespace bklv
{

class Padding : public View
{
  public:
    Padding();

    // 空视图：不画、不聚焦、不参与命中
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        FrameContext* ctx) override;
    bool isFocusable() const override { return false; }

    std::string getClassString() const override { return "bklv::Padding"; }

    const AttributeTable& attributes() const override;
};

} // namespace bklv

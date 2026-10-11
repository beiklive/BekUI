#pragma once

// 矩形：外观完全由主题 JSON 描述的纯装饰控件（对应 borealis 的 views/rectangle）。
//
// 它也是"外观驱动"这套机制的最小示例：本身没有绘制代码，
// 底/边框/圆角/状态动效都写在 `resources/theme/<theme>/widgets/rectangle.json` 里。
// variant 可以指向另一个文件（如 "card" -> widgets/rectangle.card.json），
// 这样同一个控件类能出多种外观，不用改代码。

#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

class Rectangle : public AppearanceView
{
  public:
    explicit Rectangle(const std::string& variant = "");

    std::string getClassString() const override { return "bklv::Rectangle"; }
    const AttributeTable& attributes() const override;
};

} // namespace bklv

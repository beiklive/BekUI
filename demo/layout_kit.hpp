#pragma once

// 布局演示用的小工具：一节（小标题 + 内容）与一块纯色矩形。
//
// 为什么不做成库里的控件：
//   - "一节"只是演示页的排版习惯（小标题 + 一段内容），没有交互语义；
//   - "纯色矩形"用 `Box::setBackgroundColor()` 就够，库里不需要多一个 Rectangle。

#include "bklv/ui/view/box.hpp"

#include "demo/style.hpp"

namespace demo
{

// 一节：小标题在上，内容在下面（内容为 null 时只有标题）。
inline bklv::Box* makeSection(const std::string& title, bklv::View* content)
{
    auto* section = new bklv::Box(bklv::Axis::COLUMN);
    section->setGap(10.0f);
    section->addView(makeLabel(title, 20.0f, mutedColor()));

    if (content != nullptr)
        section->addView(content);

    return section;
}

// 一块纯色矩形（布局演示用）。width <= 0 表示按父容器的拉伸规则来。
inline bklv::Box* makeColorRect(ImU32 color, float height, float width = 0.0f)
{
    auto* rect = new bklv::Box(bklv::Axis::COLUMN);
    rect->setBackgroundColor(color);
    rect->setHeight(height);

    if (width > 0.0f)
        rect->setWidth(width);

    return rect;
}

} // namespace demo

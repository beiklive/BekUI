#pragma once

// 布局演示用的小工具：一节（小标题 + 内容）与一块纯色矩形。
//
// "一节"的标题现在是库里的 `bklv::Section`（外观 `base/section/section`：
// 左侧竖条 + 左边主信息 + 右边副信息，字号/颜色/竖条全在 JSON 里）。
// 这里只负责把它和内容竖着摞起来。
//
// "纯色矩形"用 `Box::setBackgroundColor()` 就够，库里不需要多一个 Rectangle。

#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/basic/section.hpp"

#include "demo/style.hpp"

namespace demo
{

// 一节：小标题在上，内容在下面（内容为 null 时只有标题）。
inline bklv::Box* makeSection(const std::string& title, bklv::View* content, const std::string& info = std::string())
{
    auto* section = new bklv::Box(bklv::Axis::COLUMN);
    section->setGap(10.0f);
    section->addView(new bklv::Section(title, info));

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

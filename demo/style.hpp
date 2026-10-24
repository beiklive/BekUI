#pragma once

// demo 的取色与文字小工具。
//
// 颜色全部从当前主题读，键名就是 `resources/theme/<主题>/tokens/colors.json` 里
// 扁平化之后的写法（`text.primary` → `color.text.primary`）。为什么要显式取色：
// Label 默认用 `color.text.primary`，而窗口底色是 App 的清屏色（深色），
// 页面又没铺 surface，深字压在深底上几乎看不见 —— 所以 demo 自己铺一层
// `color.surface.page` 做页面底色，文字用主题里配套的深色，两者成对出现。
//
// 读不到的键退化成兜底色，主题缺项也不会变成不可见的颜色。

#include <string>

#include "bklv/ui/theme/theme_data.hpp"
#include "bklv/ui/widgets/basic/label.hpp"

namespace demo
{

// 页面底色（铺在根视图上，盖住 App 的清屏色）。
inline ImU32 pageBackgroundColor()
{
    return bklv::getTheme().color("color.surface.page", IM_COL32(242, 243, 245, 255));
}

// 正文色。
inline ImU32 textColor()
{
    return bklv::getTheme().color("color.text.primary", IM_COL32(31, 35, 40, 255));
}

// 次要文字色（说明、页脚、状态行）。
inline ImU32 mutedColor()
{
    return bklv::getTheme().color("color.text.dim", IM_COL32(95, 102, 112, 255));
}

// 建一段文字：字号与颜色一次给全，省得每个页面重复三行。
inline bklv::Label* makeLabel(const std::string& text, float fontSize, ImU32 color)
{
    auto* label = new bklv::Label();
    label->setText(text);
    label->setFontSize(fontSize);
    label->setTextColor(color);
    return label;
}

// 标题 / 正文 / 说明三种常用规格。
inline bklv::Label* makeTitle(const std::string& text)
{
    return makeLabel(text, 34.0f, textColor());
}

inline bklv::Label* makeParagraph(const std::string& text)
{
    return makeLabel(text, 20.0f, textColor());
}

inline bklv::Label* makeNote(const std::string& text)
{
    return makeLabel(text, 18.0f, mutedColor());
}

} // namespace demo

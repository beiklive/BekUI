// 按钮展示页（骨架）。
//
// 目的：把所有 button 外观摆在一屏里，方便边改 `base/button/*.json` 边看效果。
// 目前每行是「说明 + 一个按钮」；后面要加状态演示（focused / disabled / 长文案 / 图标组合）
// 就在对应 section 里继续 addView —— 不需要动外层结构。

#include "app/pages.hpp"
#include "bklv/bklv.hpp"
#include "bklv/core/icon_font.hpp"

using namespace bklv;

namespace demo
{

namespace
{

Box* makeSection(const std::string& title)
{
    Box* box = new Box(Axis::COLUMN);
    box->setWidthPercentage(100.0f);
    box->setGap(getTheme().number("metric.spacing.sm", 8.0f));

    Label* label = new Label();
    label->setText(title);
    label->setFontSize(getTheme().number("metric.font.size.md", 18.0f));
    label->setTextColor(getTheme().color("color.text.dim"));
    box->addView(label);
    return box;
}

// 一行：左边固定说明，右边按钮（按钮样式由参数决定）
Button* makeButtonRow(Box* section, const std::string& caption, const std::string& appearance,
    const std::string& text, const std::string& icon = std::string())
{
    Box* row = new Box(Axis::ROW);
    row->setWidthPercentage(100.0f);
    row->setGap(getTheme().number("metric.spacing.md", 12.0f));
    row->setAlignItems(AlignItems::CENTER);

    Label* label = new Label();
    label->setText(caption);
    label->setFontSize(getTheme().number("metric.font.size.sm", 16.0f));
    label->setTextColor(getTheme().color("color.text.disabled"));
    label->setWidth(220.0f);
    row->addView(label);

    Button* button = new Button();
    button->setAppearanceName(appearance);
    button->setText(text);
    if (!icon.empty())
        button->setIcon(icon);
    row->addView(button);

    section->addView(row);
    return button;
}

} // namespace

Box* buildButtonShowcasePage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setShrink(0.0f);
    page->setGap(getTheme().number("metric.spacing.lg", 20.0f));

    // ---- 基础形态 ----
    Box* basics = makeSection("基础形态");
    makeButtonRow(basics, "基础按钮（base/button/button）", "base/button/button", "基础按钮");
    makeButtonRow(basics, "主按钮", "base/button.primary", "主按钮");
    makeButtonRow(basics, "高亮按钮", "base/button.highlight", "高亮按钮");
    makeButtonRow(basics, "带边框", "base/button.bordered", "带边框");
    makeButtonRow(basics, "无边框", "base/button.borderless", "无边框");
    makeButtonRow(basics, "幽灵", "base/button.ghost", "幽灵按钮");
    page->addView(basics);

    // ---- 图标组合 ----
    Box* icons = makeSection("图标组合");
    makeButtonRow(icons, "纯图标", "widget/button.icon", "",
        bklv::icon::glyphByName("star"));
    makeButtonRow(icons, "图标 + 文字", "widget/button.icon_text", "图标按钮",
        bklv::icon::glyphByName("settings"));
    page->addView(icons);

    // ---- 后续在这里加：状态（focused / disabled）、长文案、一行多按钮、尺寸变体 ----

    return page;
}

} // namespace demo

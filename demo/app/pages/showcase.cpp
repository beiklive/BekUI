#include "app/pages/showcase.hpp"

#include <fstream>
#include <algorithm>
#include <cmath>
#include <sstream>

#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/scroll_box.hpp"
#include "bklv/widgets/section.hpp"

using namespace bklv;

namespace demo
{

std::string readThemeJson(const std::string& relativePath)
{
    std::ifstream file(resourcePath("resources/theme/default/" + relativePath));
    if (!file.is_open())
        return "（读不到 " + relativePath + "）";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

Box* makeColorRect(ImU32 color, float width, float height, float radius)
{
    Box* rect = new Box();
    rect->setWidth(width);
    rect->setHeight(height);
    rect->setBackgroundColor(color);
    rect->setCornerRadius(radius);
    return rect;
}

Box* makeShowcaseSection(const std::string& title, View* demoView, const std::string& jsonText)
{
    Box* section = new Box(Axis::COLUMN);
    section->setWidthPercentage(100.0f);
    section->setGap(getTheme().number("metric.spacing.sm", 8.0f));
    section->addView(makeSectionHeader(title));

    Box* panel = new Box(Axis::ROW);
    panel->setWidthPercentage(100.0f);
    panel->setHeight(300.0f);
    panel->setPadding(24.0f);
    panel->setGap(getTheme().number("metric.spacing.lg", 24.0f));
    panel->setAlignItems(AlignItems::CENTER);
    panel->setBackgroundColor(getTheme().color("color.surface.card"));
    panel->setCornerRadius(getTheme().number("metric.radius.lg", 8.0f));
    panel->setBorderThickness(1.0f);
    panel->setBorderColor(getTheme().color("color.border.default"));

    // 左：演示控件居中
    Box* stage = new Box(Axis::COLUMN);
    stage->setGrow(1.0f);
    stage->setFlexBasis(0.0f);
    stage->setHeightPercentage(100.0f);
    stage->setAlignItems(AlignItems::CENTER);
    stage->setJustifyContent(JustifyContent::CENTER);
    if (demoView != nullptr)
        stage->addView(demoView);
    panel->addView(stage);

    // 右：JSON 原文（可滚动）
    ScrollBox* codeScroll = new ScrollBox(Axis::COLUMN);
    codeScroll->setWidth(420.0f);
    codeScroll->setHeightPercentage(100.0f);
    codeScroll->setBackgroundColor(getTheme().color("color.surface.sunken"));
    codeScroll->setCornerRadius(getTheme().number("metric.radius.md", 5.0f));
    codeScroll->setPadding(12.0f);

    Label* code = new Label();
    code->setText(jsonText);
    code->setWidth(392.0f);
    code->setFontSize(13.0f);
    code->setTextColor(getTheme().color("color.text.dim"));

    // 显式给内容一个高度：ScrollingFrame 只会"内容高度为 0 时"自己累加，
    // 而 Label 在受限高度下量出来的高度会被压扁 → 滚动范围永远是 0。
    // 这里按行数 + 粗略折行估算（代码是等宽-ish 的 JSON，够用）。
    const float fontSize = 13.0f;
    const float lineHeight = fontSize * getTheme().number("metric.font.lineHeight", 1.25f);
    const float charWidth  = fontSize * 0.56f;
    const size_t perLine   = std::max<size_t>(1, (size_t)(392.0f / charWidth));
    float codeHeight = 0.0f;
    size_t begin = 0;
    while (begin <= jsonText.size())
    {
        const size_t end = jsonText.find('\n', begin);
        const size_t len = (end == std::string::npos ? jsonText.size() : end) - begin;
        codeHeight += std::max(1.0f, std::ceil((float)len / (float)perLine)) * lineHeight;
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    codeHeight += fontSize * 2.0f;
    code->setHeight(codeHeight);
    codeScroll->getContentBox()->setHeight(codeHeight);
    codeScroll->addView(code);

    panel->addView(codeScroll);
    section->addView(panel);
    return section;
}

} // namespace demo

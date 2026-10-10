#include "bklv/views/label.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/core/font.hpp"
#include "bklv/core/logger.hpp"

namespace bklv
{

// 解析实际要用的字体。
// 必须在每次测量/绘制时解析，不能在构造时固定：
// 视图树通常先于 App::run() 构建，那时默认字体还没加载。
ImFont* Label::resolveFont() const
{
    if (this->font != nullptr)
        return this->font;

    ImFont* fallback = getDefaultFont();
    if (fallback == nullptr && ImGui::GetCurrentContext() != nullptr)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.Fonts->Fonts.empty())
            fallback = io.Fonts->Fonts[0];
    }

    if (fallback != this->fallbackFont)
    {
        this->fallbackFont = fallback;
        this->cacheValid   = false;
    }
    return fallback;
}

// 计算"…"的宽度时用的字符。
static constexpr const char* ELLIPSIS = "\xE2\x80\xA6";

// 宽度未定义时使用的虚拟上限，避免逐字循环无限展开。
static constexpr float UNBOUNDED_WIDTH = 100000.0f;

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------

Label::Label()
{
    Style style = getDefaultStyle();

    this->fontSize   = style["bklv/label/default_font_size"];
    this->lineHeight = style["bklv/label/default_line_height"];
    this->textColor  = getDefaultTheme()["bklv/text"];

    // 关键一步：把文本测量注册给 Yoga。
    // Yoga 会在布局过程中调用它来决定"这个节点应该多宽多高"。
    this->setMeasureFunction([this](const void*, float width, YGMeasureMode widthMode, float,
                                 YGMeasureMode) -> YGSize {
        const bool widthBound = widthMode != YGMeasureModeUndefined && !std::isnan(width);
        if (this->isSingleLine())
        {
            // 单行：上报文本固有宽度，超出可用宽度时截断到可用宽度。
            const float textWidth  = this->measureWidth(this->text, 0, this->text.size());
            const float textHeight = this->getLinePixelHeight();
            return YGSize { widthBound ? std::min(textWidth, width) : textWidth, textHeight };
        }

        // 多行：把"不换行时的固有宽度"上报给 Yoga，由它决定最终宽度；
        // 高度按可用宽度换行算出来。最终宽度可能比固有宽度小，onLayout 会按它重新排版。
        const float intrinsicWidth = this->measureWidth(this->text, 0, this->text.size());
        const float wrapWidth      = widthBound ? width : UNBOUNDED_WIDTH;

        this->rewrap(wrapWidth);
        return YGSize { intrinsicWidth, this->wrappedHeight };
    });

    // 防止文本溢出父容器。
    YGNodeStyleSetMaxWidthPercent(this->ygNode, 100);
    YGNodeStyleSetMaxHeightPercent(this->ygNode, 100);

    this->invalidate();
}

// ---------------------------------------------------------------------------
// 属性
// ---------------------------------------------------------------------------

void Label::setText(const std::string& value)
{
    if (this->text == value)
        return;

    this->text = value;
    this->textVersion++;
    this->invalidate();
}

void Label::setFont(ImFont* value)
{
    if (this->font == value)
        return;

    this->font = value;
    this->invalidate();
}

void Label::setFontSize(float value)
{
    if (this->fontSize == value)
        return;

    this->fontSize = value;
    this->invalidate();
}

void Label::setTextColor(ImU32 color)
{
    this->textColor = color;
}

void Label::setLineHeight(float value)
{
    if (this->lineHeight == value)
        return;

    this->lineHeight = value;
    this->invalidate();
}

void Label::setSingleLine(bool value)
{
    if (this->singleLine == value)
        return;

    this->singleLine = value;
    this->invalidate();
}

void Label::setHorizontalAlign(HorizontalAlign align)
{
    this->horizontalAlign = align;
}

void Label::setVerticalAlign(VerticalAlign align)
{
    this->verticalAlign = align;
}

// ---------------------------------------------------------------------------
// 测量与排版
// ---------------------------------------------------------------------------

size_t Label::utf8CharLength(const std::string& value, size_t index)
{
    const unsigned char byte = static_cast<unsigned char>(value[index]);
    if (byte < 0x80)
        return 1;
    if ((byte & 0xE0) == 0xC0)
        return 2;
    if ((byte & 0xF0) == 0xE0)
        return 3;
    if ((byte & 0xF8) == 0xF0)
        return 4;
    return 1;
}

float Label::measureWidth(const std::string& value, size_t begin, size_t end) const
{
    ImFont* font = this->resolveFont();
    if (font == nullptr || begin >= end)
        return 0.0f;

    const ImVec2 size = font->CalcTextSizeA(this->fontSize, FLT_MAX, 0.0f, value.c_str() + begin, value.c_str() + end);
    return size.x;
}

float Label::getLinePixelHeight() const
{
    // 用一段包含中文字形的样本测高，避免用 "M" 测出来偏小。
    float glyphHeight = this->fontSize;
    if (ImFont* font = this->resolveFont())
    {
        const ImVec2 size = font->CalcTextSizeA(this->fontSize, FLT_MAX, 0.0f, "汉Ag");
        if (size.y > 0.0f)
            glyphHeight = size.y;
    }
    return std::ceil(glyphHeight * this->lineHeight);
}

bool Label::isCacheValid(float width) const
{
    return this->cacheValid && this->cacheVersion == this->textVersion && std::fabs(this->cacheWidth - width) < 0.5f;
}

void Label::rewrap(float width) const
{
    if (this->isCacheValid(width))
        return;

    this->lines.clear();
    this->wrappedWidth  = 0.0f;
    this->wrappedHeight = 0.0f;

    this->cacheVersion  = this->textVersion;
    this->cacheWidth    = width;
    this->cacheFontSize = this->fontSize;
    this->cacheValid    = true;

    const float linePixelHeight = this->getLinePixelHeight();
    const size_t length         = this->text.size();

    if (length == 0)
    {
        // 空文本仍然占一行高度，避免布局塌成 0。
        this->lines.push_back({ 0, 0 });
        this->wrappedHeight = linePixelHeight;
        return;
    }

    size_t lineBegin = 0;
    size_t index     = 0;

    while (index < length)
    {
        const size_t charLength = utf8CharLength(this->text, index);

        // 主动换行
        if (this->text[index] == '\n')
        {
            this->lines.push_back({ lineBegin, index });
            this->wrappedWidth = std::max(this->wrappedWidth, this->measureWidth(this->text, lineBegin, index));
            index += charLength;
            lineBegin = index;
            continue;
        }

        const float candidateWidth = this->measureWidth(this->text, lineBegin, index + charLength);

        // 超过可用宽度且这一行已经有内容 → 断行
        if (candidateWidth > width && index > lineBegin)
        {
            size_t breakAt = index;

            // 西文优先在空格处断行，避免把单词从中间劈开。
            for (size_t back = index; back > lineBegin; --back)
            {
                if (this->text[back - 1] == ' ')
                {
                    breakAt = back - 1;
                    break;
                }
            }

            this->lines.push_back({ lineBegin, breakAt });
            this->wrappedWidth = std::max(
                this->wrappedWidth, this->measureWidth(this->text, lineBegin, breakAt));

            // 跳掉断点处的空格
            index = breakAt;
            while (index < length && this->text[index] == ' ')
                index++;

            lineBegin = index;
            continue;
        }

        index += charLength;
    }

    if (lineBegin <= length)
    {
        this->lines.push_back({ lineBegin, length });
        this->wrappedWidth = std::max(this->wrappedWidth, this->measureWidth(this->text, lineBegin, length));
    }

    this->wrappedHeight = linePixelHeight * static_cast<float>(this->lines.size());
}

bool Label::isTruncatedCacheValid(float width) const
{
    return this->truncatedValid && this->truncatedVersion == this->textVersion
        && this->truncatedFontSize == this->fontSize && std::fabs(this->truncatedWidth - width) < 0.5f;
}

void Label::updateTruncated(float width) const
{
    if (this->isTruncatedCacheValid(width))
        return;

    this->truncatedValid    = true;
    this->truncatedVersion  = this->textVersion;
    this->truncatedWidth    = width;
    this->truncatedFontSize = this->fontSize;

    if (this->resolveFont() == nullptr || this->text.empty()
        || this->measureWidth(this->text, 0, this->text.size()) <= width)
    {
        this->truncated = this->text;
        return;
    }

    const float ellipsisWidth = this->measureWidth(ELLIPSIS, 0, std::strlen(ELLIPSIS));
    const float budget        = width - ellipsisWidth;

    if (budget <= 0.0f)
    {
        this->truncated = ELLIPSIS;
        return;
    }

    // 逐字累加，找到最后一个能放下的字符位置。
    size_t cutAt   = 0;
    size_t index   = 0;
    float consumed = 0.0f;

    while (index < this->text.size())
    {
        const size_t charLength = utf8CharLength(this->text, index);
        if (index + charLength > this->text.size())
            break;

        const float charWidth = this->measureWidth(this->text, index, index + charLength);
        if (consumed + charWidth > budget)
            break;

        consumed += charWidth;
        index += charLength;
        cutAt = index;
    }

    // 去掉截断处可能残留的空格。
    while (cutAt > 0 && this->text[cutAt - 1] == ' ')
        --cutAt;

    this->truncated = this->text.substr(0, cutAt) + ELLIPSIS;

    // 极端情况下（单字宽于上限）退化为直接省略号。
    if (cutAt == 0)
        this->truncated = ELLIPSIS;
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

void Label::onLayout()
{
    const float width = this->getWidth();
    if (width <= 0.0f)
        return;

    // 布局完成、宽度确定，这里把排版缓存算好，绘制阶段直接使用。
    if (this->isSingleLine())
    {
        this->updateTruncated(width);
        return;
    }

    this->rewrap(width);
}

void Label::draw(ImDrawList* drawList, float x, float y, float width, float height, Style, FrameContext*)
{
    ImFont* font = this->resolveFont();
    if (font == nullptr || this->text.empty() || width <= 0.0f)
        return;

    const ImU32 color          = colorWithAlpha(this->textColor, this->getAlpha());
    const float linePixelHeight = this->getLinePixelHeight();

    // ---- 单行模式：垂直居中，超宽截断 ----
    if (this->singleLine)
    {
        this->updateTruncated(width);

        float textX = x;
        const float textWidth = this->measureWidth(this->truncated, 0, this->truncated.size());
        if (this->horizontalAlign == HorizontalAlign::CENTER)
            textX = x + (width - textWidth) * 0.5f;
        else if (this->horizontalAlign == HorizontalAlign::RIGHT)
            textX = x + width - textWidth;

        const float textY = y + (height - linePixelHeight) * 0.5f;
        drawList->AddText(font, this->fontSize, ImVec2(textX, textY), color, this->truncated.c_str());
        return;
    }

    // ---- 多行模式 ----
    if (!this->isCacheValid(width))
        this->rewrap(width);

    const size_t lineCount = this->lines.size();
    if (lineCount == 0)
        return;

    const float totalHeight = linePixelHeight * static_cast<float>(lineCount);

    float lineTop = y;
    if (this->verticalAlign == VerticalAlign::CENTER)
        lineTop = y + (height - totalHeight) * 0.5f;
    else if (this->verticalAlign == VerticalAlign::BOTTOM)
        lineTop = y + height - totalHeight;

    for (size_t i = 0; i < lineCount; ++i)
    {
        const Line& line = this->lines[i];
        if (line.end <= line.begin)
            continue;

        const float lineWidth = this->measureWidth(this->text, line.begin, line.end);

        float lineX = x;
        if (this->horizontalAlign == HorizontalAlign::CENTER)
            lineX = x + (width - lineWidth) * 0.5f;
        else if (this->horizontalAlign == HorizontalAlign::RIGHT)
            lineX = x + width - lineWidth;

        drawList->AddText(font, this->fontSize, ImVec2(lineX, lineTop + linePixelHeight * static_cast<float>(i)),
            color, this->text.c_str() + line.begin, this->text.c_str() + line.end);
    }
}

} // namespace bklv

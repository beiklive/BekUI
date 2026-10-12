#include "bklv/views/label.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "bklv/core/font.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/theme_data.hpp"

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
static constexpr const char* ELLIPSIS      = "\xE2\x80\xA6";
static constexpr size_t ELLIPSIS_BYTES     = 3; // "…" 的 UTF-8 字节数

// 宽度未定义时使用的虚拟上限，避免逐字循环无限展开。
static constexpr float UNBOUNDED_WIDTH = 100000.0f;

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------

Label::Label()
{
    // 默认值全部来自 JSON 主题（字号/颜色在 applyTheme() 里按"未显式设置"再刷一次）
    this->fontSize   = getTheme().number("metric.font.size.md", this->fontSize);
    this->lineHeight = getTheme().number("metric.font.lineHeight", this->lineHeight);
    this->textColor  = getTheme().color("color.text.primary", this->textColor);

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

    this->fontSize         = value;
    this->fontSizeExplicit = true;
    this->invalidate();
}

void Label::setTextColor(ImU32 color)
{
    this->textColor        = color;
    this->textColorExplicit = true;
}

void Label::applyTheme()
{
    const ThemeData& theme = getTheme();

    // 主题给的是默认值：只填"用户没管过"的那部分
    if (!this->fontSizeExplicit)
    {
        const float size = theme.number("metric.font.size.md", this->fontSize);
        if (size != this->fontSize)
        {
            this->fontSize = size;
            this->markMeasureDirty(); // 字号变了，文本要重新测量
        }
    }

    if (!this->textColorExplicit)
        this->textColor = theme.color("color.text.primary", this->textColor);
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

        // 超过可用宽度且这一行已经有内容 → 断行。
        // 容差 0.5px：可用宽度来自 Yoga、文字宽度来自测量函数，父容器又常把子节点
        // 定成"恰好等于文字宽度"，不留容差时本该一行两字的标题会被拆成两行
        // （侧栏标题"导航"踩过；单行截断当时也是同一个原因）。
        if (candidateWidth > width + 0.5f && index > lineBegin)
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

    // 容差 0.5px：文本宽度和布局宽度是由两条路径算出来的
    // （测量函数 vs Yoga 的布局结果），父容器又常常把子节点定成"恰好等于文字宽度"。
    // 不留容差的话，只要有一点浮点误差就会判定放不下 ——
    // 表现为一段明明够短的文字被截成一个省略号（tab 标题踩过）。
    if (this->resolveFont() == nullptr || this->text.empty()
        || this->measureWidth(this->text, 0, this->text.size()) <= width + 0.5f)
    {
        this->truncated = this->text;
        return;
    }

    const float ellipsisWidth = this->measureWidth(ELLIPSIS, 0, ELLIPSIS_BYTES);
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

// 把一段文字顶点按 fontScale 以锚点为中心缩放（只动字形，不动控件）
void Label::scaleTextVertices(ImDrawList* drawList, int start, float anchorX, float anchorY)
{
    if (this->fontScaleX == 1.0f && this->fontScaleY == 1.0f)
        return;

    for (int i = start; i < drawList->VtxBuffer.Size; i++)
    {
        ImVec2& pos = drawList->VtxBuffer[i].pos;
        pos.x       = anchorX + (pos.x - anchorX) * this->fontScaleX;
        pos.y       = anchorY + (pos.y - anchorY) * this->fontScaleY;
    }
}

void Label::draw(ImDrawList* drawList, float x, float y, float width, float height, FrameContext*)
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
        const int textVtxStart = drawList->VtxBuffer.Size;
        drawList->AddText(font, this->fontSize, ImVec2(textX, textY), color, this->truncated.c_str());
        this->scaleTextVertices(drawList, textVtxStart, textX, textY);
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

        const int textVtxStart = drawList->VtxBuffer.Size;
        drawList->AddText(font, this->fontSize, ImVec2(lineX, lineTop + linePixelHeight * static_cast<float>(i)),
            color, this->text.c_str() + line.begin, this->text.c_str() + line.end);
    }
}


namespace
{

void setTextAttr(View* v, const ResolvedValue& r)
{
    static_cast<Label*>(v)->setText(r.text);
}

void setFontSizeAttr(View* v, const ResolvedValue& r)
{
    static_cast<Label*>(v)->setFontSize(r.number);
}

void setTextColorAttr(View* v, const ResolvedValue& r)
{
    static_cast<Label*>(v)->setTextColor(r.color);
}

void setSingleLineAttr(View* v, const ResolvedValue& r)
{
    static_cast<Label*>(v)->setSingleLine(r.boolean);
}

void setLineHeightAttr(View* v, const ResolvedValue& r)
{
    static_cast<Label*>(v)->setLineHeight(r.number);
}

void setHorizontalAlignAttr(View* v, const ResolvedValue& r)
{
    Label* label = static_cast<Label*>(v);
    if (r.text == "center")
        label->setHorizontalAlign(HorizontalAlign::CENTER);
    else if (r.text == "right")
        label->setHorizontalAlign(HorizontalAlign::RIGHT);
    else
        label->setHorizontalAlign(HorizontalAlign::LEFT);
}

void setVerticalAlignAttr(View* v, const ResolvedValue& r)
{
    Label* label = static_cast<Label*>(v);
    if (r.text == "center")
        label->setVerticalAlign(VerticalAlign::CENTER);
    else if (r.text == "bottom")
        label->setVerticalAlign(VerticalAlign::BOTTOM);
    else
        label->setVerticalAlign(VerticalAlign::TOP);
}

} // namespace

const AttributeTable& Label::attributes() const
{
    static const AttributeTable table = {
        { "text", AttributeKind::Text, setTextAttr, nullptr },
        { "fontSize", AttributeKind::Number, setFontSizeAttr, nullptr },
        { "textColor", AttributeKind::Color, setTextColorAttr, nullptr },
        { "singleLine", AttributeKind::Bool, setSingleLineAttr, nullptr },
        { "lineHeight", AttributeKind::Number, setLineHeightAttr, nullptr },
        { "horizontalAlign", AttributeKind::Enum, setHorizontalAlignAttr, "left|center|right" },
        { "verticalAlign", AttributeKind::Enum, setVerticalAlignAttr, "top|center|bottom" },
    };
    return table;
}

} // namespace bklv

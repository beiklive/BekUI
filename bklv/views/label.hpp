#pragma once

// 文本视图，对应 borealis 的 views/label.hpp。
//
// 渲染后端换成 ImDrawList 后，文本处理需要自己实现：
//   - 换行：按字符逐个测宽，遇到超宽就断行（对中文和西文都有效）
//   - 截断：超宽且不允许换行时，逐步找断点并补 "…"
//   - 缓存：换行结果缓存在成员里，只在文本 / 宽度 / 字号 / 字体变化时重算
// 测量阶段（Yoga measure）只算高度并让 Yoga 决定宽度，绘制阶段直接用缓存。

#include <string>
#include <vector>

#include "bklv/core/view.hpp"
#include "imgui.h"

namespace bklv
{

enum class HorizontalAlign
{
    LEFT,
    CENTER,
    RIGHT,
};

enum class VerticalAlign
{
    TOP,
    CENTER,
    BOTTOM,
};

class Label : public View
{
  public:
    Label();

    void setText(const std::string& text);
    const std::string& getText() const { return this->text; }

    void setFont(ImFont* font);
    ImFont* getFont() const { return this->font; }

    // 解析实际使用的字体：显式设置的优先，否则用全局默认字体。
    ImFont* resolveFont() const;

    void setFontSize(float size);
    float getFontSize() const { return this->fontSize; }

    void setTextColor(ImU32 color);
    ImU32 getTextColor() const { return this->textColor; }

    // 行高倍数，1.0 表示正好一行字高。
    void setLineHeight(float lineHeight);
    float getLineHeight() const { return this->lineHeight; }

    // 单行模式：不换行，超宽时截断并加省略号。
    void setSingleLine(bool singleLine);

    // 文字自身的缩放（borealis demo 的 TransformBox 里那个 fontScale）：
    // 只缩放**字形顶点**，不影响控件的布局与其它绘制。
    // 放在 Label 而不是 View 上，是因为在 frame() 那一层没法区分"文字顶点"和"图形顶点"。
    void setFontScaleX(float value) { this->fontScaleX = value; }
    void setFontScaleY(float value) { this->fontScaleY = value; }
    float getFontScaleX() const { return this->fontScaleX; }
    float getFontScaleY() const { return this->fontScaleY; }
    bool isSingleLine() const { return this->singleLine; }

    void setHorizontalAlign(HorizontalAlign align);
    void setVerticalAlign(VerticalAlign align);

    // 布局完成后由 Yoga 触发：此时才能拿到最终宽度。
    void onLayout() override;

    std::string getClassString() const override { return "bklv::Label"; }

    const bklv::AttributeTable& attributes() const override;

    // 跟随主题：只在没被显式设置过时更新字号与颜色。
    void applyTheme() override;

    // 把字号/颜色的管理权交回主题（或收回）。
    // 默认：调过 setFontSize/setTextColor 就算显式管理，换肤不再覆盖它。
    // 想让某个 Label 跟着主题走，构造后调 setXxxExplicit(false) 即可。
    void setFontSizeExplicit(bool value) { this->fontSizeExplicit = value; }
    void setTextColorExplicit(bool value) { this->textColorExplicit = value; }
    bool isFontSizeExplicit() const { return this->fontSizeExplicit; }
    bool isTextColorExplicit() const { return this->textColorExplicit; }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height, FrameContext* ctx) override;

  private:
    void scaleTextVertices(ImDrawList* drawList, int start, float anchorX, float anchorY);

    struct Line
    {
        size_t begin = 0;
        size_t end   = 0;
    };

    // 返回单个 UTF-8 字符占用的字节数（非法字节按 1 处理）。
    static size_t utf8CharLength(const std::string& value, size_t index);

    // 单行宽度测量。
    float measureWidth(const std::string& value, size_t begin, size_t end) const;

    // 实际像素行距。
    float getLinePixelHeight() const;

    // 排版缓存是否仍然有效。
    bool isCacheValid(float width) const;

    // 重算排版：换行结果写入 lines，同时算出 wrappedWidth / wrappedHeight。
    void rewrap(float width) const;

    // 单行截断结果。
    void updateTruncated(float width) const;
    bool isTruncatedCacheValid(float width) const;

    std::string text;
    size_t textVersion = 0;

    ImFont* font    = nullptr;
    // applyTheme() 只覆盖**没被显式设置过**的值：
    // 用户代码 setFontSize(18) 之后，换肤不该把它冲掉。
    float fontSize        = 20.0f;
    float fontScaleX      = 1.0f;
    float fontScaleY      = 1.0f;
    ImU32 textColor       = IM_COL32(235, 238, 242, 255);
    bool fontSizeExplicit = false;
    bool textColorExplicit = false;
    float lineHeight = 1.35f;
    bool singleLine  = false;

    HorizontalAlign horizontalAlign = HorizontalAlign::LEFT;
    VerticalAlign verticalAlign     = VerticalAlign::TOP;

    // ---- 可变的排版缓存 ----
    mutable size_t cacheVersion    = 0;
    mutable float cacheWidth       = -1.0f;
    mutable float cacheFontSize    = 0.0f;
    mutable bool cacheValid        = false;
    mutable std::vector<Line> lines;
    mutable float wrappedWidth     = 0.0f;
    mutable float wrappedHeight    = 0.0f;

    mutable bool truncatedValid    = false;
    mutable size_t truncatedVersion = 0;
    mutable float truncatedWidth   = -1.0f;
    mutable float truncatedFontSize = 0.0f;
    mutable std::string truncated;

    // 惰性解析出来的字体，默认字体变化时会清空排版缓存。
    mutable ImFont* fallbackFont = nullptr;
};

} // namespace bklv

#include "bklv/views/debug_log_view.hpp"

#include <algorithm>
#include <cmath>

#include "imgui.h"

#include "bklv/core/app.hpp"
#include "bklv/core/font.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

namespace
{

constexpr unsigned levelBit(LogLevel level)
{
    return 1u << static_cast<unsigned>(level);
}

const char* levelTag(LogLevel level)
{
    switch (level)
    {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO ";
        case LogLevel::WARNING: return "WARN ";
        case LogLevel::ERROR: return "ERROR";
    }
    return "INFO ";
}

} // namespace

DebugLogView::DebugLogView()
{
    // 贴右上角：宽度/高度用百分比，跟随画布；像素偏移不缩放（约定：代码里写死的像素不缩放）
    this->setPositionType(PositionType::ABSOLUTE);
    this->setPositionTop(getTheme().number("metric.spacing.lg", 16.0f));
    this->setPositionRight(getTheme().number("metric.spacing.lg", 16.0f));
    this->setWidthPercentage(48.0f);
    this->setHeightPercentage(42.0f);

    // 默认关闭（宿主/按钮调 setEnabled 打开）
    this->setVisibility(Visibility::GONE);

    this->applyTheme();
}

// ---------------------------------------------------------------------------
// 主题
// ---------------------------------------------------------------------------

void DebugLogView::applyTheme()
{
    const ThemeData& theme = getTheme();

    this->fontSize     = theme.number("metric.font.size.sm", 15.0f);
    this->lineHeight   = theme.number("metric.font.lineHeight", 1.4f);
    this->padding      = theme.number("metric.spacing.md", 12.0f);
    this->cornerRadius = theme.number("metric.radius.md", 5.0f);

    this->panelColor  = theme.color("color.surface.card");
    this->borderColor = theme.color("color.common.divider");
    this->headColor   = theme.color("color.text.dim");
    this->debugColor  = theme.color("color.text.dim");
    this->infoColor   = theme.color("color.text.primary");
    this->warnColor   = theme.color("color.common.yellow");
    this->errorColor  = theme.color("color.common.red");

    this->invalidateRows();
}

// ---------------------------------------------------------------------------
// 级别过滤
// ---------------------------------------------------------------------------

void DebugLogView::setLevelVisible(LogLevel level, bool visible)
{
    const unsigned bit = levelBit(level);
    const unsigned before = this->levelMask;

    if (visible)
        this->levelMask |= bit;
    else
        this->levelMask &= ~bit;

    if (this->levelMask != before)
        this->invalidateRows();
}

bool DebugLogView::isLevelVisible(LogLevel level) const
{
    return (this->levelMask & levelBit(level)) != 0;
}

void DebugLogView::setEnabled(bool enabled)
{
    this->setVisibility(enabled ? Visibility::VISIBLE : Visibility::GONE);
}

// ---------------------------------------------------------------------------
// 滚动
// ---------------------------------------------------------------------------

float DebugLogView::getRowsHeight() const
{
    // 头部状态行 + 一条分隔线的占位（和 draw 里的算法保持一致）
    return this->lineHeight * this->fontSize + 8.0f;
}

float DebugLogView::getContentWidth() const
{
    const float width = this->getWidth() - this->padding * 2.0f;
    return width > 0.0f ? width : 0.0f;
}

float DebugLogView::getMaxScroll() const
{
    const float viewHeight = this->getHeight() - this->getRowsHeight() - this->padding * 2.0f;
    if (viewHeight <= 0.0f)
        return 0.0f;

    return std::max(0.0f, this->totalHeight - viewHeight);
}

void DebugLogView::scrollToBottom()
{
    this->autoScroll   = true;
    this->scrollOffset = this->getMaxScroll();
}

void DebugLogView::scrollBy(int lines)
{
    const float step = this->lineHeight * this->fontSize;
    this->setScrollOffset(this->scrollOffset + step * static_cast<float>(lines));
}

void DebugLogView::setScrollOffset(float offset)
{
    const float maxScroll = this->getMaxScroll();
    this->scrollOffset    = std::clamp(offset, 0.0f, maxScroll);

    // 手动滚到最底下就恢复自动跟随；否则暂停（否则新日志会把视角拽走）
    this->autoScroll = this->scrollOffset >= maxScroll - 0.5f;
}

// ---------------------------------------------------------------------------
// 行缓存
// ---------------------------------------------------------------------------

std::string DebugLogView::buildRowText(const LogEntry& entry) const
{
    std::string text;
    text.reserve(entry.time.size() + entry.location.size() + entry.message.size() + 16);

    if (this->showTime)
    {
        text += entry.time;
        text += "  ";
    }

    text += levelTag(entry.level);
    text += "  ";

    if (this->showLocation && !entry.location.empty())
    {
        text += entry.location;
        text += "  ";
    }

    text += entry.message;
    return text;
}

ImU32 DebugLogView::colorForLevel(LogLevel level) const
{
    switch (level)
    {
        case LogLevel::DEBUG: return this->debugColor;
        case LogLevel::INFO: return this->infoColor;
        case LogLevel::WARNING: return this->warnColor;
        case LogLevel::ERROR: return this->errorColor;
    }
    return this->infoColor;
}

// 一行文本按 wrapWidth 会折成几行。没有可用字体（无窗口测试）时按 1 行算。
int DebugLogView::measureLineCount(const std::string& text, float wrapWidth) const
{
    if (wrapWidth <= 1.0f || ImGui::GetCurrentContext() == nullptr)
        return 1;

    ImFont* font = getDefaultFont();
    if (font == nullptr)
        return 1;

    const float single = font->CalcTextSizeA(this->fontSize, FLT_MAX, 0.0f, "Ag").y;
    if (single <= 0.0f)
        return 1;

    const ImVec2 wrapped =
        font->CalcTextSizeA(this->fontSize, FLT_MAX, wrapWidth, text.c_str(), nullptr);
    const int lines = static_cast<int>(std::ceil(wrapped.y / single));
    return std::max(1, lines);
}

void DebugLogView::rebuildRowsIfNeeded()
{
    const float wrapWidth = this->getContentWidth();

    const uint64_t version = Logger::getVersion();
    if (!this->rowsDirty && version == this->cachedVersion && this->cachedFilter == this->levelMask
        && this->cachedWidth == wrapWidth && this->cachedFontSize == this->fontSize)
        return;

    this->cachedVersion  = version;
    this->cachedFilter   = this->levelMask;
    this->cachedWidth    = wrapWidth;
    this->cachedFontSize = this->fontSize;
    this->rowsDirty      = false;
    this->rebuildCount++;

    const float rowHeight = this->lineHeight * this->fontSize;

    this->rows.clear();
    for (const LogEntry& entry : Logger::getLogs())
    {
        if (!this->isLevelVisible(entry.level))
            continue;

        Row row;
        row.text      = this->buildRowText(entry);
        row.color     = this->colorForLevel(entry.level);
        row.lineCount = this->measureLineCount(row.text, wrapWidth);
        row.height    = rowHeight * static_cast<float>(row.lineCount);
        this->rows.push_back(std::move(row));
    }

    this->totalHeight = 0.0f;
    for (const Row& row : this->rows)
        this->totalHeight += row.height;

    // 新日志到达时贴着底部；关掉自动跟随就保持用户的位置（只做边界裁剪）
    if (this->autoScroll)
        this->scrollOffset = this->getMaxScroll();
    else
        this->setScrollOffset(this->scrollOffset);
}

// ---------------------------------------------------------------------------
// 每帧
// ---------------------------------------------------------------------------

void DebugLogView::update()
{
    View::update();

    this->rebuildRowsIfNeeded();

    // 鼠标滚轮：调试面板不吃焦点，所以自己判"指针在不在面板里"。
    // 读 ImGui 状态前先确认有上下文（无窗口测试没有），和 ScrollingFrame 同一约定。
    if (ImGui::GetCurrentContext() != nullptr)
    {
        ImGuiIO& io    = ImGui::GetIO();
        const Rect frame = this->getFrame();
        if (frame.contains(Point(io.MousePos.x, io.MousePos.y)) && io.MouseWheel != 0.0f)
        {
            const float step = this->lineHeight * this->fontSize * 3.0f;
            this->setScrollOffset(this->scrollOffset - io.MouseWheel * step);
        }
    }

    if (this->autoScroll)
        this->scrollOffset = this->getMaxScroll();
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

void DebugLogView::draw(ImDrawList* drawList, float x, float y, float width, float height,
    FrameContext* ctx)
{
    (void)ctx;
    if (drawList == nullptr)
        return;

    this->rebuildRowsIfNeeded();

    const ImVec2 min(x, y);
    const ImVec2 max(x + width, y + height);

    drawList->AddRectFilled(min, max, this->panelColor, this->cornerRadius);
    drawList->AddRect(min, max, this->borderColor, this->cornerRadius);

    ImFont* font  = getDefaultFont();
    const float fontSize = this->fontSize;
    const float rowHeight = this->lineHeight * fontSize;

    // ---- 状态行 ----
    const InputManager& input = getInputManager();
    View* focus               = input.getCurrentFocus();

    std::string focusName = "(none)";
    if (focus != nullptr)
    {
        focusName = focus->getClassString();
        const size_t pos = focusName.find("::");
        if (pos != std::string::npos)
            focusName = focusName.substr(pos + 2);
    }

    char header[256];
    const float fps = ImGui::GetCurrentContext() != nullptr ? ImGui::GetIO().Framerate : 0.0f;
    std::snprintf(header, sizeof(header), "fps %.0f   布局 %u   日志 %zu/%zu   %s%s%s%s   焦点 %s",
        (double)fps, App::getLayoutCount(), this->rows.size(), this->getLogCount(),
        this->isLevelVisible(LogLevel::DEBUG) ? "D" : "-",
        this->isLevelVisible(LogLevel::INFO) ? "I" : "-",
        this->isLevelVisible(LogLevel::WARNING) ? "W" : "-",
        this->isLevelVisible(LogLevel::ERROR) ? "E" : "-", focusName.c_str());

    const float headY = y + this->padding;
    if (font != nullptr)
        drawList->AddText(font, fontSize, ImVec2(x + this->padding, headY), this->headColor, header);

    // ---- 行区 ----
    const float rowsTop    = headY + rowHeight + 6.0f;
    const float rowsBottom = max.y - this->padding;
    if (rowsBottom <= rowsTop)
        return;

    const float wrapWidth = std::max(1.0f, width - this->padding * 2.0f);
    drawList->PushClipRect(ImVec2(x + this->padding, rowsTop), ImVec2(max.x - this->padding, rowsBottom), true);

    float rowY = rowsTop - this->scrollOffset;
    for (const Row& row : this->rows)
    {
        if (rowY + row.height < rowsTop)
        {
            rowY += row.height;
            continue;
        }
        if (rowY > rowsBottom)
            break;

        if (font != nullptr)
            drawList->AddText(font, fontSize, ImVec2(x + this->padding, rowY), row.color,
                row.text.c_str(), nullptr, wrapWidth);

        rowY += row.height;
    }

    drawList->PopClipRect();

    // 没有日志时给一句提示，免得看着像坏了
    if (this->rows.empty() && font != nullptr)
    {
        drawList->AddText(font, fontSize, ImVec2(x + this->padding, rowsTop), this->headColor,
            "(暂无日志)");
    }
}

} // namespace bklv

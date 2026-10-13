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

    // 字号取主题里**最小**的那个（日志面板要尽量小、尽量多放几行）
    this->fontSize     = theme.hasNumber("metric.font.size.xs")
        ? theme.number("metric.font.size.xs")
        : theme.number("metric.font.size.sm", 15.0f);
    this->lineHeight   = theme.number("metric.font.lineHeight", 1.4f);
    this->padding      = theme.number("metric.spacing.md", 12.0f);
    this->cornerRadius = theme.number("metric.radius.md", 5.0f);

    // 半透明：压着内容也看得见日志，同时还能看到底下的界面
    this->panelColor  = colorWithAlpha(theme.color("color.surface.card"), 0.78f);
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
    return this->getRowHeight() + 8.0f;
}

// 面板能放下几行（头部状态行 + 上下内边距都扣掉；至少 1 行）
size_t DebugLogView::getVisibleRowCount() const
{
    const float rowsHeight = this->getHeight() - this->getRowsHeight() - this->padding * 2.0f;
    if (rowsHeight <= 0.0f)
        return 1;

    const float rowHeight = this->getRowHeight();
    if (rowHeight <= 0.0f)
        return 1;

    const size_t count = static_cast<size_t>(rowsHeight / rowHeight);
    return count > 0 ? count : 1;
}

// 不滚动 → 可见窗口永远是"最后 N 行"
size_t DebugLogView::getFirstVisibleRow() const
{
    const size_t visible = this->getVisibleRowCount();
    return this->rows.size() > visible ? this->rows.size() - visible : 0;
}

bool DebugLogView::isShowingLatest() const
{
    return this->getFirstVisibleRow() + this->getVisibleRowCount() >= this->rows.size();
}

// ---------------------------------------------------------------------------
// 行缓存
// ---------------------------------------------------------------------------

// 前缀（时间 + 级别 + 位置）。写进调用方给的缓冲（复用，不重新分配）
void DebugLogView::buildRowPrefix(std::string& out, const LogEntry& entry) const
{
    out.clear();

    if (this->showTime)
    {
        out += entry.time;
        out += "  ";
    }

    out += levelTag(entry.level);
    out += "  ";

    if (this->showLocation && !entry.location.empty())
    {
        out += entry.location;
        out += "  ";
    }
}

void DebugLogView::buildRowText(std::string& out, const LogEntry& entry) const
{
    this->buildRowPrefix(out, entry);
    out += entry.message;
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

void DebugLogView::rebuildRowsIfNeeded()
{
    const uint64_t version = Logger::getVersion();
    ImFont* font = ImGui::GetCurrentContext() != nullptr ? getDefaultFont() : nullptr;

    if (!this->rowsDirty && version == this->cachedVersion && this->cachedFilter == this->levelMask
        && this->cachedFontSize == this->fontSize && this->cachedFont == font)
        return;

    this->cachedVersion  = version;
    this->cachedFilter   = this->levelMask;
    this->cachedFontSize = this->fontSize;
    this->cachedFont     = font; // 字体换了前缀宽度要重新量
    this->rowsDirty      = false;
    this->rebuildCount++;

    // 只存**指针**（不复制日志正文：环形缓冲里的元素在 push/pop 两端都不会失效），
    // 换个说法：这一步是 O(条数) 的指针拷贝，不是 O(条数 × 字符串长度) 的字符串拷贝。

    this->rows.clear();
    this->rows.reserve(Logger::getLogs().size());

    for (const LogEntry& entry : Logger::getLogs())
    {
        if (!this->isLevelVisible(entry.level))
            continue;

        Row row;
        row.entry  = &entry;
        row.color  = this->colorForLevel(entry.level);
        row.prefixWidth = 0.0f;

        if (font != nullptr)
        {
            this->buildRowPrefix(this->scratchPrefix, entry);
            row.prefixWidth = font->CalcTextSizeA(this->fontSize, FLT_MAX, 0.0f,
                this->scratchPrefix.c_str()).x;
        }

        this->rows.push_back(row);
    }

    // 一行一条、不换行、不滚动：画的时候直接取"最后 N 行"，这里不用算总高
}

// ---------------------------------------------------------------------------
// 每帧
// ---------------------------------------------------------------------------

void DebugLogView::update()
{
    View::update();

    // 面板不可见时什么都不用做（View::frame 也直接返回，一帧零 draw call）
    if (!this->isEnabled())
        return;

    // 不支持滚动：这里只需要维护"最新那批行"的缓存
    this->rebuildRowsIfNeeded();
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

void DebugLogView::draw(ImDrawList* drawList, float x, float y, float width, float height,
    FrameContext* ctx)
{
    (void)ctx;
    if (drawList == nullptr || !this->isEnabled())
        return;

    // update() 每帧都会重建（版本没变就是一次比较），这里只兜底"宿主没调 update"的情况
    this->rebuildRowsIfNeeded();

    const ImVec2 min(x, y);
    const ImVec2 max(x + width, y + height);

    drawList->AddRectFilled(min, max, this->panelColor, this->cornerRadius);
    drawList->AddRect(min, max, this->borderColor, this->cornerRadius);

    ImFont* font          = getDefaultFont();
    const float fontSize  = this->fontSize;
    const float rowHeight = this->getRowHeight();

    // ---- 状态行：内容没变就复用上一帧的字符串（fps 取整，变化才重拼）----
    const InputManager& input = getInputManager();
    View* focus               = input.getCurrentFocus();
    const int fps             = ImGui::GetCurrentContext() != nullptr
        ? static_cast<int>(ImGui::GetIO().Framerate + 0.5f)
        : 0;
    const unsigned layouts    = App::getLayoutCount();
    const size_t logCount     = this->getLogCount();

    if (this->headerText.empty() || this->headerVersion != this->cachedVersion
        || this->headerFilter != this->levelMask || this->headerLogCount != logCount
        || this->headerFps != fps || this->headerLayouts != layouts
        || this->headerFocus != focus)
    {
        std::string focusName = "(none)";
        if (focus != nullptr)
        {
            focusName = focus->getClassString();
            const size_t pos = focusName.find("::");
            if (pos != std::string::npos)
                focusName = focusName.substr(pos + 2);
        }

        char header[256];
        std::snprintf(header, sizeof(header),
            "fps %d   布局 %u   日志 %zu/%zu   %s%s%s%s   焦点 %s", fps, layouts,
            this->rows.size(), logCount, this->isLevelVisible(LogLevel::DEBUG) ? "D" : "-",
            this->isLevelVisible(LogLevel::INFO) ? "I" : "-",
            this->isLevelVisible(LogLevel::WARNING) ? "W" : "-",
            this->isLevelVisible(LogLevel::ERROR) ? "E" : "-", focusName.c_str());

        this->headerText     = header;
        this->headerVersion  = this->cachedVersion;
        this->headerFilter   = this->levelMask;
        this->headerLogCount = logCount;
        this->headerFps      = fps;
        this->headerLayouts  = layouts;
        this->headerFocus    = focus;
    }

    const float headY = y + this->padding;
    if (font != nullptr)
        drawList->AddText(font, fontSize, ImVec2(x + this->padding, headY), this->headColor,
            this->headerText.c_str());

    // ---- 行区 ----
    const float rowsTop    = headY + rowHeight + 6.0f;
    const float rowsBottom = max.y - this->padding;
    if (rowsBottom <= rowsTop)
        return;

    if (this->rows.empty())
    {
        if (font != nullptr)
            drawList->AddText(font, fontSize, ImVec2(x + this->padding, rowsTop),
                this->headColor, "(暂无日志)");
        return;
    }

    // 不滚动 → 可见窗口就是"最后 N 行"（N 由面板高度除行高算出来）
    const float textX     = x + this->padding;
    const float clipRight = max.x - this->padding;

    const size_t visible = this->getVisibleRowCount();
    size_t first         = this->rows.size() > visible ? this->rows.size() - visible : 0;

    // 不换行：超出面板宽度的部分直接裁掉（PushClipRect 管住左右）
    drawList->PushClipRect(ImVec2(textX, rowsTop), ImVec2(clipRight, rowsBottom), true);

    float rowY = rowsTop;
    for (size_t i = first; i < this->rows.size(); i++)
    {
        if (rowY > rowsBottom)
            break;

        const Row& row = this->rows[i];
        if (row.entry != nullptr && font != nullptr)
        {
            // 前缀（暗色）+ 正文（级别色）：只拼进复用缓冲，稳态零分配
            this->buildRowPrefix(this->scratchPrefix, *row.entry);

            if (row.prefixWidth > 0.0f)
                drawList->AddText(font, fontSize, ImVec2(textX, rowY), this->headColor,
                    this->scratchPrefix.c_str());

            this->scratchText.clear();
            this->scratchText += row.entry->message;
            drawList->AddText(font, fontSize, ImVec2(textX + row.prefixWidth, rowY), row.color,
                this->scratchText.c_str());
        }

        rowY += rowHeight;
    }

    drawList->PopClipRect();
}

} // namespace bklv

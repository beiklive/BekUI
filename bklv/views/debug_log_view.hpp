#pragma once

// 调试日志视图：把 `Logger` 环形缓冲里的**全部**日志画出来。
//
// 取代旧实现 `views/debug_window.*`（ImGui 窗口版，已删除）。三条区别：
//
//   1. 它是**视图树里的一个 View**：挂到 `Shell::getDebugLayer()`（在 HudLayer 之上、
//      永远最后绘制），所以它跟着视图树一起画，不再依赖 `App::onDrawWindows`。
//   2. **不为每条日志建视图**：borealis 的 DebugLayer 每条日志 new 一个 Box + Label，
//      日志一多就不停触发 Yoga 重排。这里直接用 ImDrawList 画文本 → **零重排**。
//   3. 行只在「日志版本 / 级别过滤 / 面板内容宽度 / 字号」变化时重建缓存，
//      其余帧只按缓存画可见区间。
//
// 面板默认贴在右上角（宽 48%、高 42%），**默认关闭**；`setEnabled(true)` 打开。
// 它不可聚焦、也不接受点击（父链上的调试层不是可聚焦容器，`hitTest` 到不了它），
// 所以不会影响焦点导航；鼠标滚轮在面板范围内可以直接滚动日志。

#include <cstdint>
#include <string>
#include <vector>

#include "bklv/core/logger.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

class DebugLogView : public View
{
  public:
    DebugLogView();

    // 显隐（GONE 不占位也不绘制）。默认关闭。
    void setEnabled(bool enabled);
    bool isEnabled() const { return this->getVisibility() == Visibility::VISIBLE; }
    void toggle() { this->setEnabled(!this->isEnabled()); }

    // ---- 级别过滤（默认全开）----
    void setLevelVisible(LogLevel level, bool visible);
    bool isLevelVisible(LogLevel level) const;

    // 每行前面带哪些列
    void setShowTime(bool show) { this->showTime = show; this->invalidateRows(); }
    bool isShowTime() const { return this->showTime; }
    void setShowLocation(bool show) { this->showLocation = show; this->invalidateRows(); }
    bool isShowLocation() const { return this->showLocation; }

    // ---- 滚动 ----
    void setAutoScroll(bool enabled) { this->autoScroll = enabled; }
    bool isAutoScroll() const { return this->autoScroll; }
    // 滚到最新一条
    void scrollToBottom();
    // 按"行"滚动（正数 = 看更新的日志）；滚到底部会重新打开自动跟随
    void scrollBy(int lines);
    void setScrollOffset(float offset);
    float getScrollOffset() const { return this->scrollOffset; }
    // 内容总高（所有可见行的高度和）与最大可滚动量
    float getContentHeight() const { return this->totalHeight; }
    float getMaxScroll() const;

    // ---- 读取/清理 ----
    // 缓存里有多少行（= 通过过滤的日志条数）
    size_t getCachedRowCount() const { return this->rows.size(); }
    // 第 index 行画出来的文本（越界返回空串；排查/测试用）
    std::string getRowText(size_t index) const
    {
        return index < this->rows.size() ? this->rows[index].text : std::string();
    }
    // 行缓存重建过几次（诊断用：日志再多，版本次数决定它，面板本身不触发重排）
    uint64_t getRebuildCount() const { return this->rebuildCount; }
    // 缓冲里一共多少条（不过滤）
    size_t getLogCount() const { return Logger::getLogs().size(); }
    void clearLogs() { Logger::clear(); }

    void applyTheme() override;
    void update() override;

    // 调试面板不吃焦点：不进焦点链、不参与方向导航
    bool isFocusable() const override { return false; }

    std::string getClassString() const override { return "bklv::DebugLogView"; }

  protected:
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        FrameContext* ctx) override;

  private:
    struct Row
    {
        std::string text;
        ImU32 color     = 0;
        float height    = 0.0f;
        int lineCount   = 1;
    };

    // 缓存失效 + 重建（只在需要时做，绝大多数帧是空转）
    void invalidateRows() { this->rowsDirty = true; }
    void rebuildRowsIfNeeded();
    float getRowsHeight() const;
    float getContentWidth() const;
    std::string buildRowText(const LogEntry& entry) const;
    ImU32 colorForLevel(LogLevel level) const;
    int measureLineCount(const std::string& text, float wrapWidth) const;

    std::vector<Row> rows;
    bool rowsDirty = true;
    uint64_t rebuildCount = 0;

    // 缓存条件
    uint64_t cachedVersion   = 0;
    unsigned cachedFilter    = 0;
    float cachedWidth        = -1.0f;
    float cachedFontSize     = -1.0f;

    // 过滤（按 LogLevel 的下标存一位）
    unsigned levelMask = 0x0Fu;

    bool showTime     = true;
    bool showLocation = true;
    bool autoScroll   = true;

    float scrollOffset = 0.0f;
    float totalHeight  = 0.0f;

    // 主题值（applyTheme 时取好，draw 只读）
    float fontSize    = 15.0f;
    float lineHeight  = 1.4f;
    float padding     = 12.0f;
    float cornerRadius = 6.0f;
    ImU32 panelColor  = 0;
    ImU32 borderColor = 0;
    ImU32 headColor   = 0;
    ImU32 debugColor  = 0;
    ImU32 infoColor   = 0;
    ImU32 warnColor   = 0;
    ImU32 errorColor  = 0;
};

} // namespace bklv

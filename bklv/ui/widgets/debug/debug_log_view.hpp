#pragma once

// 调试日志视图：把 `Logger` 环形缓冲里的**全部**日志画出来。
//
// 取代旧实现 `views/debug_window.*`（ImGui 窗口版，已删除）。三条区别：
//
//   1. 它是**视图树里的一个 View**：挂到 `Shell::getDebugLayer()`（在 HudLayer 之上、
//      永远最后绘制），所以它跟着视图树一起画，不再依赖 `App::onDrawWindows`。
//   2. **不为每条日志建视图**：borealis 的 DebugLayer 每条日志 new 一个 Box + Label，
//      日志一多就不停触发 Yoga 重排。这里直接用 ImDrawList 画文本 → **零重排**。
//   3. 行只在「日志版本 / 级别过滤 / 字号」变化时重建缓存，其余帧只按缓存画可见区间。
//
// 三条显示约定（用户要求）：
//   · 面板**半透明**（底色是 surface.card 压了 alpha），压着内容也能看清字
//   · 一行一条日志，**不换行**（超宽直接被裁剪；要看全就把面板拉宽）
//   · 字号取主题里**最小**的字号（`metric.font.size.xs`，没有就退到 sm）
//   · **不支持滚动**：永远显示最新的一批（"即时显示"）。要翻历史就开文件输出去文件里看，
//     面板只负责"现在发生了什么"，所以没有 scrollTo/scrollBy/滚轮那一套。
//
// **绘制开销（这是硬要求：日志再多也不能拖慢主循环）**
//   · 缓存里只存 `const LogEntry*`（**不复制字符串**），只在「日志版本 / 过滤 / 字号 / 字体」
//     变化时重建一次；同一帧里来 1000 条也只重建一次。
//   · 行高固定 + 不滚动 → 可见区间就是"最后 N 行"，一次除法/比较算出来（不是从头遍历），
//     只画屏幕里那十几行。
//   · 每行只调一次 `AddText`（前缀 + 正文先进一个**复用缓冲**，容量只增不减 → 稳态零分配）。
//   · 头部状态行同理：内容没变就复用上一帧的字符串。
//   · 面板不可见时 `View::frame` 直接返回，一帧一个 draw call 都不产生。
//
// 面板默认贴在右上角（宽 48%、高 42%），**默认关闭**；`setEnabled(true)` 打开。
// 它不可聚焦、也不接受点击（父链上的调试层不是可聚焦容器，`hitTest` 到不了它），
// 所以不会影响焦点导航；鼠标滚轮在面板范围内可以直接滚动日志。

#include <cstdint>
#include <string>
#include <vector>

#include "bklv/foundation/core/logger.hpp"
#include "bklv/ui/view/view.hpp"

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

    // ---- "即时显示"的可见窗口（**没有滚动**：永远是最后一批）----
    // 面板高度能放下的行数（至少 1 行；头部状态行与内边距都扣掉）
    size_t getVisibleRowCount() const;
    // 当前从第几行开始画（= 总行数 - 可见行数，所以永远贴着最新）
    size_t getFirstVisibleRow() const;
    // 最新一行是否在可见窗口里（不支持滚动，所以这条永远成立）
    bool isShowingLatest() const;

    // ---- 读取/清理 ----
    // 缓存里有多少行（= 通过过滤的日志条数）
    size_t getCachedRowCount() const { return this->rows.size(); }
    // 第 index 行的文本（越界返回空串）。**按需拼**：绘制路径不调它，
    // 所以缓存里不用为每条日志存一份字符串。
    std::string getRowText(size_t index) const
    {
        if (index >= this->rows.size() || this->rows[index].entry == nullptr)
            return std::string();

        std::string text;
        this->buildRowText(text, *this->rows[index].entry);
        return text;
    }
    // 行缓存重建过几次（诊断用：日志再多，版本次数决定它，面板本身不触发重排）
    uint64_t getRebuildCount() const { return this->rebuildCount; }
    // 当前用的字号（主题里最小的那个）与固定行高
    float getFontSize() const { return this->fontSize; }
    float getRowHeightPx() const { return this->getRowHeight(); }
    // 面板底色（半透明，压着内容也看得见字）
    ImU32 getPanelColor() const { return this->panelColor; }
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
    // 一行 = 一条日志的指针 + 前缀宽度（前缀 = 时间/级别/位置，正文接在它后面画）
    struct Row
    {
        const LogEntry* entry = nullptr;
        ImU32 color           = 0;
        float prefixWidth     = 0.0f;
    };

    // 缓存失效 + 重建（只在需要时做，绝大多数帧是空转）
    void invalidateRows() { this->rowsDirty = true; }
    void rebuildRowsIfNeeded();
    // 复用缓冲：前缀 + 正文拼进去（clear() 不释放容量 → 稳态不分配）
    void buildRowText(std::string& out, const LogEntry& entry) const;
    // 前缀（时间 + 级别 + 位置）单独拼一次，用来量宽度
    void buildRowPrefix(std::string& out, const LogEntry& entry) const;
    // 头部状态行 + 分隔线占的高度
    float getRowsHeight() const;
    // 一行的行高（固定：不换行，所以每行一样高）
    float getRowHeight() const { return this->lineHeight * this->fontSize; }
    ImU32 colorForLevel(LogLevel level) const;

    std::vector<Row> rows;
    bool rowsDirty = true;
    uint64_t rebuildCount = 0;

    // 缓存条件
    uint64_t cachedVersion = 0;
    unsigned cachedFilter  = 0;
    float cachedFontSize   = -1.0f;
    const ImFont* cachedFont = nullptr; // 字体换了要重新量前缀宽度

    // 头部状态行的缓存（内容没变就不重新拼）
    std::string headerText;
    uint64_t headerVersion = 0;
    unsigned headerFilter  = 0;
    size_t headerLogCount  = 0;
    int headerFps          = -1;
    unsigned headerLayouts = 0;
    const View* headerFocus = nullptr;

    // 拼行的复用缓冲（稳态零分配）
    mutable std::string scratchPrefix;
    mutable std::string scratchText;

    // 过滤（按 LogLevel 的下标存一位）
    unsigned levelMask = 0x0Fu;

    bool showTime     = true;
    bool showLocation = true;

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

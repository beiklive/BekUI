// 无窗口测试：调试日志视图（views/debug_log_view.*）+ 外壳调试层。
//
// 取代旧的 ImGui 版日志窗口（views/debug_window.*，已删除）。要盯住的四件事：
//   1. 它把 Logger 环形缓冲里的**全部**日志都变成行（级过滤只影响行集合）
//   2. 行缓存只在「日志版本 / 过滤 / 面板宽度 / 字号」变化时重建 —— 日志再多也不重排
//   3. 它**不吃输入、不进焦点链**（挂在外壳调试层上时，点它那里照样命中下面的控件）
//   4. 调试层排在 HudLayer 之后（永远最后绘制 = 在所有层之上）

#include <cstdio>
#include <string>

#include <yoga/Yoga.h>

#include "bklv/core/appearance.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/view.hpp"
#include "bklv/views/debug_log_view.hpp"
#include "bklv/widgets/shell.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

class FocusBox : public Box
{
  public:
    explicit FocusBox(Axis axis = Axis::COLUMN)
        : Box(axis)
    {
    }

    bool isFocusable() const override { return true; }

    std::string getClassString() const override { return "FocusBox"; }
};

void layoutTree(View* root, float width, float height)
{
    YGNodeCalculateLayout(root->getYGNode(), width, height, YGDirectionLTR);
    root->notifyLayout();
}

// 面板要有个尺寸才谈得上滚动/换行；这里直接给一个确定的大小
void layoutPanel(DebugLogView* view, float width = 600.0f, float height = 400.0f)
{
    view->setVisibility(Visibility::VISIBLE);
    view->setWidth(width);
    view->setHeight(height);
    layoutTree(view, width, height);
}

int indexIn(const Box* layer, const View* view)
{
    const std::vector<View*>& children = layer->getChildren();
    for (size_t i = 0; i < children.size(); i++)
        if (children[i] == view)
            return (int)i;
    return -1;
}

} // namespace

int main()
{
    bklv::initResources();
    setTheme("default");

    std::printf("\n== 1. 全部日志都变成行 ==\n");
    {
        DebugLogView* view = new DebugLogView();
        check(!view->isEnabled(), "默认关闭（不占位也不绘制）");
        check(!view->isFocusable(), "日志面板不可聚焦");

        layoutPanel(view);

        // 视图构造自己也会打日志（例如主题键缺失的警告），先清掉再按绝对条数断言
        Logger::clear();
        Logger::info("第一条");
        Logger::warning("第二条");
        Logger::error("第三条");
        const bklv::LogLevel previous = Logger::getLogLevel();
        Logger::setLogLevel(bklv::LogLevel::DEBUG);
        Logger::debug("第四条 DEBUG"); // 默认级别是 INFO，这条要显式放开才进缓冲
        Logger::setLogLevel(previous);

        view->update();

        checkEqualSize(view->getLogCount(), 4, "缓冲里 4 条");
        checkEqualSize(view->getCachedRowCount(), 4, "4 条全都变成行（默认级别全开）");
        check(view->getRowText(0).find("第一条") != std::string::npos, "第 0 行的文本是第一条");

        // 级别过滤
        view->setLevelVisible(bklv::LogLevel::DEBUG, false);
        view->update();
        checkEqualSize(view->getCachedRowCount(), 3, "关掉 DEBUG 后少一行");
        check(!view->isLevelVisible(bklv::LogLevel::DEBUG), "isLevelVisible 跟着变");

        view->setLevelVisible(bklv::LogLevel::WARNING, false);
        view->setLevelVisible(bklv::LogLevel::ERROR, false);
        view->update();
        checkEqualSize(view->getCachedRowCount(), 1, "只剩 INFO");
        check(view->getRowText(0).find("第一条") != std::string::npos, "剩下的是 INFO 那条");

        view->setLevelVisible(bklv::LogLevel::DEBUG, true);
        view->setLevelVisible(bklv::LogLevel::WARNING, true);
        view->setLevelVisible(bklv::LogLevel::ERROR, true);
        view->update();
        checkEqualSize(view->getCachedRowCount(), 4, "全开又回到 4 行");

        // 列开关：关掉时间/位置后行文本里就没有它们了
        const std::string withTime = view->getRowText(0);
        view->setShowTime(false);
        view->update();
        check(view->getRowText(0) != withTime, "关掉时间后行文本变了");
        check(view->getRowText(0).find("第四条") == std::string::npos, "关掉时间不影响别的行");

        // 显示约定：半透明底色 + 主题最小字号 + 一行一条不换行
        checkFloat(view->getFontSize(), getTheme().number("metric.font.size.xs"),
            "字号取主题里最小的那个");
        check(view->getPanelColor() != 0, "面板底色取到了");
        const unsigned alpha = (view->getPanelColor() >> IM_COL32_A_SHIFT) & 0xFFu;
        check(alpha > 0 && alpha < 255, "面板底色是半透明的（0 < alpha < 255）");

        const size_t rowsBefore = view->getCachedRowCount();
        Logger::info("一条特别特别长的日志，用来确认面板不换行："
                     "它应该仍然只占一行，超出的部分被面板宽度裁掉，而不是折成好几行。");
        view->update();
        checkEqualSize(view->getCachedRowCount(), rowsBefore + 1, "超长日志也只占一行（不换行）");
        check(view->isShowingLatest(), "不支持滚动：面板永远停在最新的一批上");

        // 新日志到达 → 行数跟上
        Logger::info("第五条");
        view->update();
        checkEqualSize(view->getCachedRowCount(), 6, "新日志到达后行数跟上");

        // 清空
        view->clearLogs();
        view->update();
        checkEqualSize(view->getCachedRowCount(), 0, "清空后没有行");
        check(view->getRowText(3).empty(), "越界取行文本返回空串");

        delete view;
    }

    std::printf("\n== 1b. 缓存只存指针：环形缓冲挤掉旧条目也不会读到野指针 ==\n");
    {
        Logger::clear();
        Logger::setMaxEntries(64); // 故意把环形缓冲压小，逼它反复挤掉旧条目

        DebugLogView* view = new DebugLogView();
        layoutPanel(view);
        view->update();

        for (int i = 0; i < 200; i++)
            Logger::info("挤掉的日志 " + std::to_string(i));

        view->update();
        checkEqualSize(view->getLogCount(), 64, "环形缓冲只留 64 条");
        checkEqualSize(view->getCachedRowCount(), 64, "缓存行数与缓冲一致（旧条目已被挤掉）");
        check(view->getRowText(63).find("挤掉的日志 199") != std::string::npos,
            "最新一条在最后（指针重新取过，不是野指针）");
        check(view->getRowText(0).find("挤掉的日志 136") != std::string::npos,
            "第一条是最早还留着的那条");

        // 反复 update 不该重建缓存（重建只由"日志版本/过滤/字号/字体"决定）
        const uint64_t before = view->getRebuildCount();
        view->update();
        view->update();
        check(view->getRebuildCount() == before, "没有新日志时不重建缓存");

        delete view;
        Logger::setMaxEntries(1000); // 还原，别影响后面的小节
    }

    std::printf("\n== 2. 行缓存只在必要时重建（日志再多也不重排）==\n");
    {
        Logger::clear();
        DebugLogView* view = new DebugLogView();
        layoutPanel(view);
        view->update();

        const uint64_t before = view->getRebuildCount();
        view->update();
        view->update();
        check(view->getRebuildCount() == before, "没有新日志时不重建缓存");

        // 写日志但缓冲满了会挤掉旧的：版本仍然变，所以要重建一次
        Logger::info("新的一条");
        view->update();
        check(view->getRebuildCount() == before + 1, "有新日志时只重建一次");

        const uint64_t afterLog = view->getRebuildCount();
        for (int i = 0; i < 50; i++)
            Logger::info("批量日志");
        view->update();
        check(view->getRebuildCount() == afterLog + 1, "一次 update 里来 50 条也只重建一次");

        // 过滤变化也要重建
        view->setLevelVisible(bklv::LogLevel::INFO, false);
        view->update();
        check(view->getRebuildCount() == afterLog + 2, "过滤变化触发一次重建");

        delete view;
    }

    std::printf("\n== 3. 不支持滚动：永远只显示最新的一批 ==\n");
    {
        Logger::clear();
        DebugLogView* view = new DebugLogView();

        // 面板矮一点，能放下多少行一目了然
        view->setEnabled(true);
        view->setWidth(600.0f);
        view->setHeight(200.0f);
        layoutTree(view, 600.0f, 200.0f);

        for (int i = 0; i < 60; i++)
            Logger::info("日志行 " + std::to_string(i));
        view->update();

        const size_t fit = view->getVisibleRowCount();
        check(fit > 1, "面板高度能放下若干行");
        checkEqualSize(view->getCachedRowCount(), 60, "缓存里仍然是全部 60 条");
        checkEqualSize(view->getFirstVisibleRow(), 60 - fit, "从「最后 fit 行」开始画（贴住最新）");
        check(view->isShowingLatest(), "最新一条一定在可见窗口里");
        check(view->getRowText(59).find("日志行 59") != std::string::npos, "最后一行就是最新日志");

        // 继续写日志：窗口跟着往前推，永远贴着最新
        for (int i = 60; i < 90; i++)
            Logger::info("日志行 " + std::to_string(i));
        view->update();
        checkEqualSize(view->getFirstVisibleRow(), 90 - view->getVisibleRowCount(),
            "新日志到达：窗口跟着往前推（不需要滚动）");
        check(view->isShowingLatest(), "依然贴着最新");
        check(view->getRowText(89).find("日志行 89") != std::string::npos, "最新一条还是最后一行");

        // 日志比可见行数少时：从头画（没有空窗）
        Logger::clear();
        for (int i = 0; i < 3; i++)
            Logger::info("少量日志 " + std::to_string(i));
        view->update();
        checkEqualSize(view->getFirstVisibleRow(), 0, "日志比面板能放的行数少时从第 0 行开始");
        check(view->isShowingLatest(), "少量日志时也贴着最新");

        // 面板变高：可见行数变多，窗口起点跟着回退，仍然贴着最新
        Logger::clear();
        for (int i = 0; i < 60; i++)
            Logger::info("日志行 " + std::to_string(i));
        view->update();
        const size_t shortFit = view->getVisibleRowCount();

        view->setHeight(500.0f);
        layoutTree(view, 600.0f, 500.0f);
        view->update();
        check(view->getVisibleRowCount() > shortFit, "面板变高 → 能放下的行数变多");
        check(view->isShowingLatest(), "面板变高之后依然贴着最新");
        checkFloat(view->getRowHeightPx(), view->getFontSize() * 1.4f, "行高固定（字号 × 行高倍数）");

        delete view;
    }

    std::printf("\n== 4. 调试层在 HudLayer 之上 + 面板不吃输入 ==\n");
    {
        InputManager& input = getInputManager();
        input.resetFocusState();

        Shell* shell = new Shell();
        shell->setDimensions(800.0f, 600.0f);

        // 内容层里放一个可聚焦控件，位置正好被日志面板盖住
        Box* content = new Box(Axis::COLUMN);
        content->setWidthPercentage(100.0f);
        content->setHeightPercentage(100.0f);
        FocusBox* under = new FocusBox();
        under->setDimensions(100.0f, 40.0f);
        content->addView(under);
        shell->pushContentView(content);

        // 日志面板：贴左上角，正好压在 under 上面
        DebugLogView* logView = new DebugLogView();
        logView->setPositionTop(0.0f);
        logView->setPositionRight(0.0f);
        logView->setPositionLeft(0.0f);
        logView->setWidth(400.0f);
        logView->setHeight(200.0f);
        logView->setEnabled(true);
        shell->getDebugLayer()->addView(logView);

        layoutTree(shell, 800.0f, 600.0f);
        input.setRoot(shell);
        input.giveFocus(under);

        // 层序：调试层排在 HudLayer 之后（= 最后绘制 = 在所有层之上）
        check(shell->getDebugLayer() != nullptr, "外壳有调试层");
        check(indexIn(shell, shell->getDebugLayer()) > indexIn(shell, shell->getHudLayer()),
            "调试层排在 HudLayer 之后（画在所有层之上）");
        check(indexIn(shell, shell->getHudLayer()) > indexIn(shell, shell->getOverlayLayer()),
            "浮层排在覆盖层之后");
        check(logView->getParent() == shell->getDebugLayer(), "日志面板挂在调试层里");
        checkFloat(logView->getFrame().y, 0.0f, "面板贴到指定位置（布局生效）");

        // 不吃输入：点在面板覆盖的位置，命中的是下面的控件，不是面板
        check(!input.canFocus(logView), "日志面板进不了焦点链");
        input.dispatchPointerClick(Point(50.0f, 20.0f));
        check(input.getCurrentFocus() == under, "点面板覆盖的位置：命中的是下面的控件");

        // 面板可见时也不会挡住方向导航
        input.giveFocus(under);
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == under, "只有一个可聚焦控件时导航停在原地");

        input.setRoot(nullptr);
        input.resetFocusState();
        delete shell;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

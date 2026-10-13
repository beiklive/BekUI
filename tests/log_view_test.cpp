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

        // 新日志到达 → 行数跟上
        Logger::info("第五条");
        view->update();
        checkEqualSize(view->getCachedRowCount(), 5, "新日志到达后行数跟上");

        // 清空
        view->clearLogs();
        view->update();
        checkEqualSize(view->getCachedRowCount(), 0, "清空后没有行");
        check(view->getRowText(3).empty(), "越界取行文本返回空串");

        delete view;
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

    std::printf("\n== 3. 滚动：贴底跟随 / 手动滚开暂停 / 边界裁剪 ==\n");
    {
        Logger::clear();
        DebugLogView* view = new DebugLogView();
        layoutPanel(view, 600.0f, 200.0f);

        for (int i = 0; i < 60; i++)
            Logger::info("日志行 " + std::to_string(i));
        view->update();

        check(view->isAutoScroll(), "默认自动跟随");
        check(view->getContentHeight() > 0.0f, "内容高度算出来了");
        check(view->getMaxScroll() > 0.0f, "内容比面板高，能滚");
        checkFloat(view->getScrollOffset(), view->getMaxScroll(), "自动跟随时停在底部");

        // 往上滚：暂停自动跟随
        view->scrollBy(-3);
        check(view->getScrollOffset() < view->getMaxScroll(), "往上滚离开了底部");
        check(!view->isAutoScroll(), "手动滚开就暂停自动跟随");

        // 再来新日志：视线不被拽走
        const float keep = view->getScrollOffset();
        Logger::info("新日志不该把视角拽走");
        view->update();
        checkFloat(view->getScrollOffset(), keep, "暂停跟随后新日志不改滚动位置");

        // 滚回底部：恢复自动跟随
        view->scrollToBottom();
        check(view->isAutoScroll(), "scrollToBottom 恢复自动跟随");
        checkFloat(view->getScrollOffset(), view->getMaxScroll(), "scrollToBottom 到底");

        // 边界
        view->setScrollOffset(-100.0f);
        checkFloat(view->getScrollOffset(), 0.0f, "负偏移被裁到 0");
        view->setScrollOffset(1.0e9f);
        checkFloat(view->getScrollOffset(), view->getMaxScroll(), "超大偏移被裁到最大值");

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

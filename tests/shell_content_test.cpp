// 无窗口测试：Shell 的内容栈（层级下钻）+ 页头/底栏归属与布局（第 7 步迁移）。
//
// 这一段原来在 `widget_test.cpp` 的 "== 9. AppletFrame：内容栈 ==" 里，用的是旧
// `AppletFrame` 的三段式（contentBox / getHeader / onDismiss / 折叠补间）。
// 旧类删除后：
//   · 内容栈与下钻/返回 → Shell（本文件第 1、2 节）
//   · B 路由 → Shell::setOnBackRequested（第 3 节）
//   · 页头/底栏 → Header/BottomBar 两个控件挂在 Shell 的**覆盖层**里（第 4 节）
//   · 折叠补间搬到宿主（demo/app/demo_app.cpp）—— 库侧只保留"insets 显式给出"的语义（第 5 节）

#include <cstdio>

#include <yoga/Yoga.h>

#include "bklv/core/appearance.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/view.hpp"
#include "bklv/views/padding.hpp"
#include "bklv/widgets/bottom_bar.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/header.hpp"
#include "bklv/widgets/shell.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

Box* makePage(const char* text)
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeightPercentage(100.0f);
    page->addView(new Button(text));
    return page;
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
    initI18n();
    setLocale("zh-Hans");
    setTheme("default");

    std::printf("\n== 1. 内容栈：下钻 / 返回（Shell）==\n");
    {
        Shell* shell = new Shell();
        shell->setWidth(800.0f);
        shell->setHeight(600.0f);

        // 没有内容时栈是空的；第一层进去不产生转场
        check(shell->getContentView() == nullptr, "空外壳没有当前内容");
        check(shell->getContentStackSize() == 0, "空外壳栈为 0");

        Box* pageA = makePage("第一层");
        shell->pushContentView(pageA);
        check(shell->getContentStackSize() == 1, "初始只有一层内容");
        check(shell->getContentView() == pageA, "当前内容是第一层");
        check(!shell->isContentTransitioning(), "第一层进去不产生转场");
        check(pageA->getParent() == shell->getContentLayer(), "当前内容挂在内容层里");
        check(shell->getContentLayer()->getChildrenCount() == 1, "内容层只挂当前内容");

        YGNodeCalculateLayout(shell->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);

        // 下钻：旧层只脱离树（不删、不画），所以内容层的孩子数仍然只有 1
        Box* pageB = makePage("第二层");
        shell->pushContentView(pageB);
        check(shell->getContentStackSize() == 2, "下钻后两层");
        check(shell->getContentView() == pageB, "当前内容是第二层");
        check(shell->getContentLayer()->getChildrenCount() == 1, "内容层仍然只挂一层");
        check(pageA->getParent() == nullptr, "旧层已脱离树（但没被删）");
        check(pageB->getParent() == shell->getContentLayer(), "内容层挂的是新层");

        // 落位（转场跑完）
        shell->stepContentTransition(1.0f);
        checkFloat(pageB->getAlpha(), 1.0f, "落位后新层不透明");
        checkFloat(pageB->getTransform().translateX, 0.0f, "落位后新层位移归零");

        // 逐层返回：返回的是**同一个对象**（说明只脱离不销毁）
        // 转场语义：回调要等转场播完才执行（和 Dialog"播完再摘"同一约定）
        bool called = false;
        shell->popContentView([&called] { called = true; });
        check(!called, "转场期间回调还没跑");
        check(shell->getContentStackSize() == 1, "弹出后栈回到一层");
        check(shell->getContentView() == pageA, "逻辑上已经回到第一层");
        check(pageA->getParent() == shell->getContentLayer(), "回来的层重新挂回内容层");

        shell->stepContentTransition(1.0f);
        check(called, "转场结束后回调执行");
        check(shell->getOutgoingContentView() == nullptr, "转场结束后 out 视图清空");

        // 最外层再返回：什么都不做，只跑回调
        bool again = false;
        shell->popContentView([&again] { again = true; });
        check(again, "只有一层时 pop 立即执行回调");
        check(shell->getContentStackSize() == 1, "只有一层时 pop 不改栈");

        delete shell;
    }

    std::printf("\n== 2. 内容层里只挂当前层（旧层不参与布局）==\n");
    {
        Box* outside = new Box(Axis::COLUMN);
        outside->setWidth(1280.0f);
        outside->setHeight(720.0f);

        Shell* shell = new Shell();
        shell->setWidthPercentage(100.0f);
        shell->setHeightPercentage(100.0f);
        outside->addView(shell);

        Box* first = new Box(Axis::COLUMN);
        first->setHeight(200.0f);
        shell->pushContentView(first);

        YGNodeCalculateLayout(outside->getYGNode(), 1280.0f, 720.0f, YGDirectionLTR);
        const float layerHeight = shell->getContentLayer()->getHeight();
        checkFloat(layerHeight, 720.0f, "内容层吃满外壳（宽度/高度）");

        // 内容比内容层高：内容层高度不变（不是被内容撑大的）
        Box* tall = new Box(Axis::COLUMN);
        tall->setHeight(2000.0f);
        shell->pushContentView(tall);
        shell->stepContentTransition(1.0f);
        YGNodeCalculateLayout(outside->getYGNode(), 1280.0f, 720.0f, YGDirectionLTR);
        checkFloat(shell->getContentLayer()->getHeight(), layerHeight,
            "内容比内容层高时，内容层高度不变");
        checkEqualSize(shell->getContentLayer()->getChildrenCount(), 1, "内容层里始终只有一层");

        delete outside;
    }

    std::printf("\n== 3. B 路由：内容层没人处理时冒泡到外壳（setOnBackRequested）==\n");
    {
        Shell* shell = new Shell();
        shell->setWidth(800.0f);
        shell->setHeight(600.0f);

        Box* pageA = makePage("第一层");
        Button* pageAButton = static_cast<Button*>(pageA->getChildren()[0]);
        shell->pushContentView(pageA);
        YGNodeCalculateLayout(shell->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);

        check(!shell->hasBackRequestedHandler(), "默认没有 B 默认动作");

        bool dismissed = false;
        shell->setOnBackRequested([&dismissed] {
            dismissed = true;
            return true;
        });
        check(shell->hasBackRequestedHandler(), "注册后能查到 B 默认动作");

        // 焦点在内容里的按钮上时按 B：动作沿父链冒泡到外壳
        pageAButton->triggerAction(ControllerButton::B, false);
        check(dismissed, "内容里的 B 冒泡到外壳默认动作");

        // 内容层里还有下钻的层 → 由宿主决定"先返回上一层"
        dismissed = false;
        shell->setOnBackRequested([shell, &dismissed] {
            if (shell->getContentStackSize() > 1)
            {
                shell->popContentView();
                return true;
            }
            dismissed = true;
            return true;
        });

        Box* deep = makePage("第二层");
        Button* innerButton = static_cast<Button*>(deep->getChildren()[0]);
        shell->pushContentView(deep);
        shell->stepContentTransition(1.0f);
        check(shell->getContentStackSize() == 2, "又下钻一层，准备测冒泡");

        innerButton->triggerAction(ControllerButton::B, false);
        shell->stepContentTransition(1.0f);
        check(!dismissed, "栈深 >1 时 B 是返回上一层，不触发外壳默认动作");
        check(shell->getContentStackSize() == 1, "B 冒泡到外壳并弹掉了一层");
        check(shell->getContentView() == pageA, "回到第一层内容");

        // 只剩一层时再按 B 才落到外壳默认动作
        pageAButton->triggerAction(ControllerButton::B, false);
        check(dismissed, "只剩一层时按 B 才交给外壳默认动作");

        delete shell;
    }

    std::printf("\n== 4. 页头 / 底栏：挂覆盖层、布局与绘制次序 ==\n");
    {
        Box* outside = new Box(Axis::COLUMN);
        outside->setWidth(1280.0f);
        outside->setHeight(720.0f);

        Shell* shell          = new Shell();
        shell->setWidthPercentage(100.0f);
        shell->setHeightPercentage(100.0f);
        Header* header        = new Header();
        header->setAppearanceVariant("applet");
        BottomBar* footer     = new BottomBar();

        // 覆盖层是铺满的绝对层：页头在上、底栏在下，中间用 Padding 顶开
        shell->setHeaderView(header);
        shell->getOverlayLayer()->addView(new Padding());
        shell->setFooterView(footer);

        outside->addView(shell);
        YGNodeCalculateLayout(outside->getYGNode(), 1280.0f, 720.0f, YGDirectionLTR);

        check(header->getParent() == shell->getOverlayLayer(), "页头挂在覆盖层");
        check(footer->getParent() == shell->getOverlayLayer(), "底栏挂在覆盖层");
        check(shell->getHeaderView() == header && shell->getFooterView() == footer,
            "getHeaderView/getFooterView 能取回");

        // 覆盖层在内容层之后 → 页头/底栏画在内容之上
        check(indexIn(shell, shell->getOverlayLayer()) > indexIn(shell, shell->getContentLayer()),
            "覆盖层排在内容层之后（画在上层）");

        // 应用外壳的页头用 applet 变体（88 高）
        checkFloat(header->getActiveAppearance().getBox().height,
            getTheme().number("metric.appletFrame.headerHeight", 0.0f),
            "应用外壳的页头用 applet 变体（88）");
        header->setAppearanceVariant("popup");
        check(header->getActiveAppearance().getBox().heightAuto,
            "换 popup 变体后高度 auto（换一份外观文件即可）");

        // ---- 边距与贴底（踩过：100% 宽 + 左右外边距会溢出右边距）----
        const float side = getTheme().number("metric.appletFrame.paddingSides", 0.0f);
        checkFloat(header->getWidth(), 1280.0f - 2.0f * side,
            "页头宽度扣掉左右外边距（不是 100% + margin 溢出）");
        checkFloat(footer->getWidth(), 1280.0f - 2.0f * side, "底栏宽度扣掉左右外边距");
        checkFloat(footer->getFrame().y + footer->getHeight(), 720.0f, "底栏贴着窗口底边");
        check(header->getFrame().y < footer->getFrame().y, "页头在底栏上方");

        // 内容层与覆盖层都在外壳里，互不挤占（覆盖层是绝对层）
        checkFloat(shell->getContentLayer()->getHeight(), 720.0f, "内容层不受页头/底栏挤压");
        checkFloat(shell->getOverlayLayer()->getHeight(), 720.0f, "覆盖层也是铺满的");

        // 页头/底栏是纯显示控件：不参与焦点
        Button* headerButton = new Button("页头里的按钮");
        header->getHintBox()->addView(headerButton);
        Button* contentButton = new Button("内容里的按钮");
        Box* content = new Box(Axis::COLUMN);
        content->addView(contentButton);
        shell->pushContentView(content);
        YGNodeCalculateLayout(outside->getYGNode(), 1280.0f, 720.0f, YGDirectionLTR);

        check(!header->isFocusable(), "页头自己不可聚焦");
        check(shell->getDefaultFocus() == contentButton,
            "外壳的默认焦点落在内容层，不会跑到页头里的按钮上");

        // ---- 显隐：隐藏是 GONE（不占位、不进焦点链）----
        check(shell->isHeaderVisible() && shell->isFooterVisible(), "默认页头与底栏都显示");
        shell->setHeaderVisible(false);
        check(!shell->isHeaderVisible(), "setHeaderVisible(false) 生效");
        check(header->getVisibility() == Visibility::GONE, "隐藏的页头是 GONE（不占位）");
        shell->setHeaderVisible(true);
        check(header->getVisibility() == Visibility::VISIBLE, "再显示出来");

        shell->setFooterVisible(false);
        check(!shell->isFooterVisible(), "setFooterVisible(false) 生效");
        check(footer->getVisibility() == Visibility::GONE, "隐藏的底栏是 GONE（不占位）");
        shell->setFooterVisible(true);
        check(footer->getVisibility() == Visibility::VISIBLE, "再显示出来");

        delete outside;
    }

    std::printf("\n== 5. insets 是**显式**的：显隐不会自动改它 ==\n");
    {
        Shell* shell = new Shell();
        shell->setWidth(800.0f);
        shell->setHeight(600.0f);

        Header* header = new Header("applet");
        shell->setHeaderView(header);

        checkFloat(shell->getHeaderInset(), 0.0f, "默认 inset 是 0（库不猜页头多高）");
        shell->setHeaderInset(88.0f);
        shell->setFooterInset(73.0f);
        checkFloat(shell->getHeaderInset(), 88.0f, "显式设页头留白");
        checkFloat(shell->getFooterInset(), 73.0f, "显式设底栏留白");

        // 隐藏页头**不会**自动把 inset 归零：宿主想省掉那部分高度要自己改（方案 5.4）
        shell->setHeaderVisible(false);
        checkFloat(shell->getHeaderInset(), 88.0f, "隐藏页头不改 inset（宿主要负责）");
        shell->setHeaderInset(0.0f);
        checkFloat(shell->getHeaderInset(), 0.0f, "宿主显式归零才归零");

        delete shell;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

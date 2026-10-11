// 无窗口测试：外观驱动的控件 + 换肤。
//
// 不开窗口也能测的部分：外观绑定、状态切换、换肤后已建视图重新套用外观、
// 以及「主题只覆盖未被显式设置过的值」这条约定。

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <yoga/Yoga.h>

#include "bklv/core/appearance.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/label.hpp"
#include "bklv/widgets/bottom_bar.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/header.hpp"
#include "bklv/widgets/sidebar.hpp"
#include "bklv/widgets/slider.hpp"
#include "bklv/widgets/applet_frame.hpp"
#include "bklv/widgets/tab_frame.hpp"
#include "bklv/widgets/rectangle.hpp"

using namespace bklv;

static int g_failed = 0;
static int g_checks = 0;

static void check(bool ok, const std::string& what)
{
    g_checks++;
    if (ok)
        std::printf("  [通过] %s\n", what.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s\n", what.c_str());
    }
}

static void checkEqual(const std::string& got, const std::string& want, const std::string& what)
{
    g_checks++;
    if (got == want)
        std::printf("  [通过] %s  =>  %s\n", what.c_str(), got.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s  期望 %s 实际 %s\n", what.c_str(), want.c_str(), got.c_str());
    }
}

static void checkFloat(float got, float want, const std::string& what)
{
    g_checks++;
    if (got > want - 0.01f && got < want + 0.01f)
        std::printf("  [通过] %s  =>  %g\n", what.c_str(), got);
    else
    {
        g_failed++;
        std::printf("  [失败] %s  期望 %g 实际 %g\n", what.c_str(), want, got);
    }
}

static void writeFile(const std::string& relativePath, const std::string& content)
{
    const std::string full = resourcePath(relativePath);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(full).parent_path(), ec);
    std::ofstream out(full, std::ios::binary);
    out << content;
}

int main()
{
    setTheme("default");

    std::printf("\n== 1. 控件绑定主题里的外观 ==\n");
    {
        Button button("点我");
        const Appearance& appearance = button.getActiveAppearance();
        check(appearance.getLayers().size() == 2, "Button 取到两层外观（底 + 文字）");
        checkEqual(button.getText(), "点我", "文字进了按钮");
        checkEqual(appearance.getLayers()[1].source, "text", "文字层从 source=text 取内容");
        // getHeight() 读的是 Yoga 的**布局**结果，所以要先真的跑一次布局
        YGNodeCalculateLayout(button.getYGNode(), 300.0f, 100.0f, YGDirectionLTR);
        checkFloat(button.getHeight(), getTheme().number("metric.control.height", 0.0f),
            "按钮高度来自主题 metric.control.height");


        Rectangle rect;
        check(rect.getActiveAppearance().getLayers().size() == 1,
            "Rectangle 取到单层外观");

        // variant 指向另一份外观文件
        Button ghost("描边", "ghost");
        check(ghost.getActiveAppearance().getLayers().size() == 2,
            "ghost 变体取到自己的外观文件");
        checkEqual(ghost.getActiveAppearance().getLayers()[0].id, "bg",
            "ghost 变体的底是 bg 层");
    }

    std::printf("\n== 2. 状态切换驱动动效 ==\n");
    {
        Button button("状态");
        check(!button.isAppearanceStateActive("focused"), "初始没有聚焦状态");
        button.onFocusGained();
        check(button.isAppearanceStateActive("focused"), "聚焦后状态打开");
        check(button.getAppearanceInstanceForTest().isAnimating(), "打开了动效");

        button.onFocusLost();
        check(!button.isAppearanceStateActive("focused"), "失焦后状态关闭");

        button.setEnabled(false);
        check(button.isAppearanceStateActive("disabled"), "禁用后 disabled 状态打开");
        button.setEnabled(true);
        check(!button.isAppearanceStateActive("disabled"), "恢复后 disabled 状态关闭");
    }

    std::printf("\n== 3. 换肤对已建视图生效（不用重建）==\n");
    {
        // 造一个只有一层按钮外观的皮肤
        writeFile("resources/theme/widget-skin/theme.json",
            R"({"name":"widget-skin","version":1,"widgets":["widgets/button.json"]})");
        writeFile("resources/theme/widget-skin/colors.json", R"({"accent":{"base":"#00ff00"}})");
        writeFile("resources/theme/widget-skin/widgets/button.json",
            R"({"layers":[{"op":"rect","id":"bg","fill":"@theme/color.accent.base"}]})");

        Box root(Axis::COLUMN);
        Button* button = new Button("换肤对象");
        root.addView(button);

        check(button->getActiveAppearance().getLayers().size() == 2,
            "换肤前：两层外观");

        check(setTheme("widget-skin"), "切到 widget-skin");
        root.applyThemeRecursive();

        check(button->getActiveAppearance().getLayers().size() == 1,
            "换肤后：同一个按钮实例变成了皮肤里的单层外观");
        checkEqual(button->getText(), "换肤对象", "换肤不动控件的数据（文字还在）");

        setTheme("default");
        root.applyThemeRecursive();
        check(button->getActiveAppearance().getLayers().size() == 2,
            "切回 default 后外观也回来了");
    }

    std::printf("\n== 4. 主题只覆盖未被显式设置过的值 ==\n");
    {
        // 跟随主题
        Label follows;
        follows.setTextColorExplicit(false);
        follows.setFontSizeExplicit(false);
        follows.applyTheme();
        checkFloat(follows.getFontSize(), getTheme().number("metric.font.size.md", 0.0f),
            "没显式设过的字号跟随主题");
        check(follows.getTextColor() == getTheme().color("color.text.primary"),
            "没显式设过的颜色跟随主题");

        // 显式设置过：换肤不该冲掉
        Label pinned;
        pinned.setFontSize(33.0f);
        pinned.setTextColor(IM_COL32(1, 2, 3, 255));
        pinned.applyTheme();
        checkFloat(pinned.getFontSize(), 33.0f, "显式设过的字号不被主题覆盖");
        check(pinned.getTextColor() == IM_COL32(1, 2, 3, 255), "显式设过的颜色不被主题覆盖");

        // 换一次主题再看
        setTheme("ocean");
        pinned.applyTheme();
        follows.applyTheme();
        checkFloat(pinned.getFontSize(), 33.0f, "换主题后显式值仍然保持");
        checkFloat(follows.getFontSize(), getTheme().number("metric.font.size.md", 0.0f),
            "换主题后跟随的值跟着变");
        setTheme("default");
    }

    std::printf("\n== 5. Header / BottomBar（多文字层 + 显式宽高的装饰件）==\n");
    {
        Header header;
        header.setTitle("标题");
        header.setSubtitle("副标题");
        checkEqual(header.getTitle(), "标题", "标题进了控件");

        const Appearance& appearance = header.getActiveAppearance();
        check(appearance.getLayers().size() == 4, "Header 四层：色条 / 标题 / 副标题 / 下边线");
        if (appearance.getLayers().size() == 4)
        {
            const AppearanceLayer& accent   = appearance.getLayers()[0];
            const AppearanceLayer& title    = appearance.getLayers()[1];
            const AppearanceLayer& subtitle = appearance.getLayers()[2];
            const AppearanceLayer& line     = appearance.getLayers()[3];

            checkFloat(accent.width, 5.0f, "色条用显式 width=5（brls/header/rectangle_width）");
            check(accent.op == AppearanceLayer::Op::Rect, "色条是 rect 层");

            checkEqual(title.source, "title", "标题层的数据源是 title");
            checkEqual(subtitle.source, "subtitle", "副标题层的数据源是 subtitle");
            checkEqual(subtitle.align, "right", "副标题右对齐");

            checkFloat(line.height, 1.0f, "下边线用显式 height=1");
            checkEqual(line.valign, "bottom", "下边线贴底");
        }

        BottomBar bar("A 确定    B 返回");
        check(bar.getActiveAppearance().getLayers().size() == 2, "BottomBar 两层：上边线 + 文字");
        if (bar.getActiveAppearance().getLayers().size() == 2)
        {
            checkEqual(bar.getActiveAppearance().getLayers()[0].valign, "top", "上边线贴上");
            checkEqual(bar.getActiveAppearance().getLayers()[1].source, "text", "文字层数据源是 text");
        }
        checkEqual(bar.getText(), "A 确定    B 返回", "底栏文字");
    }

    std::printf("\n== 6. Slider：进度与外观分段 ==\n");
    {
        Slider slider;
        checkFloat(slider.getProgress(), 0.0f, "初始进度 0");

        slider.setProgress(0.4f);
        checkFloat(slider.getProgress(), 0.4f, "设置进度");
        slider.setProgress(2.0f);
        checkFloat(slider.getProgress(), 1.0f, "进度会被夹到 1");
        slider.setProgress(-1.0f);
        checkFloat(slider.getProgress(), 0.0f, "进度会被夹到 0");

        const Appearance& appearance = slider.getActiveAppearance();
        check(appearance.indexOfLayer("track") >= 0, "外观里有 track 段");
        check(appearance.indexOfLayer("fill") >= 0, "外观里有 fill 段");
        check(appearance.indexOfLayer("knob") >= 0, "外观里有 knob 段");
        check(appearance.getTransitions("focused").size() == 2, "聚焦有两条动效");
    }

    std::printf("\n== 7. TabFrame：切页只脱离不销毁 ==\n");
    {
        TabFrame frame;
        View* pageA = new Box(Axis::COLUMN);
        View* pageB = new Box(Axis::COLUMN);
        View* pageC = new Box(Axis::COLUMN);

        frame.addTab("A", pageA);
        frame.addTab("B", pageB);
        frame.addTab("C", pageC);

        check(frame.getTabCount() == 3, "三个 tab");
        check(frame.getSelectedIndex() == 0, "默认选第一个");
        check(frame.getContentBox()->getChildrenCount() == 1, "内容区只挂当前页");
        check(frame.getContentBox()->getChildren()[0] == pageA, "当前挂的是第一页");

        frame.setSelectedIndex(2);
        check(frame.getSelectedIndex() == 2, "切到第三页");
        check(frame.getContentBox()->getChildrenCount() == 1, "内容区仍然只有一个页面");
        check(frame.getContentBox()->getChildren()[0] == pageC, "挂的是第三页");

        // 切回来：还是同一个对象（说明只是脱离，没有销毁重建）
        frame.setSelectedIndex(0);
        check(frame.getContentBox()->getChildren()[0] == pageA, "切回来还是同一个页面对象");

        check(frame.getTabBar()->getChildrenCount() == 3, "tab 栏三个格子");
        TabCell* second = static_cast<TabCell*>(frame.getTabBar()->getChildren()[1]);
        check(!second->isSelected(), "未选中的格子 selected=false");
        frame.setSelectedIndex(1);
        check(second->isSelected(), "选中后 selected=true");
    }

    std::printf("\n== 8. Sidebar：条目与选中态 ==\n");
    {
        Sidebar sidebar("导航");
        checkEqual(sidebar.getTitle(), "导航", "侧栏标题");

        sidebar.addItem("条目一", nullptr);
        sidebar.addItem("条目二", nullptr);
        check(sidebar.getItemCount() == 2, "两个条目");
        check(sidebar.getSelectedIndex() == 0, "默认选第一个");

        sidebar.setSelectedIndex(1);
        check(sidebar.getSelectedIndex() == 1, "选中第二个");

        sidebar.setTitle("新标题");
        checkEqual(sidebar.getTitle(), "新标题", "改标题");

        sidebar.clearItems();
        check(sidebar.getItemCount() == 0, "清空条目");
    }

    std::printf("\n== 9. AppletFrame：内容栈（下钻 / 返回）==\n");
    {
        Box* root = new Box(Axis::COLUMN);
        Box* pageA = new Box(Axis::COLUMN);
        Button* pageAButton = new Button("第一层里的按钮");
        pageA->addView(pageAButton);
        AppletFrame* frame = new AppletFrame(pageA);
        root->addView(frame);

        check(frame->getContentStackSize() == 1, "初始只有一层内容");
        check(frame->getContentView() == pageA, "当前内容是第一层");
        check(frame->getContentBox()->getChildrenCount() == 1, "内容区只挂当前内容");

        // 下钻
        View* pageB = new Box(Axis::COLUMN);
        frame->pushContentView(pageB);
        check(frame->getContentStackSize() == 2, "下钻后两层");
        check(frame->getContentView() == pageB, "当前内容是第二层");
        check(frame->getContentBox()->getChildrenCount() == 1, "内容区仍然只挂一层");

        View* pageC = new Box(Axis::COLUMN);
        frame->pushContentView(pageC);
        check(frame->getContentStackSize() == 3, "再下钻三层");

        // 逐层返回，返回的是**同一个对象**（说明只脱离不销毁）
        bool called = false;
        frame->popContentView([&called] { called = true; });
        check(called, "popContentView 调了回调");
        check(frame->getContentView() == pageB, "退回到第二层");
        frame->popContentView();
        check(frame->getContentView() == pageA, "退回第一层");
        check(frame->getContentStackSize() == 1, "只剩一层");

        // 最外层再返回：交给 onDismiss
        bool dismissed = false;
        frame->setOnDismiss([&dismissed] { dismissed = true; });
        frame->triggerAction(ControllerButton::B, false);
        check(dismissed, "最外层按 B 交给 onDismiss");

        // 焦点在内容里的按钮上时按 B：动作要沿父链冒泡到框架（borealis 也是这么做的）
        Box* deep = new Box(Axis::COLUMN);
        Button* innerButton = new Button("内容里的按钮");
        deep->addView(innerButton);
        frame->pushContentView(deep);
        check(frame->getContentStackSize() == 2, "又下钻一层，准备测冒泡");

        dismissed = false;
        innerButton->triggerAction(ControllerButton::B, false);
        check(!dismissed, "栈深 >1 时 B 是返回上一层，不触发 onDismiss");
        check(frame->getContentStackSize() == 1, "B 冒泡到框架并弹掉了一层");
        check(frame->getContentView() == pageA, "回到第一层内容");

        // 这时 innerButton 已经随 deep 一起脱离了树，得用当前内容里的按钮
        pageAButton->triggerAction(ControllerButton::B, false);
        check(dismissed, "只剩一层时按 B 才交给 onDismiss");

        // 页头样式切换 = 换一份外观文件
        Header* header = frame->getHeader();
        checkEqual(header->getActiveAppearance().getLayers()[1].align, "left", "常规页头标题左对齐");
        frame->setHeaderStyle(AppletHeaderStyle::Popup);
        checkEqual(header->getActiveAppearance().getLayers().size() == 4
                ? header->getActiveAppearance().getLayers()[1].align : std::string("?"),
            "center", "Popup 页头标题居中（换了外观文件）");

        root->removeView(frame, true);
        delete root;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

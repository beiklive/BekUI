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
#include "bklv/views/scrolling_frame.hpp"
#include "bklv/widgets/bottom_bar.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/cell.hpp"
#include "bklv/widgets/header.hpp"
#include "bklv/widgets/sidebar.hpp"
#include "bklv/widgets/slider.hpp"
#include "bklv/widgets/applet_frame.hpp"
#include "bklv/widgets/tab_frame.hpp"
#include "bklv/widgets/rectangle.hpp"
#include "test_util.hpp"

using namespace bklv;





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
    // 资源按可执行文件目录解析：从哪个目录跑都一致，fixture 也落在 dist/
    bklv::initResources();

    setTheme("default");

    std::printf("\n== 1. 控件绑定主题里的外观 ==\n");
    {
        Button button("点我");
        const Appearance& appearance = button.getActiveAppearance();
        check(appearance.getLayers().size() == 5,
            "Button 取到五层外观（底 + 高亮底 + 文字 + 辉光 + 描边）");
        checkEqual(button.getText(), "点我", "文字进了按钮");
        checkEqual(appearance.getLayers()[2].source, "text", "文字层从 source=text 取内容");
        check(appearance.indexOfLayer("hlBorder") > 0, "带焦点高亮描边层");
        // getHeight() 读的是 Yoga 的**布局**结果，所以要先真的跑一次布局
        YGNodeCalculateLayout(button.getYGNode(), 300.0f, 100.0f, YGDirectionLTR);
        checkFloat(button.getHeight(), getTheme().number("metric.control.height", 0.0f),
            "按钮高度来自主题 metric.control.height");


        Rectangle rect;
        check(rect.getActiveAppearance().getLayers().size() == 1,
            "Rectangle 取到单层外观");

        // variant 指向另一份外观文件
        Button ghost("描边", "ghost");
        check(ghost.getActiveAppearance().getLayers().size() == 5,
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

        check(button->getActiveAppearance().getLayers().size() == 5,
            "换肤前：五层外观");

        check(setTheme("widget-skin"), "切到 widget-skin");
        root.applyThemeRecursive();

        check(button->getActiveAppearance().getLayers().size() == 1,
            "换肤后：同一个按钮实例变成了皮肤里的单层外观");
        checkEqual(button->getText(), "换肤对象", "换肤不动控件的数据（文字还在）");

        setTheme("default");
        root.applyThemeRecursive();
        check(button->getActiveAppearance().getLayers().size() == 5,
            "切回 default 后外观也回来了");
    }

    std::printf("\n== 3.5 控件样式真的来自 JSON：改 box 段，控件尺寸跟着变 ==\n");
    {
        writeFile("resources/theme/box-skin/theme.json",
            R"({"name":"box-skin","version":1,"widgets":["widgets/button.json"]})");
        writeFile("resources/theme/box-skin/widgets/button.json",
            R"({"box":{"height":72,"padding":[4,20,4,20]},
                "layers":[{"op":"rect","id":"bg","fill":"@theme/color.button.defaultBg"}]})");

        Button* button = new Button("尺寸来自 JSON");
        YGNodeCalculateLayout(button->getYGNode(), 300.0f, 200.0f, YGDirectionLTR);
        checkFloat(button->getHeight(), getTheme().number("metric.control.height", 0.0f),
            "默认主题：按钮高度 = metric.control.height");

        check(setTheme("box-skin"), "切到 box-skin");
        button->applyTheme();
        // getHeight() 读的是 Yoga 布局结果，改完样式要重跑一次布局（延迟生效）
        YGNodeCalculateLayout(button->getYGNode(), 300.0f, 200.0f, YGDirectionLTR);
        checkFloat(button->getHeight(), 72.0f, "皮肤把 box.height 改成 72，控件高度跟着变");
        checkFloat(button->getPaddingTop(), 4.0f, "box.padding 也生效");
        checkFloat(button->getPaddingLeft(), 20.0f, "box.padding 左边距生效");

        setTheme("default");
        button->applyTheme();
        YGNodeCalculateLayout(button->getYGNode(), 300.0f, 200.0f, YGDirectionLTR);
        checkFloat(button->getHeight(), getTheme().number("metric.control.height", 0.0f),
            "切回默认主题后高度回到主题值");
        delete button;
    }

    std::printf("\n== 3.6 滚动容器只吃剩余空间，不压缩同列的兄弟节点 ==\n");
    {
        // 踩过：滚动器拿"内容高度"当 flex 基准，内容一大，Yoga 按比例把同列的
        // 按钮行从 48 压到 28 → 每次切页内容区高度都变，看着像底栏在上下闪。
        Box* column = new Box(Axis::COLUMN);
        column->setWidth(400.0f);
        column->setHeight(300.0f);

        Box* topRow = new Box(Axis::ROW);
        topRow->setHeight(48.0f);
        column->addView(topRow);

        ScrollingFrame* scroller = new ScrollingFrame();
        scroller->setGrow(1.0f);
        Box* content = new Box(Axis::COLUMN);
        content->setHeight(900.0f); // 内容远高于可用空间
        scroller->setContentView(content);
        column->addView(scroller);

        YGNodeCalculateLayout(column->getYGNode(), 400.0f, 300.0f, YGDirectionLTR);
        checkFloat(topRow->getHeight(), 48.0f, "内容再高也不会压扁同列的兄弟");
        checkFloat(scroller->getHeight(), 300.0f - 48.0f, "滚动器拿满剩余空间");
        checkFloat(content->getHeight(), 900.0f, "内容保持自己的高度（可滚动）");

        // 换一页内容（更矮），滚动器高度不变
        content->setHeight(120.0f);
        YGNodeCalculateLayout(column->getYGNode(), 400.0f, 300.0f, YGDirectionLTR);
        checkFloat(scroller->getHeight(), 300.0f - 48.0f, "内容变矮后滚动器仍然占满剩余空间");

        delete column;
    }

    std::printf("\n== 3.7 跨容器的方向导航（焦点不会被当前焦点自己卡住）==\n");
    {
        // 两张卡片各自装一个按钮：焦点在卡 1 的按钮上时，按"下"要能走到卡 2 的按钮。
        // 踩过：卡片（不可聚焦的容器）把子树里的默认焦点当成候选，
        // 而那个候选正是当前焦点自己 → 用最低分短路，永远不冒泡。
        Box* page = new Box(Axis::COLUMN);
        page->setWidth(600.0f);
        page->setHeight(400.0f);

        Box* cardA = new Box(Axis::COLUMN);
        cardA->setWidthPercentage(100.0f);
        cardA->setHeight(80.0f);
        Button* buttonA = new Button("A");
        cardA->addView(buttonA);
        page->addView(cardA);

        Box* cardB = new Box(Axis::COLUMN);
        cardB->setWidthPercentage(100.0f);
        cardB->setHeight(80.0f);
        Button* buttonB = new Button("B");
        cardB->addView(buttonB);
        page->addView(cardB);

        YGNodeCalculateLayout(page->getYGNode(), 600.0f, 400.0f, YGDirectionLTR);

        check(buttonA->getNextFocus(FocusDirection::DOWN, buttonA) == buttonB,
            "同层容器里能往下走");
        check(buttonB->getNextFocus(FocusDirection::UP, buttonB) == buttonA,
            "反向也能走回来");
        check(buttonA->getNextFocus(FocusDirection::DOWN, buttonA) != buttonA,
            "不会把当前焦点自己当候选");

        // 步进器（不可聚焦）+ 内嵌滑块的那种结构：从滑块往下也要能走出去
        Box* cardC = new Box(Axis::COLUMN);
        cardC->setWidthPercentage(100.0f);
        cardC->setHeight(80.0f);
        SliderCell* cell = new SliderCell("滑块");
        cardC->addView(cell);
        page->addView(cardC);
        YGNodeCalculateLayout(page->getYGNode(), 600.0f, 400.0f, YGDirectionLTR);

        check(cell->getSlider()->getNextFocus(FocusDirection::UP, cell->getSlider()) == buttonB,
            "从内嵌滑块往上走能跨出所在的卡片");

        delete page;
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

        // 页头按 borealis 的结构：图标 / 标题 / 右侧区 都是子节点，装饰只有下边线
        const Appearance& appearance = header.getActiveAppearance();
        check(appearance.getLayers().size() == 1, "Header 只有一层装饰：下边线");
        if (!appearance.getLayers().empty())
        {
            checkFloat(appearance.getLayers()[0].height, 1.0f, "下边线 height=1");
            checkEqual(appearance.getLayers()[0].valign, "bottom", "下边线贴底");
        }

        check(header.getIcon() != nullptr, "页头有图标位");
        check(header.getIcon()->getVisibility() == Visibility::GONE, "没设图标时图标隐藏");
        check(header.getHintBox() != nullptr, "页头有右侧区");
        check(header.getHintBox()->getChildrenCount() == 1, "右侧区里放着副标题");
        checkFloat(header.getTitleLabel()->getFontSize(),
            getTheme().number("metric.header.titleFontSize", 0.0f),
            "标题字号来自主题（brls/applet_frame/header_title_font_size）");

        const Appearance& iconAppearance = header.getIcon()->getActiveAppearance();
        check(iconAppearance.getBox().hasMargin, "图标左边距来自 header_icon.json 的 box");

        // 底栏 = 上边线（外观层）+ 左侧提示 + 右侧状态区（子节点），一个控件装齐
        BottomBar bar;
        check(bar.getHints() != nullptr, "底栏里有提示栏");
        check(bar.getStatus() != nullptr, "底栏里有状态区");
        check(bar.getActiveAppearance().getLayers().size() == 1, "底栏一层：上边线");
        if (!bar.getActiveAppearance().getLayers().empty())
            checkEqual(bar.getActiveAppearance().getLayers()[0].valign, "top", "上边线贴上");
        check(bar.getActiveAppearance().getBox().hasHeight, "底栏高度来自 box 段");
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
        // 转场语义：回调要等转场播完才执行（和 Dialog"播完再摘"同一约定）
        bool called = false;
        frame->popContentView([&called] { called = true; });
        check(!called, "转场期间回调还没跑");
        frame->stepContentTransition(1.0f);
        check(called, "转场结束后回调执行");
        check(frame->getContentView() == pageB, "退回到第二层");

        frame->popContentView();
        frame->stepContentTransition(1.0f);
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

        // 页头样式切换 = 换一份外观文件（applet 变体 88 高 → popup 变体 auto）
        Header* header = frame->getHeader();
        checkFloat(header->getActiveAppearance().getBox().height,
            getTheme().number("metric.appletFrame.headerHeight", 0.0f),
            "应用外壳的页头用 applet 变体（88）");
        frame->setHeaderStyle(AppletHeaderStyle::POPUP);
        check(header->getActiveAppearance().getBox().heightAuto,
            "Popup 页头换成了另一份外观（高度 auto）");

        // ---- 外壳的边距与内容区高度（踩过：页头/底栏 100% 宽 + 左右外边距会溢出右边距；
        //      内容区高度跟着页面内容变，看着像底栏在动）----
        {
            Box* outside = new Box(Axis::COLUMN);
            outside->setWidth(1280.0f);
            outside->setHeight(720.0f);
            AppletFrame* shell = new AppletFrame();
            outside->addView(shell);
            YGNodeCalculateLayout(outside->getYGNode(), 1280.0f, 720.0f, YGDirectionLTR);

            const float side = getTheme().number("metric.appletFrame.paddingSides", 0.0f);
            checkFloat(shell->getHeader()->getWidth(), 1280.0f - 2.0f * side,
                "页头宽度扣掉左右外边距（不是 100% + margin 溢出）");
            checkFloat(shell->getFooter()->getWidth(), 1280.0f - 2.0f * side,
                "底栏宽度扣掉左右外边距");
            checkFloat(shell->getFooter()->getFrame().y + shell->getFooter()->getHeight(), 720.0f,
                "底栏贴着窗口底边");

            // 应用给内容加"auto 宽 + 左右 margin"时，得到的是 内容区宽 − 边距。
            // 写成 100% + margin 会多出边距那一段（踩过：右上按钮被推出边界）
            Box* content = new Box(Axis::COLUMN);
            content->setWidth(AUTO);
            content->setMargins(0.0f, side, 0.0f, side);
            shell->pushContentView(content);
            YGNodeCalculateLayout(outside->getYGNode(), 1280.0f, 720.0f, YGDirectionLTR);
            checkFloat(content->getWidth(), 1280.0f - 2.0f * side,
                "内容 auto 宽 + 左右 margin = 内容区宽扣掉边距");
            checkFloat(content->getX(), side, "内容左边距生效");
            shell->popContentView();

            // 内容换成更高的东西，内容区高度与底栏位置都不该变
            const float contentHeight = shell->getContentBox()->getHeight();
            const float footerY       = shell->getFooter()->getFrame().y;

            Box* tall = new Box(Axis::COLUMN);
            tall->setHeight(1200.0f); // 比内容区高一截
            shell->pushContentView(tall);
            YGNodeCalculateLayout(outside->getYGNode(), 1280.0f, 720.0f, YGDirectionLTR);

            checkFloat(shell->getContentBox()->getHeight(), contentHeight,
                "内容比内容区高时，内容区高度不变");
            checkFloat(shell->getFooter()->getFrame().y, footerY,
                "内容比内容区高时，底栏不会被动走");

            outside->removeView(shell, true);
            delete outside;
        }

        // ---- 全局显隐：关掉补间时立刻切换，内容区吃掉那部分高度 ----
        frame->setBarsAnimated(false);
        check(frame->isHeaderVisible() && frame->isBottomBarVisible(), "默认页头与底栏都显示");

        frame->setHeaderVisible(false);
        check(!frame->isHeaderVisible(), "setHeaderVisible(false) 生效");
        check(header->getVisibility() == Visibility::GONE, "页头 GONE（不占位）");

        frame->setBottomBarVisible(false);
        check(!frame->isBottomBarVisible(), "setBottomBarVisible(false) 生效");
        check(frame->getFooter()->getVisibility() == Visibility::GONE, "底栏 GONE（不占位）");

        frame->setHeaderVisible(true);
        frame->setBottomBarVisible(true);
        check(header->getVisibility() == Visibility::VISIBLE, "再显示出来");

        // ---- 开补间时折叠会逐帧变化 ----
        frame->setBarsAnimated(true);
        frame->setBarAnimationDuration(0.05f);
        frame->setHeaderVisible(false);
        check(frame->getHeader()->getVisibility() == Visibility::VISIBLE,
            "补间期间页头先保持可见（才有高度可补）");
        const float fullHeight = getTheme().number("metric.appletFrame.headerHeight", 88.0f);
        bool shrank = false;
        for (int i = 0; i < 30; i++)
        {
            Ticking::updateTickings(); // 补间是全局 ticking 驱动的
            frame->update();
            // 高度要读布局结果，所以每帧跑一次布局（正常运行由 App::frame 负责）
            YGNodeCalculateLayout(frame->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);
            if (frame->getHeader()->getHeight() < fullHeight)
                shrank = true;
        }
        check(shrank, "补间过程中页头高度在逐步变小");
        check(frame->getHeader()->getVisibility() == Visibility::GONE,
            "补间结束后页头才真正 GONE");

        root->removeView(frame, true);
        delete root;
    }

    std::printf("\n== 底栏：提示在左、状态在右 ==\n");
    {
        // borealis 的 bottomBarXML：Box(justifyContent=spaceBetween) 里 Hints 在前、状态区在后。
        // 顺序反了界面就变成"电量在左、提示在右"（工作区里被误改过一次，加测试盯住）。
        BottomBar* bar = new BottomBar();
        checkEqualSize(bar->getChildrenCount(), 2, "底栏两个子节点");
        if (bar->getChildrenCount() == 2)
        {
            checkEqual(bar->getChildren()[0]->getClassString(), "bklv::HintsBar",
                "第 0 个是提示栏（左）");
            checkEqual(bar->getChildren()[1]->getClassString(), "bklv::StatusArea",
                "第 1 个是状态区（右）");
        }
        check(bar->getHints() != nullptr && bar->getStatus() != nullptr, "两个都能取到");
        delete bar;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

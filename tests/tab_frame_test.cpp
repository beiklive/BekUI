// TabFrame：方向 / 栏位置 / 两种触发模式的焦点规则。
//
// 规格见 docs/tab-frame.md（用户拍板）：
//   CLICK —— 点击/A 才切页，焦点留在 tab 上；
//   FOCUS —— 焦点移动即切页，点击后焦点进入子页面默认焦点。

#include <cstdio>
#include <string>

#include "bklv/core/input.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/tab_frame.hpp"
#include "test_util.hpp"

using namespace bklv;

namespace
{

// 内容页：一个可聚焦按钮（FOCUS 模式要验证"焦点交给内容"）
Box* makePage(const char* label)
{
    Box* page = new Box(Axis::COLUMN);
    Button* button = new Button();
    button->setText(label);
    page->addView(button);
    return page;
}

} // namespace

int main()
{
    std::string error;
    check(loadThemeIntoGlobalFromDirectory("resources/theme/default", "default"),
        "装载 default 主题（" + error + "）");

    std::printf("\n== 1. 结构：方向 × 栏位置 ==\n");
    {
        TabFrame frame; // 默认：横向 + 栏在顶部 + 点击触发
        check(frame.getOrientation() == TabOrientation::HORIZONTAL, "默认横向");
        check(frame.getBarPlacement() == TabBarPlacement::START, "默认 START");
        check(frame.getTriggerMode() == TabTriggerMode::CLICK, "默认点击触发");

        frame.addTab("一", makePage("一"));
        frame.addTab("二", makePage("二"));

        check(frame.getTabCount() == 2, "两个 tab");
        check(frame.getChildrenCount() == 2, "栏 + 内容框两个块");
        check(frame.getChildren()[0] == frame.getBarContainer(), "START：栏在前");
        check(frame.getAxis() == Axis::COLUMN, "横向时外框是列");

        frame.setBarPlacement(TabBarPlacement::END);
        check(frame.getChildren()[0] != frame.getBarContainer(), "END：栏在后");

        frame.setOrientation(TabOrientation::VERTICAL);
        check(frame.getAxis() == Axis::ROW, "纵向时外框是行");
        check(frame.getChildren()[0] != frame.getBarContainer(), "纵向 END 仍保持交换");
        frame.setBarPlacement(TabBarPlacement::START);
        check(frame.getChildren()[0] == frame.getBarContainer(), "纵向 START：栏在前");
    }

    std::printf("\n== 2. CLICK：焦点移动不切页，点击才切、焦点留在 tab ==\n");
    {
        TabFrame frame(TabOrientation::HORIZONTAL, TabTriggerMode::CLICK);
        frame.addTab("一", makePage("一"));
        frame.addTab("二", makePage("二"));

        auto* second = static_cast<TabCell*>(frame.getTabBar()->getChildren()[1]);
        getInputManager().giveFocus(second);
        checkEqualInt((int)frame.getSelectedIndex(), 0, "焦点移到第二个 tab 仍未切页");
        check(getInputManager().getCurrentFocus() == second, "焦点在 tab 上");

        second->trigger(); // 点击 / A
        checkEqualInt((int)frame.getSelectedIndex(), 1, "触发后切页");
        check(getInputManager().getCurrentFocus() == second, "切完焦点仍在 tab 上");
    }

    std::printf("\n== 3. FOCUS：焦点移动即切页，点击后焦点进入内容 ==\n");
    {
        TabFrame frame(TabOrientation::HORIZONTAL, TabTriggerMode::FOCUS);
        frame.addTab("一", makePage("一"));
        frame.addTab("二", makePage("二"));

        auto* second = static_cast<TabCell*>(frame.getTabBar()->getChildren()[1]);
        getInputManager().giveFocus(second);
        checkEqualInt((int)frame.getSelectedIndex(), 1, "焦点移到第二个 tab 就切页");

        second->trigger();
        View* focused = getInputManager().getCurrentFocus();
        check(focused != nullptr && focused != second, "点击后焦点离开 tab");
        check(focused != nullptr && focused->getClassString() == "bklv::Button", "焦点落到内容里的按钮");
    }

    std::printf("\n== 4. 切页只脱离不销毁 ==\n");
    {
        TabFrame frame;
        View* page = makePage("一");
        frame.addTab("一", page);
        frame.addTab("二", makePage("二"));
        frame.setSelectedIndex(1);
        check(frame.getContentBox()->getChildrenCount() == 1, "内容框同时只有一个页面");
        frame.setSelectedIndex(0);
        check(frame.getContentBox()->getChildren()[0] == page, "切回来还是原来那个实例");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

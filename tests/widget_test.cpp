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
#include "bklv/widgets/button.hpp"
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

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

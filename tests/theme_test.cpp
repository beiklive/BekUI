// 无窗口测试：主题与外观（JSON 描述绘制结构 + tweeny 状态动效）。
//
// 测试自己会在 <可执行文件目录>/resources/theme/ 下写一个 test-skin 主题，
// 用来验证「换肤 = 复制 default 改名改 JSON」与「缺 key 回退 default」，
// 不往仓库里塞测试专用资源。

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "bklv/core/appearance.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"

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

static void checkColor(ImU32 got, ImU32 want, const std::string& what)
{
    g_checks++;
    if (got == want)
        std::printf("  [通过] %s\n", what.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s  期望 %08X 实际 %08X\n", what.c_str(), want, got);
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
    std::printf("\n== 1. 默认主题装载 ==\n");
    {
        const ThemeData& theme = getTheme();
        checkEqual(theme.getName(), "default", "默认主题名");
        check(theme.getColorCount() > 10, "色板有若干条");
        check(theme.getNumberCount() > 5, "数值表有若干条");
    }

    std::printf("\n== 2. 拍平成点分层 key ==\n");
    {
        const ThemeData& theme = getTheme();
        checkColor(theme.color("color.surface.card"), IM_COL32(0x26, 0x2A, 0x32, 255),
            "color.surface.card == #262a32");
        checkColor(theme.color("color.accent.base"), IM_COL32(0x5A, 0xAA, 0xF5, 255),
            "color.accent.base == #5aaaf5");
        checkFloat(theme.number("metric.radius.md"), 8.0f, "metric.radius.md == 8");
        checkFloat(theme.number("metric.font.size.md"), 15.0f, "metric.font.size.md == 15");
    }

    std::printf("\n== 3. 缺 key 用兜底值（只警告不崩）==\n");
    {
        const ThemeData& theme = getTheme();
        checkColor(theme.color("color.__nope__", IM_COL32(1, 2, 3, 4)), IM_COL32(1, 2, 3, 4),
            "缺色返回兜底值");
        checkFloat(theme.number("metric.__nope__", -7.0f), -7.0f, "缺数值返回兜底值");
    }

    std::printf("\n== 4. 控件外观编译（JSON 描述绘制结构）==\n");
    {
        std::shared_ptr<const Appearance> button = getWidgetAppearance("button");
        check(button != nullptr, "button 外观能取到");
        if (button != nullptr)
        {
            check(button->getLayers().size() == 2, "两层：底 + 文字");
            if (button->getLayers().size() == 2)
            {
                const AppearanceLayer& bg    = button->getLayers()[0];
                const AppearanceLayer& title = button->getLayers()[1];

                checkEqual(bg.id, "bg", "第 1 层 id");
                check(bg.op == AppearanceLayer::Op::Rect, "第 1 层是 rect");
                checkColor(bg.fill, IM_COL32(0x2E, 0x33, 0x3C, 255), "底：@theme/color.surface.raised");
                checkFloat(bg.radius, 8.0f, "底：圆角来自 @theme/metric.radius.md");

                checkEqual(title.id, "title", "第 2 层 id");
                check(title.op == AppearanceLayer::Op::Text, "第 2 层是 text");
                checkEqual(title.source, "text", "文字层的数据来源是 text");
                checkFloat(title.fontSize, 15.0f, "文字：字号来自主题");
                checkColor(title.textColor, IM_COL32(0xEB, 0xEE, 0xF2, 255), "文字：颜色来自主题");
            }

            check(button->getStateNames().size() == 3, "三个状态：focused / pressed / disabled");
            check(button->getTransitions("focused").size() == 2, "focused 有两条属性动效");
        }
    }

    std::printf("\n== 5. 状态动效（tweeny 逐属性插值）==\n");
    {
        std::shared_ptr<const Appearance> button = getWidgetAppearance("button");
        check(button != nullptr, "取到外观");

        AppearanceInstance instance;
        instance.setAppearance(button);
        checkColor(instance.getColor(0, AppearanceProp::Fill), IM_COL32(0x2E, 0x33, 0x3C, 255),
            "初值：底是 surface.raised");
        check(!instance.isAnimating(), "没触发状态时不该有补间在跑");

        instance.setState("focused", true);
        check(instance.isAnimating(), "聚焦后开始补间");

        // 目标色
        const ImU32 focusedFill = IM_COL32(0x5A, 0xAA, 0xF5, 255);
        const ImU32 idleFill    = IM_COL32(0x2E, 0x33, 0x3C, 255);

        // 推进 60ms（该动效 120ms），应该处在中间：既不是初值也不是终值
        instance.update(0.06f);
        const ImU32 mid = instance.getColor(0, AppearanceProp::Fill);
        check(mid != idleFill && mid != focusedFill, "60ms 时颜色在中间态（不是跳变）");
        check(instance.isAnimating(), "动效还没结束");

        // 再推进到结束
        instance.update(0.2f);
        checkColor(instance.getColor(0, AppearanceProp::Fill), focusedFill, "结束值：accent.base");
        check(!instance.isAnimating(), "动效结束后不再是 animating");

        // 文字颜色同样按各自动效插值
        checkColor(instance.getColor(1, AppearanceProp::TextColor), IM_COL32(0xFF, 0xFF, 0xFF, 255),
            "文字色补到 onAccent");

        std::printf("\n== 6. 状态取消后回到初值 ==\n");
        // 先单独验一个容易漏的情况：刚打开就马上关掉——这时当前值还等于初值，
        // 旧补间必须被撤掉，否则它会继续往状态目标跑（弹窗面板整块补透明就是这么来的）
        {
            AppearanceInstance quick;
            quick.setAppearance(button);
            quick.setState("focused", true);   // 启动 1 -> 目标色 的补间
            quick.setState("focused", false);  // 立刻撤销：当前值还没动
            for (int i = 0; i < 30; i++)
                quick.update(0.05f);
            checkColor(quick.getColor(0, AppearanceProp::Fill), IM_COL32(0x2E, 0x33, 0x3C, 255),
                "开了又马上关：停在初值，不会继续跑到状态目标");
            check(!quick.isAnimating(), "撤销后不该还有补间在跑");
        }

        instance.setState("focused", false);
        instance.update(0.3f);
        checkColor(instance.getColor(0, AppearanceProp::Fill), idleFill, "取消聚焦后底色回到初值");
        checkColor(instance.getColor(1, AppearanceProp::TextColor), IM_COL32(0xEB, 0xEE, 0xF2, 255),
            "文字色回到初值");
    }

    std::printf("\n== 7. 换肤：复制 default 改名改 JSON ==\n");
    {
        // 造一个皮肤：只覆盖一部分 key，其余应当回退 default
        writeFile("resources/theme/test-skin/theme.json",
            R"({"name":"test-skin","version":1,"widgets":["widgets/button.json"]})");
        writeFile("resources/theme/test-skin/colors.json",
            R"({"accent":{"base":"#ff8800"},"surface":{"raised":"#101010"}})");
        writeFile("resources/theme/test-skin/metrics.json", R"({"radius":{"md":2}})");
        writeFile("resources/theme/test-skin/widgets/button.json",
            R"({"layers":[{"op":"rect","id":"bg","fill":"@theme/color.surface.raised",
                "radius":"@theme/metric.radius.md"}]})");

        check(setTheme("test-skin"), "切换到 test-skin 成功");
        checkEqual(getTheme().getName(), "test-skin", "当前主题名");

        checkColor(getTheme().color("color.accent.base"), IM_COL32(0xFF, 0x88, 0x00, 255),
            "皮肤覆盖的 key 用皮肤值");
        checkFloat(getTheme().number("metric.radius.md"), 2.0f, "皮肤覆盖的数值生效");
        // 皮肤没写的 key 回退 default
        checkColor(getTheme().color("color.text.primary"), IM_COL32(0xEB, 0xEE, 0xF2, 255),
            "皮肤没写的颜色回退 default");
        checkFloat(getTheme().number("metric.font.size.md"), 15.0f, "皮肤没写的数值回退 default");

        // 外观缓存必须跟着换（否则控件还在用旧外观）
        std::shared_ptr<const Appearance> skinned = getWidgetAppearance("button");
        check(skinned != nullptr, "换肤后能取到该皮肤的外观");
        if (skinned != nullptr)
        {
            check(skinned->getLayers().size() == 1, "皮肤里的按钮只有一层（覆盖了 default 的两层）");
            checkFloat(skinned->getLayers()[0].radius, 2.0f, "皮肤外观用了皮肤的圆角");
        }

        // 皮肤里没登记的控件外观：回退 default
        check(getWidgetAppearance("rectangle") != nullptr, "皮肤没写的控件回退 default 外观");

        check(setTheme("default"), "切回 default 成功");
    }

    std::printf("\n== 8. 随仓库发布的两套主题：每份外观都必须能编译 ==\n");
    {
        // 这条是给"@theme/ 引用写错"兜底的：写错 key 会被 Appearance::compile 拒绝
        // （不静默回退成洋红）。曾经因为把 @theme/color.a.b 写成 @theme/color/a.b，
        // 选中态整块变成洋红，就是这个错误。
        const char* widgets[] = { "rectangle", "button", "button.ghost", "header", "bottom_bar",
            "slider", "tab_cell", "dialog", "dialog_backdrop",
            "dropdown", "dropdown_item", "header.popup" };

        for (const char* themeName : { "default", "ocean" })
        {
            check(setTheme(themeName), std::string("切到主题 ") + themeName);
            for (const char* widget : widgets)
            {
                std::shared_ptr<const Appearance> appearance = getWidgetAppearance(widget);
                const bool ok = appearance != nullptr && !appearance->getLayers().empty();
                check(ok, std::string("  ") + themeName + " / " + widget + " 外观能编译且非空");
            }
        }
        setTheme("default");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

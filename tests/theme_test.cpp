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
        // 默认主题对齐 borealis 的暗色主题（Switch 风格）
        checkColor(theme.color("color.surface.page"), IM_COL32(0x2D, 0x2D, 0x2D, 255),
            "color.surface.page == #2d2d2d（brls/background）");
        checkColor(theme.color("color.surface.card"), IM_COL32(0x1F, 0x22, 0x27, 255),
            "color.surface.card == #1f2227（brls/highlight/background）");
        checkColor(theme.color("color.accent.base"), IM_COL32(0x00, 0xFF, 0xCC, 255),
            "color.accent.base == #00ffcc（brls/accent）");
        checkColor(theme.color("color.text.primary"), IM_COL32(0xFF, 0xFF, 0xFF, 255),
            "color.text.primary == #ffffff（brls/text）");
        checkColor(theme.color("color.state.focus"), IM_COL32(0x19, 0x8A, 0xC6, 255),
            "color.state.focus == #198ac6（brls/highlight/color1）");
        checkFloat(theme.number("metric.radius.md"), 5.0f, "metric.radius.md == 5（brls/button/corner_radius）");
        checkFloat(theme.number("metric.font.size.md"), 18.0f,
            "metric.font.size.md == 18（brls/header/font_size）");
        checkFloat(theme.number("metric.font.size.lg"), 22.0f,
            "metric.font.size.lg == 22（brls/sidebar/item_font_size）");
        checkFloat(theme.number("metric.control.height"), 48.0f,
            "metric.control.height == 48（brls/button 的 15+15 内边距）");
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
            // 新结构：底 / 高亮底 / 文字 / 辉光 / 描边（后三层是 borealis 的焦点高亮）
            check(button->getLayers().size() == 5, "五层：底 + 高亮底 + 文字 + 辉光 + 描边");
            if (button->getLayers().size() == 5)
            {
                const AppearanceLayer& bg    = button->getLayers()[0];
                const AppearanceLayer& title = button->getLayers()[2];

                checkEqual(button->getLayers()[1].id, "hlBg", "第 1 层是高亮底");
                checkEqual(button->getLayers()[3].id, "hlGlow", "第 3 层是辉光");
                checkEqual(button->getLayers()[4].id, "hlBorder", "第 4 层是描边");

                checkEqual(bg.id, "bg", "第 1 层 id");
                check(bg.op == AppearanceLayer::Op::RECT, "第 1 层是 rect");
                checkColor(bg.fill, IM_COL32(0x50, 0x50, 0x50, 255), "底：@theme/color.surface.raised");
                checkFloat(bg.radius, 5.0f, "底：圆角来自 @theme/metric.radius.md");

                checkEqual(title.id, "title", "第 2 层 id");
                check(title.op == AppearanceLayer::Op::TEXT, "第 2 层是 text");
                checkEqual(title.source, "text", "文字层的数据来源是 text");
                checkFloat(title.fontSize, 18.0f, "文字：字号来自主题");
                checkColor(title.textColor, IM_COL32(0xFF, 0xFF, 0xFF, 255), "文字：颜色来自主题");
            }

            // 盒模型也在 JSON 里（顶层 box 段）
            const AppearanceBox& box = button->getBox();
            check(box.hasHeight, "button 的 box 声明了 height");
            checkFloat(box.height, getTheme().number("metric.control.height", 0.0f),
                "button.box.height 解析成主题数值");
            check(box.hasMinWidth, "button 的 box 声明了 minWidth");

            // 百分比 / auto / 四边内边距
            std::shared_ptr<const Appearance> card = getWidgetAppearance("rectangle.card");
            check(card != nullptr && card->getBox().hasWidth && card->getBox().widthPercent,
                "card 的宽度是百分比");
            checkFloat(card->getBox().width, 100.0f, "card 宽度 100%");
            check(card->getBox().hasPadding, "card 声明了 padding");

            // 提示行本身只是"左侧内容"；高度与左右留白归底栏（borealis 的 hints/* 在 footer 上）
            std::shared_ptr<const Appearance> hints = getWidgetAppearance("hints_bar");
            check(hints != nullptr && hints->getBox().hasGap, "hints_bar 声明了条目间距");

            std::shared_ptr<const Appearance> bar = getWidgetAppearance("bottom_bar");
            check(bar != nullptr && bar->getBox().hasPadding, "bottom_bar 声明了 padding");
            if (bar != nullptr)
            {
                checkFloat(bar->getBox().padding[1],
                    getTheme().number("metric.hints.footerPaddingSides", 0.0f),
                    "底栏左右内边距来自主题（borealis footer_padding_sides）");
                check(bar->getBox().hasHeight && !bar->getBox().heightAuto,
                    "底栏高度是具体像素");
                check(bar->getBox().hasMargin, "底栏声明了左右外边距（footer_margin_sides）");
            }

            std::shared_ptr<const Appearance> shell = getWidgetAppearance("applet_frame");
            check(shell != nullptr && shell->getBox().hasBackground, "applet_frame 声明了底色");
            if (shell != nullptr)
                checkColor(shell->getBox().background, getTheme().color("color.surface.page"),
                    "外壳底色 = color.surface.page");

            check(button->getStateNames().size() == 3, "三个状态：focused / pressed / disabled");
            // borealis 的 hideHighlightBackground：primary/highlight/default 聚焦时**不出高亮底**，
            // 只有 bordered/borderless 才出。所以这里断言的条数不一样，不是漏了。
            check(button->getTransitions("focused").size() == 4,
                "default 按钮 focused 有四条（辉光 + 描边，无高亮底）");
            std::shared_ptr<const Appearance> bordered = getWidgetAppearance("button.bordered");
            check(bordered != nullptr && bordered->getTransitions("focused").size() == 5,
                "bordered 按钮 focused 有五条（含高亮底）");
        }
    }

    std::printf("\n== 5. 状态动效（tweeny 逐属性插值）==\n");
    {
        // 用 bordered：它聚焦时才有高亮底补间（default 是 hideHighlightBackground）
        std::shared_ptr<const Appearance> button = getWidgetAppearance("button.bordered");
        check(button != nullptr, "取到外观");

        AppearanceInstance instance;
        instance.setAppearance(button);
        // bordered（borealis 的 BORDERED）：**没有底**，只有描边
        checkColor(instance.getColor(0, AppearanceProp::FILL), 0, "bordered 没有底");
        checkColor(instance.getColor(0, AppearanceProp::STROKE),
            getTheme().color("color.button.defaultBorder"), "bordered 有描边（颜色来自主题）");

        const int hlBgIndex     = button->indexOfLayer("hlBg");
        const int hlBorderIndex = button->indexOfLayer("hlBorder");
        check(hlBgIndex > 0 && hlBorderIndex > 0, "找得到高亮层");
        checkColor(instance.getColor(hlBgIndex, AppearanceProp::FILL), 0,
            "初始不聚焦：高亮底是透明的");
        check(!instance.isAnimating(), "没触发状态时不该有补间在跑");

        instance.setState("focused", true);
        check(instance.isAnimating(), "聚焦后开始补间");

        // 目标色
        // 断言"补到了主题里那个颜色"，而不是写死色值：
        // 主题是数据，测试不该因为改色就被误伤
        const ImU32 focusedFill = getTheme().color("color.highlight.background");
        const ImU32 idleFill    = 0; // 高亮底初始是全透明

        // 推进 60ms（该动效 120ms），应该处在中间：既不是初值也不是终值
        instance.update(0.06f);
        const ImU32 mid = instance.getColor(hlBgIndex, AppearanceProp::FILL);
        check(mid != idleFill && mid != focusedFill, "60ms 时高亮底色在中间态（不是跳变）");
        check(instance.isAnimating(), "动效还没结束");

        // 描边宽度同样在补
        const float midStroke = instance.getNumber(hlBorderIndex, AppearanceProp::STROKE_WIDTH);
        check(midStroke > 0.0f && midStroke < 5.0f, "描边宽度在 0 与 5 之间");

        // 再推进到结束
        instance.update(0.2f);
        checkColor(instance.getColor(hlBgIndex, AppearanceProp::FILL), focusedFill,
            "结束值：高亮底 = color.highlight.background");
        checkFloat(instance.getNumber(hlBorderIndex, AppearanceProp::STROKE_WIDTH), 5.0f,
            "结束值：描边 5px（borealis 的 highlight/stroke_width）");
        checkColor(instance.getColor(hlBorderIndex, AppearanceProp::STROKE),
            getTheme().color("color.highlight.border"), "结束值：描边色 = color1");
        check(!instance.isAnimating(), "动效结束后不再是 animating");

        // 文字颜色同样按各自动效插值
        checkColor(instance.getColor(2, AppearanceProp::TEXT_COLOR),
            getTheme().color("color.button.defaultText"), "文字色保持按钮字色（高亮不改字色）");

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
            checkColor(quick.getColor(button->indexOfLayer("hlBg"), AppearanceProp::FILL), 0,
                "开了又马上关：停在初值（高亮底仍是透明），不会继续跑到状态目标");
            check(!quick.isAnimating(), "撤销后不该还有补间在跑");
        }

        instance.setState("focused", false);
        instance.update(0.3f);
        checkColor(instance.getColor(hlBgIndex, AppearanceProp::FILL), idleFill,
            "取消聚焦后高亮底回到透明");
        checkFloat(instance.getNumber(hlBorderIndex, AppearanceProp::STROKE_WIDTH), 0.0f,
            "取消聚焦后描边宽度归零");
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
        checkColor(getTheme().color("color.text.primary"), IM_COL32(0xFF, 0xFF, 0xFF, 255),
            "皮肤没写的颜色回退 default");
        checkFloat(getTheme().number("metric.font.size.md"), 18.0f, "皮肤没写的数值回退 default");

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
        const char* widgets[] = { "rectangle", "button", "button.ghost", "button.primary",
            "button.highlight", "button.bordered", "button.borderless", "header", "bottom_bar",
            "slider", "tab_cell", "dialog", "dialog_backdrop",
            "dropdown", "dropdown_item", "header.popup",
            "text_field", "edit_text_dialog", "hint_item", "hints_bar",
            "cell", "cell_detail", "cell_bool", "cell_radio", "cell_selector", "cell_slider",
            "cell_input" };

        for (const char* themeName : { "default", "ocean" })
        {
            check(setTheme(themeName), std::string("切到主题 ") + themeName);
            for (const char* widget : widgets)
            {
                std::shared_ptr<const Appearance> appearance = getWidgetAppearance(widget);
                // 非空 = 至少声明了 layers 或 box（纯布局控件可以只写 box）
                const bool ok = appearance != nullptr
                    && (!appearance->getLayers().empty() || appearance->getBox().hasHeight
                        || appearance->getBox().hasWidth || appearance->getBox().hasPadding);
                check(ok, std::string("  ") + themeName + " / " + widget + " 外观能编译且非空");

                // 层序：高亮底（hlBg）必须画在内容层（text/image/custom）之前，
                // 否则聚焦时"高亮底"会把内容盖住 —— 开关/单选的开关在聚焦后消失就是这个。
                if (appearance != nullptr)
                {
                    const auto& layers = appearance->getLayers();
                    int hlBgIndex = -1, contentIndex = -1;
                    for (size_t i = 0; i < layers.size(); i++)
                    {
                        const std::string& id = layers[i].id;
                        if (id == "hlBg" && hlBgIndex < 0)
                            hlBgIndex = (int)i;
                        if (contentIndex < 0 && (layers[i].op == AppearanceLayer::Op::TEXT
                                || layers[i].op == AppearanceLayer::Op::IMAGE
                                || layers[i].op == AppearanceLayer::Op::CUSTOM))
                            contentIndex = (int)i;
                    }
                    check(hlBgIndex < 0 || contentIndex < 0 || hlBgIndex < contentIndex,
                        std::string("  ") + themeName + " / " + widget
                            + "：hlBg 在内容层之前（否则聚焦会把内容盖住）");

                    // 阴影同理：必须画在自己的底色**之前**，画在后面会被不透明的底盖掉
                    int shadowIndex = -1, bgFillIndex = -1;
                    for (size_t i = 0; i < layers.size(); i++)
                    {
                        if (layers[i].op == AppearanceLayer::Op::SHADOW && shadowIndex < 0)
                            shadowIndex = (int)i;
                        else if (bgFillIndex < 0 && layers[i].op == AppearanceLayer::Op::RECT)
                            bgFillIndex = (int)i;
                    }
                    check(shadowIndex < 0 || bgFillIndex < 0 || shadowIndex < bgFillIndex,
                        std::string("  ") + themeName + " / " + widget
                            + "：shadow 层在底色之前（否则会被底盖住）");
                }
            }
        }
        setTheme("default");
    }

    std::printf("\n== 常用色调色板（color.common.*）==\n");
    {
        // widget JSON 里的原始色值统一放 common 组，最典型的是透明
        // （以前 "#00000000" 在 60 处重复）。缺 key 会编译失败，但这里直接断言，
        // 让人一眼看到"调色板是有人用的"。
        checkColor(getTheme().color("color.common.transparent", IM_COL32(1, 2, 3, 4)), 0,
            "common.transparent 是全透明");
        checkColor(getTheme().color("color.common.white", 0), IM_COL32(255, 255, 255, 255),
            "common.white");
        checkColor(getTheme().color("color.common.black", 0), IM_COL32(0, 0, 0, 255),
            "common.black");
        check(getTheme().color("color.common.overlay", 0) != 0, "common.overlay 有值（遮罩色）");
        check(getTheme().color("color.common.divider", 0) != 0, "common.divider 有值（分割线）");

        // 基础色板：红橙黄绿青蓝紫 + 黑/白/灰，各带 Light/Dark 档
        struct Palette
        {
            const char* name;
            ImU32 base;
        };
        const Palette hues[] = {
            { "red", IM_COL32(0xf4, 0x43, 0x36, 255) },
            { "orange", IM_COL32(0xff, 0x98, 0x00, 255) },
            { "yellow", IM_COL32(0xff, 0xeb, 0x3b, 255) },
            { "green", IM_COL32(0x4c, 0xaf, 0x50, 255) },
            { "cyan", IM_COL32(0x00, 0xbc, 0xd4, 255) },
            { "blue", IM_COL32(0x21, 0x96, 0xf3, 255) },
            { "purple", IM_COL32(0x9c, 0x27, 0xb0, 255) },
            { "pink", IM_COL32(0xe9, 0x1e, 0x63, 255) },
            { "brown", IM_COL32(0x79, 0x55, 0x48, 255) },
            { "grey", IM_COL32(0x9e, 0x9e, 0x9e, 255) },
        };
        for (const Palette& hue : hues)
        {
            const std::string base  = std::string("color.common.") + hue.name;
            const std::string light = base + "Light";
            const std::string dark  = base + "Dark";

            checkColor(getTheme().color(base, 0), hue.base,
                std::string("common.") + hue.name + " 是标准色");
            check(getTheme().color(light, 0) != 0 && getTheme().color(light, 0) != hue.base,
                std::string("common.") + hue.name + "Light 存在且与 base 不同");
            check(getTheme().color(dark, 0) != 0 && getTheme().color(dark, 0) != hue.base,
                std::string("common.") + hue.name + "Dark 存在且与 base 不同");
        }

        // 换到 ocean 也必须有（调色板每套主题各一份）
        setTheme("ocean");
        checkColor(getTheme().color("color.common.transparent", IM_COL32(1, 2, 3, 4)), 0,
            "ocean 主题也有 common.transparent");
        setTheme("default");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

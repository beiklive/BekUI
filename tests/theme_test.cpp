// 无窗口测试：主题与外观（JSON 描述绘制结构 + tweeny 状态动效）。
//
// 测试自己会在 <可执行文件目录>/resources/theme/ 下写一个 test-skin 主题，
// 用来验证「换肤 = 复制 default 改名改 JSON」与「缺 key 回退 default」，
// 不往仓库里塞测试专用资源。

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "bklv/compile/layer_registry.hpp"
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
        // 页面底色是**主题自己的选择**（改主题就跟着改这里的黄金值）；
        // 底线是它必须存在且不透明——半透明的页面底色会让整个界面透出清屏色
        const ImU32 page = theme.color("color.surface.page", 0);
        check((page >> 24) == 0xFF, "color.surface.page 存在且不透明");
        checkColor(page, IM_COL32(0xF2, 0xF3, 0xF5, 255), "color.surface.page == #f2f3f5");
        checkColor(theme.color("color.surface.card"), IM_COL32(0xFF, 0xFF, 0xFF, 255),
            "color.surface.card == #ffffff（brls/highlight/background）");
        checkColor(theme.color("color.accent.base"), IM_COL32(0x00, 0x7A, 0x63, 255),
            "color.accent.base == #007a63（brls/accent）");
        checkColor(theme.color("color.text.primary"), IM_COL32(0x1F, 0x23, 0x28, 255),
            "color.text.primary == #1f2328（brls/text）");
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
        std::shared_ptr<const Appearance> button = getWidgetAppearance("base/button/button");
        check(button != nullptr, "button 外观能取到");
        if (button != nullptr)
        {
            // 结构断言：按 id 找层，不钉死层数/顺序（删装饰层不该让测试红）
            const int bgIndex    = button->indexOfLayer("bg");
            const int titleIndex = button->indexOfLayer("title");
            check(bgIndex >= 0, "有底（bg）");
            check(titleIndex >= 0, "有文字（title）");
            check(button->indexOfLayer("hlBg") >= 0, "有高亮底（hlBg）");
            if (bgIndex >= 0)
            {
                const AppearanceLayer& bg = button->getLayers()[(size_t)bgIndex];
                check(bg.op == AppearanceLayer::Op::RECT, "底是 rect");
                checkColor(bg.fill, getTheme().color("color.button.defaultBg"), "底：按钮默认底色");
                checkFloat(bg.radius, 5.0f, "底：圆角来自 @theme/metric.radius.md");
            }
            if (titleIndex >= 0)
            {
                const AppearanceLayer& title = button->getLayers()[(size_t)titleIndex];
                check(title.op == AppearanceLayer::Op::TEXT, "文字层是 text");
                checkEqual(title.dataId, "text", "文字层的数据来源是 text");
                checkFloat(title.fontSize, getTheme().number("metric.button.textSize", 0.0f),
                    "文字：字号来自主题");
                checkColor(title.textColor, getTheme().color("color.button.defaultText"),
                    "文字：颜色来自主题");
            }
        }
    }

    std::printf("\n== 5. 状态动效（tweeny 逐属性插值）==\n");
    {
        // 用默认按钮的 pressed：底从 defaultBg 补到 defaultPressedBg（60ms）
        std::shared_ptr<const Appearance> button = getWidgetAppearance("base/button/button");
        check(button != nullptr, "取到外观");

        AppearanceInstance instance;
        instance.setAppearance(button);

        const int bgIndex       = button->indexOfLayer("bg");
        const ImU32 idleFill    = getTheme().color("color.button.defaultBg");
        const ImU32 pressedFill = getTheme().color("color.button.defaultPressedBg");

        checkColor(instance.getColor(bgIndex, AppearanceProp::FILL), idleFill,
            "初始：按钮默认底色");
        check(!instance.isAnimating(), "没触发状态时不该有补间在跑");

        instance.setState("pressed", true);
        check(instance.isAnimating(), "按下后开始补间");

        instance.update(0.03f); // 动效 60ms，走一半
        const ImU32 mid = instance.getColor(bgIndex, AppearanceProp::FILL);
        check(mid != idleFill && mid != pressedFill, "30ms 时底色在中间态（不是跳变）");
        check(instance.isAnimating(), "动效还没结束");

        instance.update(0.1f);
        checkColor(instance.getColor(bgIndex, AppearanceProp::FILL), pressedFill,
            "结束值：按下底色");
        check(!instance.isAnimating(), "动效结束后不再是 animating");

        // 文字色同样按补间走（pressed 里也写了 title.color）
        checkColor(instance.getColor(button->indexOfLayer("title"), AppearanceProp::TEXT_COLOR),
            getTheme().color("color.button.defaultText"), "文字色保持按钮字色");

        std::printf("\n== 6. 状态取消后回到初值 ==\n");
        // 刚按下就松开：当前值还等于初值时，旧补间必须被撤掉
        {
            AppearanceInstance quick;
            quick.setAppearance(button);
            quick.setState("pressed", true);
            quick.setState("pressed", false);
            for (int i = 0; i < 30; i++)
                quick.update(0.05f);
            checkColor(quick.getColor(bgIndex, AppearanceProp::FILL), idleFill,
                "按下又立刻松开：停在默认底色，不会继续跑到按下色");
            check(!quick.isAnimating(), "撤销后不该还有补间在跑");
        }

        instance.setState("pressed", false);
        instance.update(0.3f);
        checkColor(instance.getColor(bgIndex, AppearanceProp::FILL), idleFill,
            "松开后回到默认底色");
    }

    std::printf("\n== 7. 换肤：复制 default 改名改 JSON ==\n");
    {
        // 造一个皮肤：只覆盖一部分 key，其余应当回退 default
        // 新布局：tokens/ 放色板与数值、base/ 放基础控件外观，清单写在 files[]
        writeFile("resources/theme/test-skin/theme.json",
            R"({"name":"test-skin","version":1,"author":"测试","description":"覆盖测试用皮肤",
                "files":["tokens/colors.json","tokens/metrics.json","base/button/button.json"]})");
        writeFile("resources/theme/test-skin/tokens/colors.json",
            R"({"accent":{"base":"#ff8800"},"surface":{"raised":"#101010"}})");
        writeFile("resources/theme/test-skin/tokens/metrics.json", R"({"radius":{"md":2}})");
        writeFile("resources/theme/test-skin/base/button/button.json",
            R"({"layers":[{"op":"rect","id":"bg","fill":"@theme/color.surface.raised",
                "radius":"@theme/metric.radius.md"}]})");

        check(setTheme("test-skin"), "切换到 test-skin 成功");
        checkEqual(getTheme().getName(), "test-skin", "当前主题名");

        checkColor(getTheme().color("color.accent.base"), IM_COL32(0xFF, 0x88, 0x00, 255),
            "皮肤覆盖的 key 用皮肤值");
        checkFloat(getTheme().number("metric.radius.md"), 2.0f, "皮肤覆盖的数值生效");
        // 皮肤没写的 key 回退 default
        checkColor(getTheme().color("color.text.primary"), IM_COL32(0x1F, 0x23, 0x28, 255),
            "皮肤没写的颜色回退 default");
        checkFloat(getTheme().number("metric.font.size.md"), 18.0f, "皮肤没写的数值回退 default");

        // 外观缓存必须跟着换（否则控件还在用旧外观）
        std::shared_ptr<const Appearance> skinned = getWidgetAppearance("base/button/button");
        check(skinned != nullptr, "换肤后能取到该皮肤的外观");
        if (skinned != nullptr)
        {
            check(skinned->getLayers().size() == 1, "皮肤里的按钮只有一层（覆盖了 default 的两层）");
            checkFloat(skinned->getLayers()[0].radius, 2.0f, "皮肤外观用了皮肤的圆角");
        }

        // 皮肤里没登记的控件外观：回退 default
        check(getWidgetAppearance("base/rectangle") != nullptr, "皮肤没写的控件回退 default 外观");

        check(setTheme("default"), "切回 default 成功");
    }

    std::printf("\n== 8. 随仓库发布的两套主题：每份外观都必须能编译 ==\n");
    {
        // 这条是给"@theme/ 引用写错"兜底的：写错 key 会被 Appearance::compile 拒绝
        // （不静默回退成洋红）。曾经因为把 @theme/color.a.b 写成 @theme/color/a.b，
        // 选中态整块变成洋红，就是这个错误。
        // 名单直接用主题清单（files[]），不再手抄：加/删外观文件时这条测试自动跟上
        for (const char* themeName : { "default", "ios" })
        {
            check(setTheme(themeName), std::string("切到主题 ") + themeName);

            // 外观名清单来自主题自己登记的 files[]（base/ 与 widget/ 两类）
            const std::vector<std::string>& names = getTheme().getWidgetNames();
            check(names.size() >= 40, std::string("  ") + themeName + " 登记的外观够多");

            for (const std::string& widget : names)
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

                    // 阴影的层序要分内外（两者要求**相反**）：
                    //   · 外投影：画在底色**之前**（垫在控件下面），画在后面会被不透明的底盖掉
                    //   · 内阴影：画在底色**之后**（它画的是内缘，被底盖住就等于没画）
                    // 解析期会对这两类分别处理（见 core/appearance.cpp 的 rankOf），
                    // 所以这里断言的是**排序之后**的结果，作者按书写顺序写对即可。
                    int shadowIndex = -1, innerShadowIndex = -1, bgFillIndex = -1;
                    for (size_t i = 0; i < layers.size(); i++)
                    {
                        if (layers[i].op == AppearanceLayer::Op::SHADOW)
                        {
                            if (layers[i].inner)
                            {
                                if (innerShadowIndex < 0)
                                    innerShadowIndex = (int)i;
                            }
                            else if (shadowIndex < 0)
                                shadowIndex = (int)i;
                        }
                        else if (bgFillIndex < 0 && layers[i].op == AppearanceLayer::Op::RECT)
                            bgFillIndex = (int)i;
                    }
                    check(shadowIndex < 0 || bgFillIndex < 0 || shadowIndex < bgFillIndex,
                        std::string("  ") + themeName + " / " + widget
                            + "：外投影在底色之前（否则会被底盖住）");
                    check(innerShadowIndex < 0 || bgFillIndex < 0 || innerShadowIndex > bgFillIndex,
                        std::string("  ") + themeName + " / " + widget
                            + "：内阴影在底色之后（它画的是内缘，被底盖住等于没画）");
                }
            }
        }
        setTheme("default");
    }

    std::printf("\n== 9. 主题清单反向校验：磁盘上的 .json 必须都登记在 files[] 里 ==\n");
    {
        // 第 8 节是「清单 → 编译」（files[] 里登记的都要能编译），缺的是反方向
        // 「磁盘 → 清单」：主题的 files[] 是**显式清单**（romfs 上不列目录，见 AGENTS.md），
        // 所以"文件写好了但忘了登记"不会报错 —— 运行时会以"主题里没有这份外观"的形式
        // 静默回退到 default，改了半天不生效；少写 `.json` 后缀也是同一个现象。
        // CMake 侧的源清单漏项在 configure 期就会失败，主题侧原来没有对应物，这里补上。
        for (const char* themeName : { "default", "ios" })
        {
            const std::string themeDir = std::string("resources/theme/") + themeName;

            std::ifstream in(resourcePath(themeDir + "/theme.json"), std::ios::binary);
            if (!in)
            {
                check(false, std::string(themeName) + " 有 theme.json");
                continue;
            }
            const nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
            check(!doc.is_discarded(), std::string(themeName) + " 的 theme.json 语法合法");
            if (doc.is_discarded())
                continue;

            // 登记项：原样比较（**带 .json 后缀**，名字 = 相对主题根的路径）
            std::vector<std::string> listed;
            for (const auto& item : doc.value("files", nlohmann::json::array()))
                if (item.is_string())
                    listed.push_back(item.get<std::string>());

            // 磁盘上的全部 .json（theme.json 自己是元信息，不登记）
            //
            // **只走清单用到的四个目录**（tokens / base / widget / view）：dist/resources 是
            // `copy_directory` 拷出来的，主题目录改过布局（旧的扁平 `colors.json` / `metrics.json`
            // / `widgets/` → 现在的 `tokens/ base/ widget/ view/`），残留文件属于"复制的卫生问题"
            // （已改成先删后拷），不该在这里判成清单错误 —— 否则换台机器跑就是一堆假失败。
            std::vector<std::string> onDisk;
            std::error_code ec;
            const std::filesystem::path root = resourcePath(themeDir);
            for (const char* sub : { "tokens", "base", "widget", "view" })
            {
                const std::filesystem::path dir = root / sub;
                if (!std::filesystem::exists(dir, ec))
                    continue;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec))
                {
                    if (!entry.is_regular_file() || entry.path().extension() != ".json")
                        continue;
                    const std::string rel =
                        std::filesystem::relative(entry.path(), root, ec).generic_string();
                    if (!rel.empty())
                        onDisk.push_back(rel);
                }
            }

            for (const std::string& file : onDisk)
            {
                const bool found = std::find(listed.begin(), listed.end(), file) != listed.end();
                check(found, std::string(themeName) + " / " + file + " 已登记进 files[]");
            }
            // 反方向也查：登记了但磁盘上没有（清单里留了已删文件的名字）
            for (const std::string& file : listed)
            {
                const bool found = std::find(onDisk.begin(), onDisk.end(), file) != onDisk.end();
                check(found, std::string(themeName) + " / " + file + " 在磁盘上存在");
            }
            check(listed.size() == onDisk.size(),
                std::string(themeName) + " files[] 条数与磁盘 .json 数一致（"
                    + std::to_string(listed.size()) + " vs " + std::to_string(onDisk.size()) + "）");
        }
    }

    std::printf("\n== 10. 代码里直接引用的主题 key 必须存在 ==\n");
    {
        // 外观 JSON 里的 @theme/ 引用写错会**编译失败**（第 8 节兜住）；但 C++ 里
        // `getTheme().color(key)` / `.number(key)` 直接引用的 key 少了不会编译失败，
        // 只在运行期记一条日志然后不给颜色/不给尺寸（"代码里写死的尺寸"往往就是这么来的）。
        // 这里把代码里直接引用的 key 钉住：改名或换主题时立刻失败，而不是界面上悄悄少一块。
        const char* colors[] = {
            "color.highlight.background", // Cell 家族与 JSONRecyclerCell 的聚焦底
            "color.surface.card",         // JSONRecyclerCell 的常态底
            "color.cell.background",      // Cell 家族底色
            "color.sidebar.separator",    // 侧栏分组分隔线
        };
        const char* numbers[] = {
            "metric.progressSpinner.size",
            "metric.scroll.paddingV",
            "metric.scroll.paddingH",
            "metric.status.itemMarginTop",
            "metric.editTextDialog.caretWidth",
            "metric.sidebar.separatorThickness",
            "metric.sidebar.separatorHeight",
        };
        for (const char* themeName : { "default", "ios" })
        {
            check(setTheme(themeName), std::string("切到主题 ") + themeName);
            for (const char* key : colors)
                check(getTheme().hasColor(key), std::string(themeName) + " 有颜色 " + key);
            for (const char* key : numbers)
                check(getTheme().hasNumber(key), std::string(themeName) + " 有数值 " + key);
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

        // 换到 ios 也必须有（调色板每套主题各一份）
        setTheme("ios");
        checkColor(getTheme().color("color.common.transparent", IM_COL32(1, 2, 3, 4)), 0,
            "ios 主题也有 common.transparent");
        setTheme("default");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

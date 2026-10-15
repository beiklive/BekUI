#include <string>

#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

// 外壳开关页：原来这三个按钮挂在内容列顶部，等于所有页面都带着 —— 挪进单独页面，
// 想看的时候切过来点。它们操作的是**外壳级**状态（页头/底栏显隐、日志窗口），
// 走的是既有的 PageDeps（页面 → 外壳唯一的那条路），不直接摸 Shell。

namespace
{

// 一行开关：按钮 + 右侧当前状态
Box* makeSwitchRow(const char* titleKey, const char* onKey, const char* offKey, bool on,
    std::function<void()> onToggle)
{
    const ThemeData& theme = getTheme();

    Box* row = makeCard();
    Box* line = makeRow(theme.number("metric.spacing.md", 12.0f), AlignItems::CENTER);

    Label* title = makeText(getStr(titleKey), theme.number("metric.font.size.md", 18.0f),
        theme.color("color.text.primary"));
    line->addView(title);

    Padding* spacer = new Padding();
    line->addView(spacer);

    Label* state = makeText(getStr(on ? onKey : offKey),
        theme.number("metric.font.size.sm", 15.0f), theme.color("color.text.dim"));
    line->addView(state);

    Button* button = new Button(getStr("shell/toggle"));
    button->setOnClick([button, state, on, onKey, offKey, onToggle = std::move(onToggle)] {
        onToggle();
        // 按钮本身不改状态，状态由外壳回读 —— 这里直接翻一下显示（外壳是同步的）
        const bool now = !on;
        button->setText(getStr("shell/toggle"));
        state->setText(getStr(now ? onKey : offKey));
    });
    line->addView(button);

    row->addView(line);
    return row;
}

} // namespace

Box* buildShellPage()
{
    const PageDeps& deps = getPageDeps();

    Box* page = makeColumn(getTheme().number("metric.spacing.sm", 8.0f));
    page->addView(makeHint(getStr("shell/intro")));

    View* section = nullptr;
    {
        AppearanceView* header = new AppearanceView("base/section", false);
        header->setText(getStr("shell/section_bars"));
        section = header;
    }
    page->addView(section);
    page->addView(makeHint(getStr("shell/bars_hint")));

    const Shell* shell = deps.shell;
    const bool headerVisible = shell != nullptr ? shell->isHeaderVisible() : true;
    const bool footerVisible = shell != nullptr ? shell->isFooterVisible() : true;

    page->addView(makeSwitchRow("shell/header", "shell/on", "shell/off", headerVisible, [&deps, headerVisible] {
        if (deps.setHeaderVisible != nullptr)
            deps.setHeaderVisible(!headerVisible);
    }));
    page->addView(makeSwitchRow("shell/footer", "shell/on", "shell/off", footerVisible, [&deps, footerVisible] {
        if (deps.setFooterVisible != nullptr)
            deps.setFooterVisible(!footerVisible);
    }));

    {
        AppearanceView* logSection = new AppearanceView("base/section", false);
        logSection->setText(getStr("shell/section_debug"));
        page->addView(logSection);
    }
    page->addView(makeHint(getStr("shell/debug_hint")));

    const bool logVisible = deps.getLogWindowVisible != nullptr && deps.getLogWindowVisible();
    page->addView(makeSwitchRow("shell/log", "shell/on", "shell/off", logVisible, [&deps, logVisible] {
        if (deps.setLogWindowVisible != nullptr)
            deps.setLogWindowVisible(!logVisible);
    }));

    return page;
}

} // namespace demo

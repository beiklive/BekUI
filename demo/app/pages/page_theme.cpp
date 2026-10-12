#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/core/app.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/rectangle.hpp"

using namespace bklv;

namespace demo
{

namespace
{
App* g_app = nullptr;
} // namespace

void setThemeSwitchApp(App* app)
{
    g_app = app;
}

Box* buildThemePage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("theme/hint")));

    // ---- 换肤 ----
    Rectangle* switchCard = makeCard();
    switchCard->addView(makeText(getStr("theme/switch_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    Label* current = makeHint(getStr("theme/current", getTheme().getName()));

    Box* switchRow = makeRow(getTheme().number("metric.spacing.sm", 8.0f));
    for (const std::string& name : getTheme().getAvailableThemes())
    {
        Button* button = new Button(name);
        button->setOnClick([current, name] {
            if (g_app != nullptr && g_app->setTheme(name))
                current->setText(getStr("theme/current", getTheme().getName()));
        });
        switchRow->addView(button);
    }
    switchCard->addView(switchRow);
    switchCard->addView(current);

    // ---- 外观驱动的控件 ----
    switchCard->addView(makeText(getStr("theme/widgets_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));
    switchCard->addView(makeHint(getStr("theme/widgets_desc")));

    Box* widgetsRow = makeRow(getTheme().number("metric.spacing.md", 12.0f));

    Rectangle* accent = new Rectangle(); // 纯装饰：一行绘制代码都没有
    accent->setDimensions(120.0f, getTheme().number("metric.control.height", 48.0f));
    widgetsRow->addView(accent);

    Button* normal = new Button(getStr("theme/button_normal"));
    widgetsRow->addView(normal);

    Button* ghost = new Button(getStr("theme/button_ghost"), "ghost");
    widgetsRow->addView(ghost);

    Button* disabled = new Button(getStr("theme/button_disabled"));
    disabled->setEnabled(false);
    widgetsRow->addView(disabled);

    switchCard->addView(widgetsRow);
    page->addView(switchCard);

    // ---- 状态动效 ----
    Rectangle* stateCard = makeCard();
    stateCard->addView(makeText(getStr("theme/states_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));
    stateCard->addView(makeHint(getStr("theme/states_desc")));

    Box* stateRow = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    for (int i = 0; i < 3; ++i)
        stateRow->addView(new Button(getStr("theme/focus_sample", i + 1)));
    stateCard->addView(stateRow);
    page->addView(stateCard);

    return page;
}

} // namespace demo

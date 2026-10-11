#include "app/pages/pages.hpp"

#include <string>

#include "app/page_helpers.hpp"
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

// 页面要能切主题，所以得拿到 App。main.cpp 在建 PageHost 之前塞进来。
App* g_app = nullptr;

// 跟随主题的面板：底色来自当前主题。换肤时 applyTheme() 会被调用，
// 所以不用重建视图——这就是 `View::applyTheme()` 的用处。
class ThemePanel : public Box
{
  public:
    ThemePanel()
        : Box(Axis::COLUMN)
    {
        this->setWidthPercentage(100.0f);
        this->setHeight(AUTO);
        this->setPadding(16.0f);
        this->setGap(12.0f);
        this->setCornerRadius(getTheme().number("metric.radius.lg", 10.0f));
        this->applyTheme();
    }

    void applyTheme() override
    {
        this->setBackgroundColor(getTheme().color("color.surface.card"));
        this->setCornerRadius(getTheme().number("metric.radius.lg", 10.0f));
    }
};

} // namespace

void setThemeSwitchApp(App* app)
{
    g_app = app;
}

Box* buildThemePage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);

    page->addView(makeTitle(getStr("tab/theme")));
    page->addView(makeHint(getStr("theme/hint")));

    // ---- 1. 换肤 ----
    ThemePanel* switchPanel = new ThemePanel();
    switchPanel->addView(makeText(getStr("theme/switch_title"), 16.0f, rgb(200, 208, 220)));

    Label* current = makeText(getStr("theme/current", getTheme().getName()), 14.0f,
        getTheme().color("color.text.dim"));
    current->setTextColorExplicit(false); // 跟随主题

    Box* row = makeRow(10.0f);
    row->setAlignItems(AlignItems::CENTER);

    for (const std::string& name : getTheme().getAvailableThemes())
    {
        Button* button = new Button(name);
        button->setOnClick([current, name] {
            if (g_app != nullptr && g_app->setTheme(name))
                current->setText(getStr("theme/current", getTheme().getName()));
        });
        row->addView(button);
    }
    switchPanel->addView(row);
    switchPanel->addView(current);
    page->addView(switchPanel);

    // ---- 2. 外观驱动的控件：绘制结构全在 JSON 里 ----
    ThemePanel* widgetPanel = new ThemePanel();
    widgetPanel->addView(makeText(getStr("theme/widgets_title"), 16.0f, rgb(200, 208, 220)));
    widgetPanel->addView(
        makeText(getStr("theme/widgets_desc"), 14.0f, getTheme().color("color.text.dim")));

    Box* widgetRow = makeRow(12.0f);
    widgetRow->setAlignItems(AlignItems::CENTER);

    Rectangle* accent = new Rectangle();
    accent->setDimensions(120.0f, 44.0f);
    widgetRow->addView(accent);

    Button* normal = new Button(getStr("theme/button_normal"));
    normal->setOnClick([] {});
    widgetRow->addView(normal);

    Button* ghost = new Button(getStr("theme/button_ghost"), "ghost");
    ghost->setOnClick([] {});
    widgetRow->addView(ghost);

    Button* disabled = new Button(getStr("theme/button_disabled"));
    disabled->setEnabled(false);
    widgetRow->addView(disabled);

    widgetPanel->addView(widgetRow);
    page->addView(widgetPanel);

    // ---- 3. 状态动效：焦点/按下都由外观 JSON 描述、tweeny 插值 ----
    ThemePanel* statePanel = new ThemePanel();
    statePanel->addView(makeText(getStr("theme/states_title"), 16.0f, rgb(200, 208, 220)));
    statePanel->addView(makeText(getStr("theme/states_desc"), 14.0f, getTheme().color("color.text.dim")));

    Box* stateRow = makeRow(12.0f);
    stateRow->setAlignItems(AlignItems::CENTER);
    for (int i = 0; i < 3; ++i)
    {
        Button* focusable = new Button(getStr("theme/focus_sample", i + 1));
        focusable->setOnClick([] {});
        stateRow->addView(focusable);
    }
    statePanel->addView(stateRow);
    page->addView(statePanel);

    return page;
}

} // namespace demo

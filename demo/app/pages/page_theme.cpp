#include "app/pages/pages.hpp"

#include <string>

#include "app/page_helpers.hpp"
#include "bklv/core/app.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/bottom_bar.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/dialog.hpp"
#include "bklv/widgets/dropdown.hpp"
#include "bklv/widgets/header.hpp"
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

    // 页头：色条 + 标题 + 副标题 + 下边线，结构全在 widgets/header.json 里
    Header* header = new Header();
    header->setTitle(getStr("tab/theme"));
    header->setSubtitle(getTheme().getName());
    page->addView(header);

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
        button->setOnClick([current, header, name] {
            if (g_app != nullptr && g_app->setTheme(name))
            {
                current->setText(getStr("theme/current", getTheme().getName()));
                header->setSubtitle(getTheme().getName());
            }
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

    // ---- 4. 弹窗：遮罩 + 面板 + 按钮行，开关也是外观状态动效 ----
    ThemePanel* dialogPanel = new ThemePanel();
    dialogPanel->addView(makeText(getStr("theme/dialog_title"), 16.0f, rgb(200, 208, 220)));
    dialogPanel->addView(makeText(getStr("theme/dialog_desc"), 14.0f, getTheme().color("color.text.dim")));

    // 复用同一个实例：关掉之后还能再开，不用每次新建
    static Dialog* dialog = nullptr;
    Button* openDialog     = new Button(getStr("theme/open_dialog"));
    openDialog->setOnClick([] {
        if (dialog == nullptr)
        {
            dialog = new Dialog();
            dialog->setTitle(getStr("theme/dialog_demo_title"));
            dialog->setText(getStr("theme/dialog_demo_text"));
            dialog->addButton(getStr("hints/ok"), []( ) {
                if (dialog != nullptr)
                    dialog->close();
            });
            dialog->addButton(getStr("theme/dialog_cancel"), [] {
                if (dialog != nullptr)
                    dialog->close();
            });
        }
        dialog->open();
    });
    Box* dialogRow = makeRow(12.0f);
    dialogRow->setAlignItems(AlignItems::CENTER);
    dialogRow->addView(openDialog);
    dialogPanel->addView(dialogRow);
    page->addView(dialogPanel);

    // ---- 5. 下拉：触发器 + 弹层列表（列表锚在触发器下方，可滚动）----
    ThemePanel* dropdownPanel = new ThemePanel();
    dropdownPanel->addView(makeText(getStr("theme/dropdown_title"), 16.0f, rgb(200, 208, 220)));
    dropdownPanel->addView(
        makeText(getStr("theme/dropdown_desc"), 14.0f, getTheme().color("color.text.dim")));

    Label* picked = makeText(getStr("theme/dropdown_picked", getStr("theme/option_1")), 14.0f,
        getTheme().color("color.text.dim"));
    picked->setTextColorExplicit(false);

    Dropdown* dropdown = new Dropdown();
    dropdown->setTitle(getStr("theme/dropdown_label"));
    dropdown->setOptions({ getStr("theme/option_1"), getStr("theme/option_2"),
        getStr("theme/option_3"), getStr("theme/option_4") });
    dropdown->setOnChange([picked, dropdown](size_t) {
        picked->setText(getStr("theme/dropdown_picked", dropdown->getSelectedText()));
    });

    Box* dropdownRow = makeRow(16.0f);
    dropdownRow->setAlignItems(AlignItems::CENTER);
    Box* dropdownSlot = new Box(Axis::COLUMN);
    dropdownSlot->setWidth(320.0f);
    dropdownSlot->addView(dropdown);
    dropdownRow->addView(dropdownSlot);
    dropdownRow->addView(picked);
    dropdownPanel->addView(dropdownRow);
    page->addView(dropdownPanel);

    // ---- 6. 底栏：上边线 + 一行提示（结构同样来自 JSON）----
    BottomBar* bottom = new BottomBar();
    bottom->showHintsFromFocus();
    if (bottom->getText().empty()) // 还没有焦点时没有提示，用静态文案兜底
        bottom->setText(getStr("theme/bottom_hint"));
    page->addView(bottom);

    return page;
}

} // namespace demo

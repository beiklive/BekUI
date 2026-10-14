#include "app/pages.hpp"

#include <string>
#include <vector>

#include "bklv/bklv.hpp"
#include "bklv/core/icon_font.hpp"

#include "app/ui.hpp"

using namespace bklv;

namespace demo
{

namespace
{

// 组件容器的底（base/box.component）：演示"改 base 就改所有衍生控件"
Rectangle* makeComponentBox()
{
    Rectangle* box = new Rectangle();
    box->setAppearanceName("base/box");
    box->setAppearanceVariant("component");
    return box;
}

// 布局容器（base/box.layout）：无边框无背景，只有默认内边距
Rectangle* makeLayoutBox()
{
    Rectangle* box = new Rectangle();
    box->setAppearanceName("base/box");
    box->setAppearanceVariant("layout");
    return box;
}

// 组合按钮：外观名就是主题里的完整路径（widget/button.text 等）
Button* makeVariantButton(const std::string& variant, const std::string& text)
{
    Button* button = new Button(text);
    button->setAppearanceName("widget/button." + variant);
    return button;
}

// Material 图标字形（控件图标用 Material，不用 switch 那套按键图标）
std::string glyph(const char* name)
{
    const char* found = icon::glyphByName(name);
    if (found != nullptr && found[0] != 0)
        return found;

    const std::vector<icon::Glyph>& all = icon::allGlyphs();
    return all.empty() ? std::string() : all.front().utf8;
}

// 一行一个控件：整行铺满，控件 grow=1（不并排挤在一起）
Box* makeItemRow(View* item)
{
    Box* row = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    item->setGrow(1.0f);
    row->addView(item);
    return row;
}

} // namespace

Box* buildBaseWidgetsPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("base_widgets/hint")));

    // ---------------------------------------------------------------- 容器
    Rectangle* containerCard = makeCard();
    containerCard->addView(makeText(getStr("base_widgets/containers"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    // 布局容器：无边框、无背景、有默认内边距 —— 里面放标签 + 一个组件容器
    Rectangle* layout = makeLayoutBox();
    layout->setAxis(Axis::ROW);
    layout->setAlignItems(AlignItems::CENTER);
    layout->addView(makeText("layout", getTheme().number("metric.font.size.sm", 15.0f),
        getTheme().color("color.text.dim")));
    Rectangle* innerComponent = makeComponentBox();
    innerComponent->addView(makeText(getStr("base_widgets/component_inner"),
        getTheme().number("metric.font.size.sm", 15.0f), getTheme().color("color.text.primary")));
    layout->addView(innerComponent);
    containerCard->addView(makeItemRow(layout));

    page->addView(containerCard);

    // ---------------------------------------------------------------- 按钮变体
    Rectangle* buttonCard = makeCard();
    buttonCard->addView(makeText(getStr("base_widgets/buttons"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    // 普通按钮：组件容器 + 居中的文本
    buttonCard->addView(makeItemRow(makeVariantButton("text", getStr("base_widgets/btn_text"))));

    // 图标按钮：只有图标（Material）
    Button* iconOnly = makeVariantButton("icon", "");
    iconOnly->setIcon(glyph("star"));
    buttonCard->addView(makeItemRow(iconOnly));

    // 图标文字按钮：图标 + 文字
    Button* iconText = makeVariantButton("icon_text", getStr("base_widgets/btn_icon_text"));
    iconText->setIcon(glyph("save"));
    buttonCard->addView(makeItemRow(iconText));

    // 下拉框按钮：点它弹遮罩列表（复用 Dropdown 的弹层），外观换成组合变体
    Dropdown* dropdown = new Dropdown();
    dropdown->setOptions({ getStr("base_widgets/option_a"), getStr("base_widgets/option_b"),
        getStr("base_widgets/option_c") }, 0);
    AppearanceView* trigger = dropdown->getTrigger();
    trigger->setAppearanceName("widget/button.dropdown");
    trigger->setIcon(glyph("list"));
    trigger->setText(getStr("base_widgets/btn_dropdown"));
    trigger->setInfoText(dropdown->getSelectedText());
    dropdown->setOnChange([dropdown, trigger](size_t) {
        trigger->setInfoText(dropdown->getSelectedText());
    });
    buttonCard->addView(makeItemRow(dropdown));

    // 文字开关按钮：信息文字在开/关之间切换，颜色不同（外观里两个状态）
    Button* switchText = makeVariantButton("switch_text", getStr("base_widgets/btn_switch_text"));
    switchText->setIcon(glyph("info"));
    switchText->setSwitched(false);
    switchText->setInfoText(getStr("base_widgets/off"));
    switchText->setOnClick([switchText] {
        switchText->setSwitched(!switchText->isSwitched());
        switchText->setInfoText(switchText->isSwitched() ? getStr("base_widgets/on")
                                                        : getStr("base_widgets/off"));
    });
    buttonCard->addView(makeItemRow(switchText));

    // 图案开关按钮：轨道 + 滑块，切换有过渡动画（外观 on/off 状态里是补间）
    Button* switchIcon = makeVariantButton("switch_icon", getStr("base_widgets/btn_switch_icon"));
    switchIcon->setIcon(glyph("settings"));
    switchIcon->setSwitched(true);
    switchIcon->setOnClick([switchIcon] { switchIcon->setSwitched(!switchIcon->isSwitched()); });
    buttonCard->addView(makeItemRow(switchIcon));

    // 信息按钮：信息文字由程序决定
    Button* info = makeVariantButton("info", getStr("base_widgets/btn_info"));
    info->setIcon(glyph("home"));
    info->setInfoText(getStr("base_widgets/info_value"));
    info->setOnClick([info] {
        static int clicks = 0;
        clicks++;
        info->setInfoText(std::to_string(clicks));
    });
    buttonCard->addView(makeItemRow(info));

    // 选择器（数值模式）：进入设置态后焦点钉住，LR/方向键调值，长按连发
    SelectorButton* number = new SelectorButton(getStr("base_widgets/btn_selector_number"));
    number->setIcon(glyph("palette"));
    number->setNumberRange(0.0f, 100.0f, 5.0f, 1, 40.0f);
    number->setOnValueChanged([](float value) {
        BKLV_LOG_INFO("选择器（数值）：%.1f", (double)value);
    });
    buttonCard->addView(makeItemRow(number));

    // 选择器（列表模式）：LR 切换选项，越界环绕（箭头显示成 L / R）
    SelectorButton* list = new SelectorButton(getStr("base_widgets/btn_selector_list"));
    list->setIcon(glyph("list"));
    list->setOptions({ getStr("base_widgets/option_a"), getStr("base_widgets/option_b"),
        getStr("base_widgets/option_c") });
    list->setArrowStyle(SelectorArrow::LR);
    list->setOnSelectionChanged([](int index, const std::string& text) {
        BKLV_LOG_INFO("选择器（列表）：" + std::to_string(index) + " " + text);
    });
    buttonCard->addView(makeItemRow(list));

    buttonCard->addView(makeHint(getStr("base_widgets/selector_hint")));
    page->addView(buttonCard);

    return page;
}

} // namespace demo

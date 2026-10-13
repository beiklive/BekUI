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

// 组合按钮：基名 widget/button + 变体名（组合外观全在主题 JSON 里）
Button* makeVariantButton(const std::string& variant, const std::string& text)
{
    Button* button = new Button(text);
    // 外观名就是主题里的完整路径（变体用点拼在后面）：widget/button.text 等
    button->setAppearanceName("widget/button." + variant);
    return button;
}

// 图标字形：**从字体实际有的码位表里挑**（写死码位容易挑到占位字形，画出方块）
std::string glyph(size_t index)
{
    const std::vector<unsigned int>& codepoints = getIconFontCodepoints();
    if (codepoints.empty())
        return {};

    return icon::utf8Glyph(codepoints[index % codepoints.size()]);
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

    // 布局容器：无边框、无背景、有默认内边距 —— 里面再放一层组件容器
    Rectangle* layout = makeLayoutBox();
    layout->setAxis(Axis::ROW);
    layout->addView(makeText("layout", getTheme().number("metric.font.size.sm", 15.0f),
        getTheme().color("color.text.dim")));

    Rectangle* component = makeComponentBox();
    component->addView(makeText(getStr("base_widgets/component_inner"),
        getTheme().number("metric.font.size.sm", 15.0f), getTheme().color("color.text.primary")));
    layout->addView(component);
    containerCard->addView(layout);

    page->addView(containerCard);

    // ---------------------------------------------------------------- 按钮变体
    Rectangle* buttonCard = makeCard();
    buttonCard->addView(makeText(getStr("base_widgets/buttons"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    Box* row1 = makeRow(getTheme().number("metric.spacing.md", 12.0f));

    // 普通按钮：组件容器 + 居中文本
    row1->addView(makeVariantButton("text", getStr("base_widgets/btn_text")));

    // 图标按钮：只有图标
    Button* iconOnly = makeVariantButton("icon", "");
    iconOnly->setIcon(glyph(0));
    row1->addView(iconOnly);

    // 图标文字按钮：图标 + 文字（文字占剩余宽度）
    Button* iconText = makeVariantButton("icon_text", getStr("base_widgets/btn_icon_text"));
    iconText->setIcon(glyph(1));
    row1->addView(iconText);

    buttonCard->addView(row1);

    Box* row2 = makeRow(getTheme().number("metric.spacing.md", 12.0f));

    // 文字开关按钮：信息文字在开/关之间切换，颜色不同（外观里两个状态）
    Button* switchText = makeVariantButton("switch_text", getStr("base_widgets/btn_switch_text"));
    switchText->setIcon(glyph(2));
    switchText->setSwitched(false);
    switchText->setOnClick([switchText] {
        switchText->setSwitched(!switchText->isSwitched());
        switchText->setInfoText(switchText->isSwitched() ? getStr("base_widgets/on")
                                                        : getStr("base_widgets/off"));
    });
    switchText->setInfoText(getStr("base_widgets/off"));
    row2->addView(switchText);

    // 图案开关按钮：轨道 + 滑块，切换有过渡动画（外观 on/off 状态里是补间）
    Button* switchIcon = makeVariantButton("switch_icon", getStr("base_widgets/btn_switch_icon"));
    switchIcon->setIcon(glyph(3));
    switchIcon->setSwitched(true);
    switchIcon->setOnClick([switchIcon] { switchIcon->setSwitched(!switchIcon->isSwitched()); });
    row2->addView(switchIcon);

    // 信息按钮：信息文字由程序决定
    Button* info = makeVariantButton("info", getStr("base_widgets/btn_info"));
    info->setIcon(glyph(4));
    info->setInfoText(getStr("base_widgets/info_value"));
    info->setOnClick([info] {
        static int clicks = 0;
        clicks++;
        info->setInfoText(std::to_string(clicks));
    });
    row2->addView(info);

    buttonCard->addView(row2);

    Box* row3 = makeRow(getTheme().number("metric.spacing.md", 12.0f));

    // 下拉框按钮：点触发器弹遮罩列表（复用 Dropdown 的弹层），外观换成组合变体
    Dropdown* dropdown = new Dropdown();
    dropdown->setOptions({ getStr("base_widgets/option_a"), getStr("base_widgets/option_b"),
        getStr("base_widgets/option_c") }, 0);
    // 触发器换成组合外观：文字层是标题、信息层是当前值（DropdownTrigger 是 AppearanceView，都能设）
    AppearanceView* trigger = dropdown->getTrigger();
    trigger->setAppearanceName("widget/button.dropdown");
    trigger->setIcon(glyph(5));
    trigger->setText(getStr("base_widgets/btn_dropdown"));
    trigger->setInfoText(dropdown->getSelectedText());
    dropdown->setOnChange([dropdown, trigger](size_t) {
        trigger->setInfoText(dropdown->getSelectedText());
    });
    row3->addView(dropdown);

    buttonCard->addView(row3);

    // 选择器（数值模式）：进入设置态后焦点钉住，LR/方向键调值，长按连发
    SelectorButton* number = new SelectorButton(getStr("base_widgets/btn_selector_number"));
    number->setIcon(glyph(6));
    number->setNumberRange(0.0f, 100.0f, 5.0f, 1, 40.0f);
    number->setOnValueChanged([](float value) {
        BKLV_LOG_INFO("选择器（数值）：%.1f", (double)value);
    });
    Box* row4 = makeRow(getTheme().number("metric.spacing.md", 12.0f));

    // 选择器（列表模式）：LR 切换选项，越界环绕
    SelectorButton* list = new SelectorButton(getStr("base_widgets/btn_selector_list"));
    list->setIcon(glyph(7));
    list->setOptions({ getStr("base_widgets/option_a"), getStr("base_widgets/option_b"),
        getStr("base_widgets/option_c") });
    list->setArrowStyle(SelectorArrow::LR);
    list->setOnSelectionChanged([](int index, const std::string& text) {
        BKLV_LOG_INFO("选择器（列表）：" + std::to_string(index) + " " + text);
    });
    row4->addView(number);
    buttonCard->addView(row4);

    Box* row5 = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    row5->addView(list);
    buttonCard->addView(row5);
    buttonCard->addView(makeHint(getStr("base_widgets/selector_hint")));

    page->addView(buttonCard);

    return page;
}

} // namespace demo

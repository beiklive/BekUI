#include "app/pages.hpp"

#include <string>
#include <vector>

#include "bklv/bklv.hpp"
#include "bklv/core/icon_font.hpp"

#include "app/ui.hpp"

using namespace bklv;

namespace demo
{

// 基础控件总览页：**一个控件一节**。
//
//   每一节的构成：header（控件名）→ 控件本体 → 它的各状态（一行一个状态，左边写状态名）
//
// 几点约定（都是踩过之后定下来的）：
//   1. 状态是**外观层**的东西，同一个控件不能既是 focused 又是 disabled，
//      所以每个状态用**一个独立实例**，直接 setAppearanceState() 摆出来 —— 不是真去点它。
//      状态名照主题 JSON 里实际写的 state（base/button.json 是 focused / pressed / disabled）。
//   2. 选择器、下拉这类**没有多状态可看**的只给一行本体；开关的 on/off 就是它的状态，给两行。
//   3. 一行一个控件（不并排挤在一起），控件 grow=1 铺满。

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

// 小节 = 卡片 + header（控件名）。一个基础控件一节。
Rectangle* makeSection(const char* titleKey)
{
    Rectangle* card = makeCard();
    card->addView(makeText(getStr(titleKey), getTheme().number("metric.font.size.md", 18.0f),
        getTheme().color("color.text.primary")));
    return card;
}

// 状态行：左边是状态名（固定宽度，保证右边的控件纵向对齐），右边是"那个状态下的控件"
Box* makeStateRow(const char* stateKey, View* item)
{
    Box* row = makeRow(getTheme().number("metric.spacing.md", 12.0f));

    // 单行 + 固定宽度：状态名不许换行（换行了每行高度会不一样，右边控件就对不齐）
    Label* name = makeText(getStr(stateKey), getTheme().number("metric.font.size.sm", 15.0f),
        getTheme().color("color.text.dim"), true);
    name->setWidth(104.0f);
    row->addView(name);

    item->setGrow(1.0f);
    row->addView(item);
    return row;
}

// 按钮的一个状态：state 传空串 = 默认态
Button* makeStateButton(
    const char* variant, const std::string& text, const char* state, const char* iconName)
{
    Button* button = makeVariantButton(variant, text);
    if (iconName != nullptr)
        button->setIcon(glyph(iconName));
    if (state != nullptr && state[0] != 0)
        button->setAppearanceState(state, true);
    return button;
}

// 按钮一族：本体（默认）+ 各状态。状态名照主题 JSON 里的 state 写。
void addButtonStates(
    Rectangle* section, const char* variant, const std::string& text, const char* iconName)
{
    struct StateSpec
    {
        const char* key;
        const char* state;
    };
    static const StateSpec kStates[] = {
        { "base_widgets/state_default", "" },
        { "base_widgets/state_focused", "focused" },
        { "base_widgets/state_pressed", "pressed" },
        { "base_widgets/state_disabled", "disabled" },
    };

    for (const StateSpec& spec : kStates)
        section->addView(
            makeStateRow(spec.key, makeStateButton(variant, text, spec.state, iconName)));
}

// 开关一族：开 / 关两行（on / off 本身就是它的状态）
void addSwitchStates(
    Rectangle* section, const char* variant, const std::string& text, const char* iconName)
{
    Button* on = makeStateButton(variant, text, "", iconName);
    on->setSwitched(true);
    on->setInfoText(getStr("base_widgets/on"));
    section->addView(makeStateRow("base_widgets/state_on", on));

    Button* off = makeStateButton(variant, text, "", iconName);
    off->setSwitched(false);
    off->setInfoText(getStr("base_widgets/off"));
    section->addView(makeStateRow("base_widgets/state_off", off));
}

// 文字：正文大小 + 主色
Label* bodyText(const std::string& text)
{
    return makeText(text, getTheme().number("metric.font.size.sm", 15.0f),
        getTheme().color("color.text.primary"));
}

} // namespace

Box* buildBaseWidgetsPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("base_widgets/hint")));

    // ---------------------------------------------------------------- 容器

    // 基本盒：唯一"有底有框"的基础容器
    {
        Rectangle* section = makeSection("base_widgets/sec_box");
        Rectangle* box    = new Rectangle();
        box->setAppearanceName("base/box");
        box->addView(bodyText(getStr("base_widgets/box_inner")));
        section->addView(makeItemRow(box));
        section->addView(makeHint(getStr("base_widgets/box_hint")));
        page->addView(section);
    }

    // 布局容器：只负责摆位置（横排 + 居中），自己不带底
    {
        Rectangle* section = makeSection("base_widgets/sec_layout");
        Rectangle* layout  = makeLayoutBox();
        layout->setAxis(Axis::ROW);
        layout->setAlignItems(AlignItems::CENTER);
        layout->addView(makeText("layout", getTheme().number("metric.font.size.sm", 15.0f),
            getTheme().color("color.text.dim")));
        Rectangle* inner = makeComponentBox();
        inner->addView(bodyText(getStr("base_widgets/component_inner")));
        layout->addView(inner);
        section->addView(makeItemRow(layout));
        section->addView(makeHint(getStr("base_widgets/layout_hint")));
        page->addView(section);
    }

    // 组件容器：它 import 了 base/box，改 base 这里一起变
    {
        Rectangle* section = makeSection("base_widgets/sec_component");
        Rectangle* box     = makeComponentBox();
        box->addView(bodyText(getStr("base_widgets/component_inner")));
        section->addView(makeItemRow(box));
        section->addView(makeHint(getStr("base_widgets/component_hint")));
        page->addView(section);
    }

    // ---------------------------------------------------------------- 基础元素

    // 文本：普通 / 次级 / 单行截断
    {
        Rectangle* section = makeSection("base_widgets/sec_label");
        section->addView(makeItemRow(bodyText(getStr("base_widgets/label_body"))));
        section->addView(makeItemRow(makeHint(getStr("base_widgets/label_dim"))));
        Label* truncated = makeText(getStr("base_widgets/label_long"),
            getTheme().number("metric.font.size.sm", 15.0f),
            getTheme().color("color.text.primary"), true);
        section->addView(makeStateRow("base_widgets/state_single_line", truncated));
        section->addView(makeHint(getStr("base_widgets/label_hint")));
        page->addView(section);
    }

    // 图片：解码后上传成纹理，尺寸由布局决定
    {
        Rectangle* section = makeSection("base_widgets/sec_image");
        Image* image       = new Image();
        image->setImageFromFileAsync("resources/testcard.png");
        image->setWidth(120.0f);
        image->setHeight(120.0f);
        Box* row = makeRow(getTheme().number("metric.spacing.md", 12.0f));
        row->addView(image);
        row->addView(makeHint(getStr("base_widgets/image_hint")));
        section->addView(row);
        page->addView(section);
    }

    // 滑块：进度决定几何（语义绘制），颜色/圆角/粗细来自 JSON
    {
        Rectangle* section = makeSection("base_widgets/sec_slider");
        Label* value       = makeHint(getStr("base_widgets/slider_value", 40));
        Slider* slider     = new Slider();
        slider->setProgress(0.4f);
        slider->setOnChange([value](float progress) {
            value->setText(getStr("base_widgets/slider_value", (int)(progress * 100.0f)));
        });
        section->addView(makeStateRow("base_widgets/state_default", slider));
        section->addView(value);
        section->addView(makeHint(getStr("base_widgets/slider_hint")));
        page->addView(section);
    }

    // 进度圈：ImDrawList 没有旋转 API，自己在 C++ 里拼四边形
    {
        Rectangle* section = makeSection("base_widgets/sec_spinner");
        Box* row           = makeRow(getTheme().number("metric.spacing.lg", 16.0f));
        row->addView(new ProgressSpinner());
        ProgressSpinner* slow = new ProgressSpinner();
        slow->setDuration(2.4f);
        slow->setThickness(4.0f);
        row->addView(slow);
        row->addView(makeHint(getStr("base_widgets/spinner_hint")));
        section->addView(row);
        page->addView(section);
    }

    // ---------------------------------------------------------------- 按钮（带状态矩阵）

    // 纯文字：最普通的按钮，四种状态都摆出来
    {
        Rectangle* section = makeSection("base_widgets/sec_btn_text");
        addButtonStates(section, "text", getStr("base_widgets/btn_text"), nullptr);
        section->addView(makeHint(getStr("base_widgets/states_hint")));
        page->addView(section);
    }

    // 纯图标
    {
        Rectangle* section = makeSection("base_widgets/sec_btn_icon");
        addButtonStates(section, "icon", "", "star");
        page->addView(section);
    }

    // 图标 + 文字
    {
        Rectangle* section = makeSection("base_widgets/sec_btn_icon_text");
        addButtonStates(section, "icon_text", getStr("base_widgets/btn_icon_text"), "save");
        page->addView(section);
    }

    // 信息按钮：右侧信息文字由程序决定
    {
        Rectangle* section = makeSection("base_widgets/sec_btn_info");
        addButtonStates(section, "info", getStr("base_widgets/btn_info"), "home");
        Button* live = makeVariantButton("info", getStr("base_widgets/btn_info"));
        live->setIcon(glyph("home"));
        live->setInfoText(getStr("base_widgets/info_value"));
        live->setOnClick([live] {
            static int clicks = 0;
            clicks++;
            live->setInfoText(std::to_string(clicks));
        });
        section->addView(makeStateRow("base_widgets/state_clickable", live));
        section->addView(makeHint(getStr("base_widgets/info_hint")));
        page->addView(section);
    }

    // 文字开关：开 / 关两个状态（信息文字跟着变）
    {
        Rectangle* section = makeSection("base_widgets/sec_switch_text");
        addSwitchStates(section, "switch_text", getStr("base_widgets/btn_switch_text"), "info");
        Button* live = makeVariantButton("switch_text", getStr("base_widgets/btn_switch_text"));
        live->setIcon(glyph("info"));
        live->setSwitched(false);
        live->setInfoText(getStr("base_widgets/off"));
        live->setOnClick([live] {
            live->setSwitched(!live->isSwitched());
            live->setInfoText(live->isSwitched() ? getStr("base_widgets/on")
                                                 : getStr("base_widgets/off"));
        });
        section->addView(makeStateRow("base_widgets/state_clickable", live));
        page->addView(section);
    }

    // 图案开关：轨道 + 滑块，切换有过渡动画（补间写在 on/off 状态里）
    {
        Rectangle* section = makeSection("base_widgets/sec_switch_icon");
        addSwitchStates(section, "switch_icon", getStr("base_widgets/btn_switch_icon"), "settings");
        Button* live = makeVariantButton("switch_icon", getStr("base_widgets/btn_switch_icon"));
        live->setIcon(glyph("settings"));
        live->setSwitched(true);
        live->setOnClick([live] { live->setSwitched(!live->isSwitched()); });
        section->addView(makeStateRow("base_widgets/state_clickable", live));
        section->addView(makeHint(getStr("base_widgets/switch_hint")));
        page->addView(section);
    }

    // ---------------------------------------------------------------- 没有多状态的：只给本体

    // 下拉触发器：点它弹遮罩列表（弹层复用 Dropdown）
    {
        Rectangle* section = makeSection("base_widgets/sec_dropdown");
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
        section->addView(makeItemRow(dropdown));
        section->addView(makeHint(getStr("base_widgets/dropdown_hint")));
        page->addView(section);
    }

    // 选择器（数值模式）：进入设置态后焦点钉住，LR / 方向键调值，长按连发
    {
        Rectangle* section = makeSection("base_widgets/sec_selector_number");
        SelectorButton* number = new SelectorButton(getStr("base_widgets/btn_selector_number"));
        number->setIcon(glyph("palette"));
        number->setNumberRange(0.0f, 100.0f, 5.0f, 1, 40.0f);
        number->setOnValueChanged(
            [](float value) { BKLV_LOG_INFO("选择器（数值）：%.1f", (double)value); });
        section->addView(makeItemRow(number));
        section->addView(makeHint(getStr("base_widgets/selector_hint")));
        page->addView(section);
    }

    // 选择器（列表模式）：LR 切换选项，越界环绕
    {
        Rectangle* section = makeSection("base_widgets/sec_selector_list");
        SelectorButton* list = new SelectorButton(getStr("base_widgets/btn_selector_list"));
        list->setIcon(glyph("list"));
        list->setOptions({ getStr("base_widgets/option_a"), getStr("base_widgets/option_b"),
            getStr("base_widgets/option_c") });
        list->setArrowStyle(SelectorArrow::LR);
        list->setOnSelectionChanged([](int index, const std::string& text) {
            BKLV_LOG_INFO("选择器（列表）：" + std::to_string(index) + " " + text);
        });
        section->addView(makeItemRow(list));
        page->addView(section);
    }

    return page;
}

} // namespace demo

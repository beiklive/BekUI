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
    Box* row = makeRow(getTheme().number("metric.spacing.lg", 20.0f));
    item->setGrow(1.0f);
    row->addView(item);
    return row;
}

// 小节 = header（控件名 + **改哪个 JSON**）+ 若干行/状态矩阵。一个基础控件一节。
//
// ⚠️ header 必须是**参与布局的子节点**，不能当容器的层用：层不进流、不占高度，
// 会直接压在第一个子节点上（这一页以前就是拿 section 当"带层的容器"，标题画在小节正中间、
// 叠着下面的行 —— 用户报的"布局混乱"有一半是它）。所以这里套一层 column：
// 上面是 base/section（只有标题那一条那么高），下面才是内容。
//
// 右对齐那个 info 层以前一直空着，长说明只好各占一整行 —— 那是"又长又乱"的另一半原因。
// 现在把"这个控件由哪份 JSON 决定"放进 info（**文件路径不做 i18n**：它是技术标识符，
// 和代码里的 appearanceName 一样），长说明只留真正讲行为的那几条。
Box* makeSection(const char* titleKey, const char* themeFile)
{
    AppearanceView* header = new AppearanceView("base/section", false);
    header->setText(getStr(titleKey));
    if (themeFile != nullptr)
        header->setInfoText(themeFile);

    Box* section = makeColumn(getTheme().number("metric.spacing.md", 14.0f));
    section->setWidthPercentage(100.0f);
    section->addView(header);
    return section;
}

// 状态矩阵的一格：上面一行小字写状态名，下面是被摆成那个状态的控件。
//
// 为什么把多个状态并列（**这条是本页对"一行一个控件"约定的显式豁免**）：
// 状态矩阵是标本表 —— 同一个控件的不同状态并排才看得出差别，5 行拆开看反而要来回对比。
// 豁免的前提是"不会被挤到截断"：一格至少 280px（三列铺满 950px 的内容宽），
// 而信息文字只有图标按钮那类才有。
Box* makeStateCell(const char* labelKey, View* item, bool stretch = true)
{
    Box* cell = makeColumn(4.0f);
    cell->setGrow(1.0f);
    cell->addView(makeText(getStr(labelKey), getTheme().number("metric.font.size.xs", 12.0f),
        getTheme().color("color.text.dim")));
    if (stretch)
        item->setGrow(1.0f);
    cell->addView(item);
    return cell;
}

// 一行放若干个状态格（3 个左右，视内容宽）
Box* makeStateGrid(const char* const* labels, View* const* items, int count, bool stretch = true)
{
    Box* row = makeRow(getTheme().number("metric.spacing.lg", 16.0f), AlignItems::STRETCH);
    for (int i = 0; i < count; i++)
        row->addView(makeStateCell(labels[i], items[i], stretch));
    return row;
}

// 状态行：左边是状态名（固定宽度，保证右边的控件纵向对齐），右边是"那个状态下的控件"。
// stretch=false 用于固定尺寸的控件（正方形 / 圆形的图标按钮）—— 拉伸会把圆形拉成椭圆
Box* makeStateRow(const char* stateKey, View* item, bool stretch = true)
{
    Box* row = makeRow(getTheme().number("metric.spacing.lg", 20.0f));

    // 单行 + 固定宽度：状态名不许换行（换行了每行高度会不一样，右边控件就对不齐）
    Label* name = makeText(getStr(stateKey), getTheme().number("metric.font.size.sm", 15.0f),
        getTheme().color("color.text.dim"), true);
    name->setWidth(104.0f);
    row->addView(name);

    if (stretch)
        item->setGrow(1.0f);
    row->addView(item);
    return row;
}

// 按钮的一个状态：state 传空串 = 默认态
Button* makeStateButton(const char* variant, const std::string& text, const char* state,
    const char* iconName, bool glass = false)
{
    Button* button = makeVariantButton(variant, text);
    if (iconName != nullptr)
        button->setIcon(glyph(iconName));
    if (state != nullptr && state[0] != 0)
        button->setAppearanceState(state, true);
    if (glass)
        button->setAppearanceVariant("glass");   // 主题里的 <外观>.glass：底换成液态玻璃
    return button;
}

// 按钮一族：本体（默认）+ 各状态。状态名照主题 JSON 里的 state 写。
void addButtonStates(Box* section, const char* variant, const std::string& text,
    const char* iconName, bool stretch = true)
{
    // 本体 + 三个状态一行；禁用 + 玻璃版本一行（见 makeStateCell 上关于豁免的说明）
    static const char* const kRowOne[3] = { "base_widgets/state_default",
        "base_widgets/state_focused", "base_widgets/state_pressed" };
    static const char* const kRowTwo[2] = { "base_widgets/state_disabled",
        "base_widgets/state_glass" };

    View* rowOne[3] = { makeStateButton(variant, text, "", iconName),
        makeStateButton(variant, text, "focused", iconName),
        makeStateButton(variant, text, "pressed", iconName) };
    section->addView(makeStateGrid(kRowOne, rowOne, 3, stretch));

    View* rowTwo[2] = { makeStateButton(variant, text, "disabled", iconName),
        makeStateButton(variant, text, "", iconName, /*glass=*/true) };
    section->addView(makeStateGrid(kRowTwo, rowTwo, 2, stretch));
}

// 开关一族：开 / 关两行（on / off 本身就是它的状态）
void addSwitchStates(
    Box* section, const char* variant, const std::string& text, const char* iconName)
{
    static const char* const kLabels[3] = { "base_widgets/state_on", "base_widgets/state_off",
        "base_widgets/state_glass" };

    Button* on = makeStateButton(variant, text, "", iconName);
    on->setSwitched(true);
    on->setInfoText(getStr("base_widgets/on"));

    Button* off = makeStateButton(variant, text, "", iconName);
    off->setSwitched(false);
    off->setInfoText(getStr("base_widgets/off"));

    Button* glass = makeStateButton(variant, text, "", iconName, true);
    glass->setSwitched(true);
    glass->setInfoText(getStr("base_widgets/on"));

    View* items[3] = { on, off, glass };
    section->addView(makeStateGrid(kLabels, items, 3));
}

// 玻璃板：给"没有自己的面"的控件（文字/图片/滑块/进度）用的液态玻璃版本 ——
// 控件放在一块玻璃上，读作"这个控件在液态玻璃里"。
Rectangle* makeGlassPlate(View* content)
{
    Rectangle* plate = new Rectangle();
    plate->setAppearanceName("base/glass_plate");
    content->setGrow(1.0f);
    plate->addView(content);
    return plate;
}

// 玻璃板一行（状态名 + 板子）
Box* makeGlassPlateRow(View* content)
{
    return makeStateRow("base_widgets/state_glass", makeGlassPlate(content));
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
    // 小节之间用比节内行距更大的间距：否则 17 个节会连成一整块（用户报过"过于紧凑"）
    Box* page = makeColumn(getTheme().number("metric.spacing.xl", 24.0f));
    page->addView(makeHint(getStr("base_widgets/hint")));

    // ---------------------------------------------------------------- 容器

    // 基本盒：唯一"有底有框"的基础容器
    {
        Box* section = makeSection("base_widgets/sec_box", "base/box.json");
        Rectangle* box    = new Rectangle();
        box->setAppearanceName("base/box");
        box->addView(bodyText(getStr("base_widgets/box_inner")));
        section->addView(makeItemRow(box));

        Rectangle* glassBox = new Rectangle();
        glassBox->setAppearanceName("base/box");
        glassBox->addView(bodyText(getStr("base_widgets/box_inner")));
        section->addView(makeGlassPlateRow(glassBox));

        // 渐变底：同一个 rect 层，把 fill 换成 gradient{angle, stops}。
        // 这份外观**内联在页面里**，不进主题 —— 默认主题是纯扁平风格，渐变属于"能力演示"
        //（和 stripe / quad 同一做法：能力示例不占随机发布的主题资源）
        Rectangle* gradientBox = new Rectangle();
        gradientBox->setAppearanceName("base/box"); // 兜底：内联那份编译失败也不至于空白
        std::string gradientError;
        if (!gradientBox->applyAppearanceJson(R"({
  "box": {
    "width": "100%",
    "height": "auto",
    "padding": "@theme/metric.card.padding",
    "gap": "@theme/metric.spacing.sm"
  },
  "layers": [
    {
      "id": "bg",
      "fill": "#00000000",
      "radius": "@theme/metric.radius.lg",
      "gradient": {
        "type": "linear",
        "angle": 90,
        "stops": [
          { "at": 0.0, "color": "#2E4A9EFF" },
          { "at": 1.0, "color": "#7B3FB0FF" }
        ]
      }
    }
  ]
})", gradientError))
        {
            BKLV_LOG_WARNING("渐变示例外观编译失败：%s", gradientError.c_str());
        }
        gradientBox->addView(bodyText(getStr("base_widgets/box_gradient")));
        section->addView(makeItemRow(gradientBox));
        page->addView(section);
    }

    // 布局容器：只负责摆位置（横排 + 居中），自己不带底
    {
        Box* section = makeSection("base_widgets/sec_layout", "base/box.layout.json");
        Rectangle* layout  = makeLayoutBox();
        layout->setAxis(Axis::ROW);
        layout->setAlignItems(AlignItems::CENTER);
        layout->addView(makeText("layout", getTheme().number("metric.font.size.sm", 15.0f),
            getTheme().color("color.text.dim")));
        Rectangle* inner = makeComponentBox();
        inner->addView(bodyText(getStr("base_widgets/component_inner")));
        layout->addView(inner);
        section->addView(makeItemRow(layout));

        Rectangle* glassInner = makeComponentBox();
        glassInner->addView(bodyText(getStr("base_widgets/component_inner")));
        section->addView(makeGlassPlateRow(glassInner));
        page->addView(section);
    }

    // 组件容器：它 import 了 base/box，改 base 这里一起变
    {
        Box* section = makeSection("base_widgets/sec_component", "base/box.component.json");
        Rectangle* box     = makeComponentBox();
        box->addView(bodyText(getStr("base_widgets/component_inner")));
        section->addView(makeItemRow(box));

        Rectangle* glassBox = makeComponentBox();
        glassBox->addView(bodyText(getStr("base_widgets/component_inner")));
        section->addView(makeGlassPlateRow(glassBox));
        page->addView(section);
    }

    // ---------------------------------------------------------------- 基础元素

    // 文本：普通 / 次级 / 单行截断
    {
        Box* section = makeSection("base_widgets/sec_label", "views/label.cpp");
        section->addView(makeItemRow(bodyText(getStr("base_widgets/label_body"))));
        section->addView(makeItemRow(makeHint(getStr("base_widgets/label_dim"))));
        Label* truncated = makeText(getStr("base_widgets/label_long"),
            getTheme().number("metric.font.size.sm", 15.0f),
            getTheme().color("color.text.primary"), true);
        section->addView(makeStateRow("base_widgets/state_single_line", truncated));
        section->addView(makeGlassPlateRow(bodyText(getStr("base_widgets/label_body"))));
        section->addView(makeHint(getStr("base_widgets/label_hint")));
        page->addView(section);
    }

    // 图片：解码后上传成纹理，尺寸由布局决定
    {
        Box* section = makeSection("base_widgets/sec_image", "views/image.cpp");
        Image* image       = new Image();
        image->setImageFromFileAsync("resources/testcard.png");
        image->setWidth(120.0f);
        image->setHeight(120.0f);
        Box* row = makeRow(getTheme().number("metric.spacing.lg", 20.0f));
        row->addView(image);
        row->addView(makeHint(getStr("base_widgets/image_hint")));
        section->addView(row);

        Image* glassImage = new Image();
        glassImage->setImageFromFileAsync("resources/testcard.png");
        glassImage->setWidth(120.0f);
        glassImage->setHeight(120.0f);
        section->addView(makeGlassPlateRow(glassImage));
        page->addView(section);
    }

    // 滑块：进度决定几何（语义绘制），颜色/圆角/粗细来自 JSON
    {
        Box* section = makeSection("base_widgets/sec_slider", "base/slider.json");
        Label* value       = makeHint(getStr("base_widgets/slider_value", 40));
        Slider* slider     = new Slider();
        slider->setProgress(0.4f);
        slider->setOnChange([value](float progress) {
            value->setText(getStr("base_widgets/slider_value", (int)(progress * 100.0f)));
        });
        section->addView(makeStateRow("base_widgets/state_default", slider));

        Slider* glassSlider = new Slider();
        glassSlider->setProgress(0.4f);
        section->addView(makeGlassPlateRow(glassSlider));
        section->addView(value);
        section->addView(makeHint(getStr("base_widgets/slider_hint")));
        page->addView(section);
    }

    // 进度圈：ImDrawList 没有旋转 API，自己在 C++ 里拼四边形
    {
        Box* section = makeSection("base_widgets/sec_spinner", "views/progress_spinner.cpp");
        Box* row           = makeRow(getTheme().number("metric.spacing.lg", 16.0f));
        row->addView(new ProgressSpinner());
        ProgressSpinner* slow = new ProgressSpinner();
        slow->setDuration(2.4f);
        slow->setThickness(4.0f);
        row->addView(slow);
        row->addView(makeHint(getStr("base_widgets/spinner_hint")));
        section->addView(row);

        Box* glassRow = makeRow(getTheme().number("metric.spacing.lg", 16.0f));
        glassRow->addView(new ProgressSpinner());
        section->addView(makeGlassPlateRow(glassRow));
        page->addView(section);
    }

    // ---------------------------------------------------------------- 按钮（带状态矩阵）

    // 纯文字：最普通的按钮，四种状态都摆出来
    {
        Box* section = makeSection("base_widgets/sec_btn_text", "widget/button.text.json");
        addButtonStates(section, "text", getStr("base_widgets/btn_text"), nullptr);
        section->addView(makeHint(getStr("base_widgets/states_hint")));
        page->addView(section);
    }

    // 纯图标：两款 —— 正方形（icon）与圆形（icon_circle），图标都完全居中
    {
        Box* section = makeSection("base_widgets/sec_btn_icon", "widget/button.icon.json");
        section->addView(makeHint(getStr("base_widgets/shape_square")));
        addButtonStates(section, "icon", "", "star", false);
        section->addView(makeHint(getStr("base_widgets/shape_circle")));
        addButtonStates(section, "icon_circle", "", "settings", false);
        section->addView(makeHint(getStr("base_widgets/icon_hint")));
        page->addView(section);
    }

    // 图标 + 文字
    {
        Box* section = makeSection("base_widgets/sec_btn_icon_text", "widget/button.icon_text.json");
        addButtonStates(section, "icon_text", getStr("base_widgets/btn_icon_text"), "save");
        page->addView(section);
    }

    // 列表按钮：普通按钮的"列表元素"款 —— 没有边框，文字正常左对齐
    {
        Box* section = makeSection("base_widgets/sec_btn_list", "widget/button.list.json");
        addButtonStates(section, "list", getStr("base_widgets/btn_list"), nullptr);
        section->addView(makeHint(getStr("base_widgets/list_hint")));
        page->addView(section);
    }

    // 信息按钮：右侧信息文字由程序决定
    {
        Box* section = makeSection("base_widgets/sec_btn_info", "widget/button.info.json");
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
        Box* section = makeSection("base_widgets/sec_switch_text", "widget/button.switch_text.json");
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
        Box* section = makeSection("base_widgets/sec_switch_icon", "widget/button.switch_icon.json");
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
        Box* section = makeSection("base_widgets/sec_dropdown", "widget/button.dropdown.json");
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

        Dropdown* glassDropdown = new Dropdown();
        glassDropdown->setOptions({ getStr("base_widgets/option_a"),
            getStr("base_widgets/option_b"), getStr("base_widgets/option_c") }, 0);
        AppearanceView* glassTrigger = glassDropdown->getTrigger();
        glassTrigger->setAppearanceName("widget/button.dropdown");
        glassTrigger->setAppearanceVariant("glass");
        glassTrigger->setIcon(glyph("list"));
        glassTrigger->setText(getStr("base_widgets/btn_dropdown"));
        glassTrigger->setInfoText(glassDropdown->getSelectedText());
        section->addView(makeStateRow("base_widgets/state_glass", glassDropdown));
        section->addView(makeHint(getStr("base_widgets/dropdown_hint")));
        page->addView(section);
    }

    // 选择器（数值模式）：进入设置态后焦点钉住，LR / 方向键调值，长按连发
    {
        Box* section = makeSection("base_widgets/sec_selector_number", "widget/button.selector.json");
        SelectorButton* number = new SelectorButton(getStr("base_widgets/btn_selector_number"));
        number->setIcon(glyph("palette"));
        number->setNumberRange(0.0f, 100.0f, 5.0f, 1, 40.0f);
        number->setOnValueChanged(
            [](float value) { BKLV_LOG_INFO("选择器（数值）：%.1f", (double)value); });
        section->addView(makeItemRow(number));

        SelectorButton* glassNumber = new SelectorButton(getStr("base_widgets/btn_selector_number"));
        glassNumber->setIcon(glyph("palette"));
        glassNumber->setNumberRange(0.0f, 100.0f, 5.0f, 1, 40.0f);
        glassNumber->setAppearanceVariant("glass");
        section->addView(makeStateRow("base_widgets/state_glass", glassNumber));
        section->addView(makeHint(getStr("base_widgets/selector_hint")));
        page->addView(section);
    }

    // 选择器（列表模式）：LR 切换选项，越界环绕
    {
        Box* section = makeSection("base_widgets/sec_selector_list", "widget/button.selector.json");
        SelectorButton* list = new SelectorButton(getStr("base_widgets/btn_selector_list"));
        list->setIcon(glyph("list"));
        list->setOptions({ getStr("base_widgets/option_a"), getStr("base_widgets/option_b"),
            getStr("base_widgets/option_c") });
        list->setArrowStyle(SelectorArrow::LR);
        list->setOnSelectionChanged([](int index, const std::string& text) {
            BKLV_LOG_INFO("选择器（列表）：" + std::to_string(index) + " " + text);
        });
        section->addView(makeItemRow(list));

        SelectorButton* glassList = new SelectorButton(getStr("base_widgets/btn_selector_list"));
        glassList->setIcon(glyph("list"));
        glassList->setOptions({ getStr("base_widgets/option_a"), getStr("base_widgets/option_b"),
            getStr("base_widgets/option_c") });
        glassList->setArrowStyle(SelectorArrow::LR);
        glassList->setAppearanceVariant("glass");
        section->addView(makeStateRow("base_widgets/state_glass", glassList));
        page->addView(section);
    }

    return page;
}

} // namespace demo

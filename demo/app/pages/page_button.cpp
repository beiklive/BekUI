// 按钮展示页（骨架）。
//
// 现在只有**一个基础按钮**（外观 `base/button/button`），后面由你来加内容。
// 想继续加就在 page 上 addView —— 不需要动外层结构。

#include "app/pages.hpp"

#include <string>
#include "bklv/bklv.hpp"
#include "bklv/core/icon_font.hpp"

using namespace bklv;

namespace demo
{

Box* buildButtonShowcasePage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setShrink(0.0f);
    page->setGap(getTheme().number("metric.spacing.md", 12.0f));
    // 按钮保持自然宽度（列方向默认 stretch 会把它拉满整行）
    page->setAlignItems(AlignItems::FLEX_START);
    
    // 0) 普通按钮
    page->addView(bklv::makeSectionHeader("基础按钮"));
    Box* button_box = new Box(Axis::ROW);
    button_box->setWidthPercentage(100.0f); // 关键：行自身要有确定宽度，grow 才有"剩余空间"可分
    button_box->setGap(getTheme().number("metric.spacing.md", 12.0f));


    Button* button = new Button();
    button->setAppearanceName("base/button/button");
    button->setText("基础按钮");
    button_box->addView(button);

    Button* border_button = new Button();
    border_button->setAppearanceName("base/button/button.bordered");
    border_button->setText("边框按钮");
    button_box->addView(border_button);

    Button* tab_button = new Button();
    tab_button->setAppearanceName("base/button/tab_button");
    tab_button->setText("列表按钮");
    button_box->addView(tab_button);

    page->addView(button_box);
    for (View* child : button_box->getChildren())
    {
        child->setGrow(1.0f);
        child->setFlexBasis(0.0f);  // 从 0 开始分（否则按文字宽度打底）
        child->setShrink(0.0f);
        child->setMinWidth(0.0f); // 外观里的 minWidth（120）会卡住 1 份的宽度
    }

    // 0) 图标按钮
    page->addView(bklv::makeSectionHeader("图标按钮"));

    Box* icon_row = new Box(Axis::ROW);
    icon_row->setWidthPercentage(100.0f);
    icon_row->setGap(getTheme().number("metric.spacing.md", 12.0f));
    icon_row->setAlignItems(AlignItems::CENTER);

    // 1) 纯图标正方形
    Button* square = new Button();
    square->setAppearanceName("base/button/button.icon");
    square->setIcon(bklv::icon::glyphByName("star"));
    icon_row->addView(square);

    // 2) 纯图标圆形
    Button* circle = new Button();
    circle->setAppearanceName("base/button/button.icon_circle");
    circle->setIcon(bklv::icon::glyphByName("favorite"));
    icon_row->addView(circle);

    // 3) 图标 + 文字：点一下换图标和文字，验证接口生效
    Button* icon_text = new Button();
    icon_text->setAppearanceName("base/button/button.icon_text");
    icon_text->setIcon(bklv::icon::glyphByName("settings"));
    icon_text->setText("图标 + 文字");
    icon_text->setWidth(220.0f);
    icon_text->setOnClick([icon_text] {
        static bool alt = false;
        alt = !alt;
        icon_text->setIcon(bklv::icon::glyphByName(alt ? "check" : "settings"));
        icon_text->setText(alt ? "已切换" : "图标 + 文字");
    });
    icon_row->addView(icon_text);

    // 0) 信息按钮
    //LR按钮
    
    








    page->addView(icon_row);
    Box* row1 = new Box(Axis::COLUMN);
    row1->setWidthPercentage(100.0f);
    row1->setGap(10.f);
    // ---- 表单按钮（Toggle / Custom / Option / Value）----
    page->addView(bklv::makeSectionHeader("表单按钮"));

    // 第一行：Toggle + Custom
    ToggleButton* toggle = new ToggleButton("无线网络", bklv::icon::glyphByName("settings"));
    toggle->setWidth(300.0f);
    toggle->setChecked(true);
    toggle->setOnToggle([](bool on) { demo::toastManager()->showInfo(on ? "无线网络：开" : "无线网络：关"); });
    row1->addView(toggle);

    ToggleButton* text_on = new ToggleButton("文字开关", bklv::icon::glyphByName("settings"),
        ToggleStyle::TEXT);
    text_on->setWidth(300.0f);
    text_on->setChecked(true);
    row1->addView(text_on);

    CustomButton* custom = new CustomButton("存储路径");
    custom->setIcon(bklv::icon::glyphByName("settings"));
    custom->setRightText("/sd/roms");
    custom->setWidth(300.0f);
    row1->addView(custom);

    // 第二行：Option + Value
    OptionButton* option = new OptionButton("缩放");
    option->setIcon(bklv::icon::glyphByName("settings"));
    option->setOptions({"原始", "拉伸", "整数缩放"}, 0);
    option->setWidth(360.0f);
    option->setOnSelectionChanged([](size_t i) {
        const char* names[] = {"原始", "拉伸", "整数缩放"};
        demo::toastManager()->showInfo(std::string("画面缩放：") + names[i]);
    });
    row1->addView(option);

    ValueButton* value = new ValueButton("音量");
    value->setIcon(bklv::icon::glyphByName("settings"));
    value->setup(50.0f, 0.0f, 100.0f, 1.0f, 0);
    value->setWidth(300.0f);
    value->setOnValueChanged([](float v) { demo::toastManager()->showInfo("音量：" + std::to_string((int)v)); });
    row1->addView(value);
    for (View* child : row1->getChildren())
    {
        child->setWidthPercentage(100.0f); // 外观里的 minWidth（120）会卡住 1 份的宽度
    }
    page->addView(row1);

    // ---- 选择器 / 下拉 ----
    page->addView(bklv::makeSectionHeader("选择器 / 下拉"));

    Box* selector_row = new Box(Axis::ROW);
    selector_row->setWidthPercentage(100.0f);
    selector_row->setGap(getTheme().number("metric.spacing.md", 12.0f));
    selector_row->setAlignItems(AlignItems::CENTER);

    SelectorButton* volume = new SelectorButton("音量");
    volume->setIcon(bklv::icon::glyphByName("settings"));
    volume->setNumberRange(0.0f, 100.0f, 1.0f, 0, 50.0f);
    volume->setWidth(360.0f);
    volume->setOnValueChanged([](float v) {
        demo::toastManager()->showInfo("音量：" + std::to_string((int)v));
    });
    selector_row->addView(volume);

    SelectorButton* scale_mode = new SelectorButton("缩放模式");
    scale_mode->setIcon(bklv::icon::glyphByName("settings"));
    scale_mode->setMode(SelectorMode::LIST);
    scale_mode->setOptions({"原始", "拉伸", "整数缩放"});
    scale_mode->setSelectedIndex(1);
    scale_mode->setArrowStyle(SelectorArrow::LR);
    scale_mode->setWidth(360.0f);
    scale_mode->setOnSelectionChanged([](int, const std::string& text) {
        demo::toastManager()->showInfo("缩放：" + text);
    });
    selector_row->addView(scale_mode);
    page->addView(selector_row);

    Box* dropdown_row = new Box(Axis::ROW);
    dropdown_row->setWidthPercentage(100.0f);
    dropdown_row->setGap(getTheme().number("metric.spacing.md", 12.0f));
    dropdown_row->setAlignItems(AlignItems::CENTER);

    Dropdown* theme_dropdown = new Dropdown();
    theme_dropdown->setTitle("主题");
    theme_dropdown->setOptions({"默认", "深色", "浅色"}, 0);
    theme_dropdown->setWidth(360.0f);
    theme_dropdown->setOnChange([](size_t index) {
        demo::toastManager()->showInfo("主题切换到第 " + std::to_string((int)index + 1) + " 项");
    });
    dropdown_row->addView(theme_dropdown);

    Dropdown* quality = new Dropdown();
    quality->setTitle("画质");
    quality->setOptions({"自动", "性能", "画质", "自定义", "省电", "极致"});
    quality->setWidth(360.0f);
    quality->setOnChange([](size_t index) {
        demo::toastManager()->showInfo("画质选项：" + std::to_string((int)index));
    });
    dropdown_row->addView(quality);
    page->addView(dropdown_row);

    // ---- Toast ----
    page->addView(bklv::makeSectionHeader("Toast"));
    Box* toast_row = new Box(Axis::ROW);
    toast_row->setWidthPercentage(100.0f);
    toast_row->setGap(getTheme().number("metric.spacing.md", 12.0f));
    toast_row->setAlignItems(AlignItems::CENTER);

    Button* ok = new Button("成功提示");
    ok->setOnClick([] { demo::toastManager()->showSuccess("存档已保存"); });
    toast_row->addView(ok);

    Button* bad = new Button("错误提示");
    bad->setOnClick([] { demo::toastManager()->showError("无法读取 SD 卡"); });
    toast_row->addView(bad);

    Button* info = new Button("信息提示");
    info->setOnClick([] { demo::toastManager()->showInfo("已切换到 2 倍速"); });
    toast_row->addView(info);

    page->addView(toast_row);

    // 页面打开时弹一条，方便截图/验收（只影响 demo）
    if (demo::toastManager() != nullptr)
        demo::toastManager()->showInfo("按钮展示页已打开");

    return page;
}

} // namespace demo

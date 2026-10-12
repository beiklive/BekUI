#include "app/pages.hpp"

#include <vector>

#include "bklv/bklv.hpp"

#include "app/ui.hpp"
#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

Box* buildWidgetsPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("widgets/hint")));

    // ---- 按钮 ----
    Rectangle* buttonCard = makeCard();
    buttonCard->addView(makeText(getStr("widgets/buttons"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    // borealis 的 5 种按钮 style 全覆盖：primary / highlight / default / bordered / borderless
    // （+ 我们自己的 ghost，作为"更淡的一种"）
    Box* buttonRow = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    buttonRow->addView(new Button(getStr("widgets/button_default")));
    buttonRow->addView(new Button(getStr("widgets/button_primary"), "primary"));
    buttonRow->addView(new Button(getStr("widgets/button_highlight"), "highlight"));
    buttonCard->addView(buttonRow);

    Box* buttonRow2 = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    buttonRow2->addView(new Button(getStr("widgets/button_bordered"), "bordered"));
    buttonRow2->addView(new Button(getStr("widgets/button_borderless"), "borderless"));
    buttonRow2->addView(new Button(getStr("widgets/ghost"), "ghost"));
    Button* disabled = new Button(getStr("widgets/disabled"));
    disabled->setEnabled(false);
    buttonRow2->addView(disabled);
    buttonCard->addView(buttonRow2);
    page->addView(buttonCard);

    // ---- 滑块 ----
    Rectangle* sliderCard = makeCard();
    sliderCard->addView(makeText(getStr("widgets/slider"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    Label* volume = makeHint(getStr("widgets/volume", 40));
    Slider* slider = new Slider();
    slider->setProgress(0.4f);
    slider->setOnChange([volume](float progress) {
        volume->setText(getStr("widgets/volume", (int)(progress * 100.0f)));
    });
    sliderCard->addView(slider);
    sliderCard->addView(volume);
    page->addView(sliderCard);

    // ---- 下拉 ----
    Rectangle* dropdownCard = makeCard();
    dropdownCard->addView(makeText(getStr("widgets/dropdown"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    Label* picked = makeHint(getStr("theme/dropdown_picked", getStr("theme/option_1")));
    Dropdown* dropdown = new Dropdown();
    dropdown->setTitle(getStr("theme/dropdown_label"));
    dropdown->setOptions({ getStr("theme/option_1"), getStr("theme/option_2"),
        getStr("theme/option_3"), getStr("theme/option_4") });
    dropdown->setOnChange([picked, dropdown](size_t) {
        picked->setText(getStr("theme/dropdown_picked", dropdown->getSelectedText()));
    });
    dropdownCard->addView(dropdown);
    dropdownCard->addView(picked);
    page->addView(dropdownCard);

    // ---- 进度与加载 ----
    Rectangle* progressCard = makeCard();
    progressCard->addView(makeText(getStr("widgets/progress"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    Box* progressRow = makeRow(getTheme().number("metric.spacing.lg", 16.0f));
    progressRow->addView(new ProgressSpinner());

    Box* bars = makeColumn(getTheme().number("metric.spacing.sm", 8.0f));
    struct BarSpec
    {
        const char* key;
        float progress;
        const char* color;
    };
    const BarSpec specs[] = {
        { "widgets/download", 0.72f, "color.card.blue" },
        { "widgets/cache", 0.45f, "color.card.green" },
        { "widgets/usage", 0.88f, "color.card.orange" },
    };
    for (const BarSpec& spec : specs)
    {
        Label* caption = makeHint(getStr(spec.key, (int)(spec.progress * 100.0f)));
        bars->addView(caption);

        // 进度条：轨道 + 填充（两层 Rectangle，外观与颜色都来自主题）
        Rectangle* track = new Rectangle();
        track->setHeight(12.0f);
        track->setBackgroundColor(getTheme().color("color.surface.sunken"));

        Box* trackRow = makeRow(0.0f, AlignItems::STRETCH);
        trackRow->setHeight(12.0f);

        Rectangle* fill = new Rectangle();
        fill->setWidthPercentage(spec.progress * 100.0f);
        fill->setHeight(12.0f);
        fill->setBackgroundColor(getTheme().color(spec.color));
        trackRow->addView(fill);

        // 轨道本身是容器，填充放进去
        track->addView(trackRow);
        track->setHeight(AUTO);
        bars->addView(track);
    }
    progressRow->addView(bars);
    progressCard->addView(progressRow);
    page->addView(progressCard);

    // ---- 横向滚动（对标 borealis components 的 HScroll）----
    Rectangle* hscrollCard = makeCard();
    hscrollCard->addView(makeText(getStr("widgets/hscroll"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    HScrollingFrame* hscroll = new HScrollingFrame();
    hscroll->setHeight(150.0f);
    Box* hscrollRow = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    for (int i = 0; i < 8; i++)
    {
        Box* item = new Box(Axis::COLUMN);
        item->setAlignItems(AlignItems::CENTER);
        item->setGap(6.0f);

        Image* thumb = new Image();
        thumb->setDimensions(120.0f, 68.0f);
        thumb->setCornerRadius(getTheme().number("metric.radius.sm", 4.0f));
        thumb->setImageFromFileAsync("resources/testcard.png");
        item->addView(thumb);

        item->addView(makeHint(getStr("widgets/hscroll_item", i + 1)));
        hscrollRow->addView(item);
    }
    hscroll->setContentView(hscrollRow);
    hscrollCard->addView(hscroll);
    page->addView(hscrollCard);

    // ---- 标签对齐（对标 borealis components 的 labels：left / center / right）----
    Rectangle* alignCard = makeCard();
    alignCard->addView(makeText(getStr("widgets/label_align"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    Box* alignRow = makeRow(0.0f);
    alignRow->setJustifyContent(JustifyContent::SPACE_EVENLY);
    alignRow->setAlignItems(AlignItems::CENTER);
    const float alignWidth = getTheme().number("metric.label.alignDemoWidth", 200.0f);
    for (const auto& item : std::initializer_list<std::pair<const char*, HorizontalAlign>>{
             { "widgets/label_left", HorizontalAlign::LEFT },
             { "widgets/label_center", HorizontalAlign::CENTER },
             { "widgets/label_right", HorizontalAlign::RIGHT } })
    {
        Label* label = makeText(getStr(item.first), getTheme().number("metric.font.size.sm", 14.0f),
            getTheme().color("color.text.primary"), true);
        label->setWidthPercentage(30.0f);
        label->setMinWidth(alignWidth * 0.4f);
        label->setHorizontalAlign(item.second);
        label->setBackgroundColor(getTheme().color("color.surface.hover"));
        alignRow->addView(label);
    }
    alignCard->addView(alignRow);
    page->addView(alignCard);

    // ---- 图片 ----
    Rectangle* imageCard = makeCard();
    imageCard->addView(makeText(getStr("widgets/image"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));
    // 异步加载：文件 IO + 解码在后台线程，纹理上传回主线程（大图不会卡帧）
    Image* image = new Image();
    image->setDimensions(160.0f, 90.0f);
    image->setImageFromFileAsync("resources/testcard.png");
    imageCard->addView(image);

    Button* reload = new Button(getStr("widgets/image_reload"));
    reload->setOnClick([image] { image->setImageFromFileAsync("resources/testcard.png"); });
    imageCard->addView(reload);

    // 5 种尺寸 / 缩放组合（对标 borealis components 的 images 一节）：
    // 缩小 / 原尺寸 / 放大 / 拉伸变形 / 居中裁切
    Box* imageRow = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    imageRow->setAlignItems(AlignItems::CENTER);
    imageRow->setJustifyContent(JustifyContent::SPACE_EVENLY);

    struct ImageCase
    {
        const char* caption;
        float width;
        float height;
        ImageScalingType scaling;
    };
    const ImageCase cases[] = {
        { "widgets/image_downscaled", 76.0f, 76.0f, ImageScalingType::FILL },
        { "widgets/image_original", AUTO, AUTO, ImageScalingType::FIT },
        { "widgets/image_upscaled", 170.0f, 170.0f, ImageScalingType::FILL },
        { "widgets/image_stretched", 300.0f, 75.0f, ImageScalingType::STRETCH },
        { "widgets/image_centered", 140.0f, 75.0f, ImageScalingType::CENTER },
    };

    for (const ImageCase& item : cases)
    {
        Box* column = new Box(Axis::COLUMN);
        column->setAlignItems(AlignItems::CENTER);
        column->setGap(4.0f);

        Image* sample = new Image();
        sample->setWidth(item.width);
        sample->setHeight(item.height);
        sample->setScalingType(item.scaling);
        sample->setCornerRadius(getTheme().number("metric.radius.sm", 4.0f));
        sample->setImageFromFileAsync("resources/testcard.png");
        column->addView(sample);

        column->addView(makeHint(getStr(item.caption)));
        imageRow->addView(column);
    }
    imageCard->addView(imageRow);
    page->addView(imageCard);

    return page;
}

} // namespace demo

#include "app/pages.hpp"

#include <vector>

#include "app/ui.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/progress_spinner.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/dropdown.hpp"
#include "bklv/widgets/slider.hpp"

using namespace bklv;

namespace demo
{

Box* buildWidgetsPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeTitle(getStr("tab/widgets")));
    page->addView(makeHint(getStr("widgets/hint")));

    // ---- 按钮 ----
    Rectangle* buttonCard = makeCard();
    buttonCard->addView(makeText(getStr("widgets/buttons"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    Box* buttonRow = makeRow(getTheme().number("metric.spacing.md", 12.0f));
    buttonRow->addView(new Button(getStr("widgets/ok")));
    buttonRow->addView(new Button(getStr("widgets/ghost"), "ghost"));
    Button* disabled = new Button(getStr("widgets/disabled"));
    disabled->setEnabled(false);
    buttonRow->addView(disabled);
    buttonCard->addView(buttonRow);
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

    // ---- 图片 ----
    Rectangle* imageCard = makeCard();
    imageCard->addView(makeText(getStr("widgets/image"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));
    Image* image = new Image();
    image->setDimensions(160.0f, 90.0f);
    image->setImageFromFile("resources/testcard.png");
    imageCard->addView(image);
    page->addView(imageCard);

    return page;
}

} // namespace demo

// 滑块演示：基础 Slider 与 demo 衍生外观（import base/slider/slider）。

#include "app/pages.hpp"
#include "app/pages/showcase.hpp"

using namespace bklv;

namespace demo
{

Box* buildSliderPage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setGap(getTheme().number("metric.spacing.lg", 24.0f));

    {
        Box* stage = new Box(Axis::COLUMN);
        stage->setWidth(460.0f);
        stage->setGap(12.0f);

        Slider* slider = new Slider();
        slider->setProgress(0.35f);
        slider->setStep(0.02f);
        slider->setOnChange([](float value) {
            toastManager()->showInfo("进度：" + std::to_string((int)(value * 100.0f)) + "%");
        });
        stage->addView(slider);

        page->addView(makeShowcaseSection("Slider · 基础外观", stage,
            readThemeJson("base/slider/slider.json"),
            "左右键/点按调整，值域为 0~1；轨道、已填充段、把手和几何都在外观 JSON 里。"));
    }

    {
        Box* stage = new Box(Axis::COLUMN);
        stage->setWidth(460.0f);
        stage->setGap(12.0f);

        Slider* slider = new Slider();
        slider->setAppearanceName("demo/slider_accent");
        slider->setProgress(0.7f);
        stage->addView(slider);

        page->addView(makeShowcaseSection("Slider · 衍生外观（demo/slider_accent）", stage,
            readThemeJson("demo/slider_accent.json"),
            "分类规则：基础滑块在 base/slider/slider；demo 衍生外观放 demo/，"
            "import 基础滑块后只覆盖 fill 颜色。"));
    }

    return page;
}

} // namespace demo

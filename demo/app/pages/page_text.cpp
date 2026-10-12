#include "app/pages.hpp"

#include "app/ui.hpp"

#include <string>
#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

Box* buildTextPage()
{
    const ThemeData& theme = getTheme();

    Box* page = makeColumn(theme.number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("text/hint")));

    // ---- 字号档位（全部来自主题，换肤/缩放会跟着变）----
    Rectangle* sizeCard = makeCard();
    sizeCard->addView(makeText(getStr("text/sizes"), theme.number("metric.font.size.md", 18.0f),
        theme.color("color.text.primary")));

    for (const char* key : { "metric.font.size.sm", "metric.font.size.md", "metric.font.size.lg",
             "metric.font.size.title" })
    {
        Label* sample = new Label();
        sample->setText(getStr("text/sample", key));
        sample->setFontSize(theme.number(key, 18.0f));
        sample->setTextColorExplicit(false);
        sizeCard->addView(sample);
    }
    page->addView(sizeCard);

    // ---- 单行截断 ----
    Rectangle* truncateCard = makeCard();
    truncateCard->addView(makeText(getStr("text/truncate"), theme.number("metric.font.size.md", 18.0f),
        theme.color("color.text.primary")));
    truncateCard->addView(makeHint(getStr("text/truncate_desc")));

    Label* longSingle = new Label();
    longSingle->setText(getStr("text/long"));
    longSingle->setSingleLine(true);
    longSingle->setFontSizeExplicit(false);
    longSingle->setTextColorExplicit(false);
    longSingle->setWidth(280.0f);
    truncateCard->addView(longSingle);
    page->addView(truncateCard);

    // ---- 多行换行 ----
    Rectangle* wrapCard = makeCard();
    wrapCard->addView(makeText(getStr("text/wrap"), theme.number("metric.font.size.md", 18.0f),
        theme.color("color.text.primary")));
    wrapCard->addView(makeHint(getStr("text/wrap_desc")));

    Label* wrapped = new Label();
    wrapped->setText(getStr("text/long"));
    wrapped->setFontSizeExplicit(false);
    wrapped->setTextColorExplicit(false);
    wrapped->setLineHeight(theme.number("metric.font.lineHeight", 1.4f));
    wrapped->setWidth(360.0f);
    wrapCard->addView(wrapped);
    page->addView(wrapCard);

    // ---- 颜色档位 ----
    Rectangle* colorCard = makeCard();
    colorCard->addView(makeText(getStr("text/colors"), theme.number("metric.font.size.md", 18.0f),
        theme.color("color.text.primary")));
    for (const char* key : { "color.text.primary", "color.text.dim", "color.text.disabled",
             "color.accent.base" })
    {
        Label* sample = makeText(key, theme.number("metric.font.size.md", 18.0f),
            theme.color(key));
        colorCard->addView(sample);
    }
    page->addView(colorCard);

    // ---- 交互式调节（对标 borealis 的 TextTestTab）----
    // 宽度 / 高度 / 垂直对齐 / 水平对齐 / 单行，实时作用到下面这个标签上
    Rectangle* liveCard = makeCard();
    liveCard->addView(makeText(getStr("text/live"),
        theme.number("metric.font.size.md", 18.0f), theme.color("color.text.primary")));

    Label* target = new Label();
    target->setText(getStr("text/live_sample"));
    target->setFontSize(theme.number("metric.font.size.sm", 14.0f));
    target->setTextColorExplicit(false);
    target->setBackgroundColor(theme.color("color.surface.hover"));
    target->setWidth(theme.number("metric.label.liveWidth", 320.0f));
    target->setHeight(60.0f);
    liveCard->addView(target);

    auto refreshTarget = [target] {
        // 宽度/高度是 0 就回到 auto（borealis 的约定）
        target->setWidth(target->getWidth() == 0.0f ? AUTO : target->getWidth());
        target->invalidate();
    };

    SliderCell* widthCell = new SliderCell(getStr("text/live_width"));
    widthCell->setRange(0.0f, 1.0f);
    widthCell->setValue(0.8f);
    widthCell->setDetail("320");
    widthCell->setOnChange([target, widthCell, refreshTarget](float value) {
        const int pixels = (int)(value * 400.0f);
        target->setWidth(pixels <= 0 ? AUTO : (float)pixels);
        widthCell->setDetail(pixels <= 0 ? "auto" : std::to_string(pixels));
        refreshTarget();
    });
    liveCard->addView(widthCell);

    SliderCell* heightCell = new SliderCell(getStr("text/live_height"));
    heightCell->setRange(0.0f, 1.0f);
    heightCell->setValue(0.15f);
    heightCell->setDetail("60");
    heightCell->setOnChange([target, heightCell, refreshTarget](float value) {
        const int pixels = (int)(value * 400.0f);
        target->setHeight(pixels <= 0 ? AUTO : (float)pixels);
        heightCell->setDetail(pixels <= 0 ? "auto" : std::to_string(pixels));
        refreshTarget();
    });
    liveCard->addView(heightCell);

    SelectorCell* vAlign = new SelectorCell(getStr("text/live_valign"));
    vAlign->setOptions({ "top", "center", "bottom" });
    vAlign->setSelectedIndex(1);
    vAlign->setOnChange([target](int index, const std::string&) {
        target->setVerticalAlign(static_cast<VerticalAlign>(index));
    });
    liveCard->addView(vAlign);

    SelectorCell* hAlign = new SelectorCell(getStr("text/live_halign"));
    hAlign->setOptions({ "left", "center", "right" });
    hAlign->setSelectedIndex(0);
    hAlign->setOnChange([target](int index, const std::string&) {
        target->setHorizontalAlign(static_cast<HorizontalAlign>(index));
    });
    liveCard->addView(hAlign);

    BooleanCell* singleLine = new BooleanCell(getStr("text/live_singleline"), false);
    singleLine->setOnChange([target](bool value) { target->setSingleLine(value); });
    liveCard->addView(singleLine);
    page->addView(liveCard);

    return page;
}

} // namespace demo

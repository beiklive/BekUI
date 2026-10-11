#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/theme_data.hpp"

using namespace bklv;

namespace demo
{

Box* buildTextPage()
{
    const ThemeData& theme = getTheme();

    Box* page = makeColumn(theme.number("metric.spacing.md", 12.0f));
    page->addView(makeTitle(getStr("tab/text")));
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

    return page;
}

} // namespace demo

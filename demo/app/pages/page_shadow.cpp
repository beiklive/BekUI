// 阴影演示页：外观 JSON 的 shadow 层 + 全局质量开关（fast / smooth）。

#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

namespace
{

// 一张带阴影的卡片，展示某个变体（变体文件见 resources/theme/*/widgets/rectangle.card.*.json）
Rectangle* makeShadowCard(const std::string& variant, const std::string& title)
{
    Rectangle* card = new Rectangle("card");
    card->setAppearanceVariant(variant);
    card->addView(makeText(title, getTheme().number("metric.font.size.md", 18.0f),
        getTheme().color("color.text.primary")));
    card->addView(makeHint(getStr("shadow/variant_hint")));
    return card;
}

} // namespace

Box* buildShadowPage()
{
    Box* page = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("shadow/hint")));

    // ---- 质量开关 ----
    Rectangle* switchCard = makeCard();
    switchCard->addView(makeText(getStr("shadow/quality_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    BooleanCell* toggle = new BooleanCell(getStr("shadow/quality_cell"),
        shadow::getQuality() == ShadowQuality::SMOOTH);
    toggle->setStyle(BooleanCellStyle::SWITCH);
    Label* state = makeHint(getStr("shadow/quality_state", shadow::qualityName(shadow::getQuality())));

    toggle->setOnChange([state](bool on) {
        shadow::setQuality(on ? ShadowQuality::SMOOTH : ShadowQuality::FAST);
        state->setText(getStr("shadow/quality_state", shadow::qualityName(shadow::getQuality())));
    });
    switchCard->addView(toggle);
    switchCard->addView(state);
    switchCard->addView(makeHint(getStr("shadow/quality_hint")));
    page->addView(switchCard);

    // ---- 参数（每个变体只改 JSON 里的 feather / offset / radius / 颜色）----
    Rectangle* paramsCard = makeCard();
    paramsCard->addView(makeText(getStr("shadow/params_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));

    Box* row = makeRow(getTheme().number("metric.spacing.md", 12.0f), AlignItems::STRETCH);
    row->addView(makeShadowCard("", getStr("shadow/card_default")));
    row->addView(makeShadowCard("soft", getStr("shadow/card_soft")));
    row->addView(makeShadowCard("lifted", getStr("shadow/card_lifted")));
    paramsCard->addView(row);
    paramsCard->addView(makeHint(getStr("shadow/params_hint")));
    page->addView(paramsCard);

    // 半透明底色才看得出 cutout 的差别：不开时阴影会从透明底里透出来
    Rectangle* cutCard = makeCard();
    cutCard->addView(makeText(getStr("shadow/cutout_title"),
        getTheme().number("metric.font.size.md", 18.0f), getTheme().color("color.text.primary")));
    Box* cutRow = makeRow(getTheme().number("metric.spacing.md", 12.0f), AlignItems::STRETCH);
    cutRow->addView(makeShadowCard("glass", getStr("shadow/card_glass")));
    cutRow->addView(makeShadowCard("glasscut", getStr("shadow/card_glasscut")));
    cutCard->addView(cutRow);
    cutCard->addView(makeHint(getStr("shadow/cutout_hint")));
    page->addView(cutCard);

    return page;
}

} // namespace demo

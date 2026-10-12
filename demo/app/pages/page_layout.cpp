#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/rectangle.hpp"

using namespace bklv;

namespace demo
{

namespace
{

// 一个小色块，用来直观看出布局结果
Rectangle* block(float width, float height, const char* color)
{
    Rectangle* rect = new Rectangle();
    rect->setDimensions(width, height);
    rect->setBackgroundColor(getTheme().color(color));
    return rect;
}

} // namespace

Box* buildLayoutPage()
{
    const ThemeData& theme = getTheme();

    Box* page = makeColumn(theme.number("metric.spacing.md", 12.0f));
    page->addView(makeHint(getStr("layout/hint")));

    // ---- 主轴排列 ----
    Rectangle* mainCard = makeCard();
    mainCard->addView(makeText(getStr("layout/justify"), theme.number("metric.font.size.md", 18.0f),
        theme.color("color.text.primary")));

    for (const char* mode : { "FLEX_START", "CENTER", "FLEX_END", "SPACE_BETWEEN" })
    {
        Box* row = makeRow(10.0f);
        row->setJustifyContent(mode == std::string("CENTER") ? JustifyContent::CENTER
                : mode == std::string("FLEX_END")                    ? JustifyContent::FLEX_END
                : mode == std::string("SPACE_BETWEEN")               ? JustifyContent::SPACE_BETWEEN
                                                                     : JustifyContent::FLEX_START);
        row->setHeight(38.0f);
        row->addView(block(48.0f, 24.0f, "color.card.blue"));
        row->addView(block(48.0f, 24.0f, "color.card.green"));
        row->addView(block(48.0f, 24.0f, "color.card.orange"));
        mainCard->addView(row);
    }
    page->addView(mainCard);

    // ---- 交叉轴对齐 ----
    Rectangle* crossCard = makeCard();
    crossCard->addView(makeText(getStr("layout/align"), theme.number("metric.font.size.md", 18.0f),
        theme.color("color.text.primary")));

    const AlignItems aligns[] = { AlignItems::FLEX_START, AlignItems::CENTER, AlignItems::FLEX_END,
        AlignItems::STRETCH };
    for (AlignItems align : aligns)
    {
        Box* row = makeRow(10.0f, align);
        row->setHeight(56.0f);
        row->addView(block(48.0f, 20.0f, "color.card.blue"));
        row->addView(block(48.0f, 34.0f, "color.card.green"));
        row->addView(block(48.0f, 26.0f, "color.card.purple"));
        crossCard->addView(row);
    }
    page->addView(crossCard);

    // ---- grow 分配剩余空间 ----
    Rectangle* growCard = makeCard();
    growCard->addView(makeText(getStr("layout/grow"), theme.number("metric.font.size.md", 18.0f),
        theme.color("color.text.primary")));
    growCard->addView(makeHint(getStr("layout/grow_desc")));

    Box* growRow = makeRow(10.0f, AlignItems::CENTER);
    growRow->setHeight(40.0f);
    Rectangle* fixed = block(80.0f, 24.0f, "color.card.blue");
    growRow->addView(fixed);

    Rectangle* grown = new Rectangle();
    grown->setHeight(24.0f);
    grown->setGrow(1.0f);
    grown->setBackgroundColor(theme.color("color.accent.base"));
    growRow->addView(grown);
    growCard->addView(growRow);
    page->addView(growCard);

    // ---- 百分比尺寸 ----
    Rectangle* percentCard = makeCard();
    percentCard->addView(makeText(getStr("layout/percent"), theme.number("metric.font.size.md", 18.0f),
        theme.color("color.text.primary")));

    Box* percentRow = makeRow(10.0f, AlignItems::CENTER);
    percentRow->setHeight(40.0f);
    for (float ratio : { 25.0f, 35.0f, 40.0f })
    {
        Rectangle* part = new Rectangle();
        part->setWidthPercentage(ratio);
        part->setHeight(20.0f);
        part->setBackgroundColor(ratio < 30.0f   ? theme.color("color.card.blue")
                : ratio < 38.0f                  ? theme.color("color.card.green")
                                                 : theme.color("color.card.orange"));
        percentRow->addView(part);
    }
    percentCard->addView(percentRow);
    page->addView(percentCard);

    return page;
}

} // namespace demo

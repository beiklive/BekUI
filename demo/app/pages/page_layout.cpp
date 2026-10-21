// 布局演示：左侧用纯色 rect 摆出 Yoga 的主轴/交叉轴/伸缩行为，
// 右侧滚动显示与这段布局等价的 box JSON（说明 JSON 能表达同样的东西）。

#include "app/pages.hpp"
#include "app/pages/showcase.hpp"

using namespace bklv;

namespace
{

Box* makeBar(JustifyContent justify)
{
    Box* row = new Box(Axis::ROW);
    row->setWidth(360.0f);
    row->setHeight(54.0f);
    row->setPadding(8.0f);
    row->setGap(6.0f);
    row->setJustifyContent(justify);
    row->setAlignItems(AlignItems::CENTER);
    row->setBackgroundColor(getTheme().color("color.surface.sunken"));
    row->setCornerRadius(8.0f);
    row->addView(demo::makeColorRect(getTheme().color("color.accent.base"), 40.0f, 32.0f));
    row->addView(demo::makeColorRect(getTheme().color("color.common.green"), 40.0f, 32.0f));
    row->addView(demo::makeColorRect(getTheme().color("color.common.orange"), 40.0f, 32.0f));
    return row;
}

Box* makeAlignRow(AlignItems align)
{
    Box* row = new Box(Axis::ROW);
    row->setWidth(360.0f);
    row->setHeight(64.0f);
    row->setPadding(8.0f);
    row->setGap(6.0f);
    row->setAlignItems(align);
    row->setBackgroundColor(getTheme().color("color.surface.sunken"));
    row->setCornerRadius(8.0f);
    row->addView(demo::makeColorRect(getTheme().color("color.common.blue"), 40.0f, 18.0f));
    row->addView(demo::makeColorRect(getTheme().color("color.common.purple"), 40.0f, 34.0f));
    row->addView(demo::makeColorRect(getTheme().color("color.common.pink"), 40.0f, 48.0f));
    return row;
}

Box* makeGrowDemo()
{
    Box* row = new Box(Axis::ROW);
    row->setWidth(360.0f);
    row->setHeight(64.0f);
    row->setPadding(8.0f);
    row->setGap(8.0f);
    row->setBackgroundColor(getTheme().color("color.surface.sunken"));
    row->setCornerRadius(8.0f);

    const float colors[3] = { 1.0f, 2.0f, 1.0f };
    const ImU32 palette[3] = { getTheme().color("color.common.teal"),
        getTheme().color("color.accent.base"), getTheme().color("color.common.green") };
    for (int i = 0; i < 3; i++)
    {
        Box* cell = new Box();
        cell->setHeightPercentage(100.0f);
        cell->setGrow(colors[i]);
        cell->setFlexBasis(0.0f);
        cell->setBackgroundColor(palette[i]);
        cell->setCornerRadius(6.0f);
        row->addView(cell);
    }
    return row;
}

const char* JUSTIFY_JSON = R"({
  "box": {
    "axis": "row",
    "justifyContent": "flexStart | center | spaceBetween",
    "alignItems": "center",
    "gap": 6,
    "width": 360,
    "height": 54
  }
})";

const char* ALIGN_JSON = R"({
  "box": {
    "axis": "row",
    "alignItems": "center | flexEnd",
    "gap": 6,
    "width": 360,
    "height": 64
  }
})";

const char* GROW_JSON = R"({
  "box": { "axis": "row", "gap": 8, "padding": 8 },
  "children": [
    { "grow": 1, "flexBasis": 0 },
    { "grow": 2, "flexBasis": 0 },
    { "grow": 1, "flexBasis": 0 }
  ]
})";

} // namespace

namespace demo
{

Box* buildLayoutShowcasePage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setGap(getTheme().number("metric.spacing.lg", 24.0f));

    Box* justify = new Box(Axis::COLUMN);
    justify->setGap(10.0f);
    justify->addView(makeBar(JustifyContent::FLEX_START));
    justify->addView(makeBar(JustifyContent::CENTER));
    justify->addView(makeBar(JustifyContent::SPACE_BETWEEN));
    page->addView(makeShowcaseSection("布局 · 主轴分布（justifyContent）", justify, JUSTIFY_JSON,
        "同一个 row，只改 justifyContent：flexStart 靠左、center 居中、spaceBetween 两端对齐。"));

    Box* align = new Box(Axis::COLUMN);
    align->setGap(10.0f);
    align->addView(makeAlignRow(AlignItems::CENTER));
    align->addView(makeAlignRow(AlignItems::FLEX_END));
    page->addView(makeShowcaseSection("布局 · 交叉轴对齐（alignItems）", align, ALIGN_JSON,
        "不同高度的色块按 align 摆放：center 垂直居中，flexEnd 贴底。"));

    page->addView(makeShowcaseSection("布局 · 弹性比例（grow + flexBasis）", makeGrowDemo(), GROW_JSON,
        "grow 1 : 2 : 1 且 flexBasis 为 0：宽度按比例分剩余空间，而不是先占内容宽。"));

    return page;
}

} // namespace demo

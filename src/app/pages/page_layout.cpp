#include "app/pages/pages.hpp"

#include <string>

#include "app/page_helpers.hpp"
#include "app/widgets.hpp"
#include "bklv/core/input.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/progress_spinner.hpp"
#include "bklv/views/recycler.hpp"
#include "bklv/views/scrolling_frame.hpp"

using namespace bklv;

namespace demo
{

Box* buildLayoutPage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);

    page->addView(makeTitle("Flexbox 布局"));
    page->addView(makeHint("全部由 Yoga 计算：主轴方向、分布方式、交叉轴对齐、gap、padding、"
                           "grow/shrink 与最小最大尺寸。改窗口大小会实时重排。"));

    // 主轴分布
    Box* justifyCard = makeCard();
    justifyCard->addView(makeText("justifyContent", 16.0f, rgb(200, 208, 220)));

    struct JustifySpec
    {
        const char* name;
        JustifyContent value;
    };
    const JustifySpec justifies[] = {
        { "FLEX_START", JustifyContent::FLEX_START },
        { "CENTER", JustifyContent::CENTER },
        { "SPACE_BETWEEN", JustifyContent::SPACE_BETWEEN },
    };

    for (const JustifySpec& spec : justifies)
    {
        Box* line = new Box(Axis::COLUMN);
        line->setWidthPercentage(100.0f);
        line->setMarginTop(10.0f);
        line->addView(makeText(spec.name, 13.0f, rgb(160, 170, 185)));

        Box* row = makeRow(0.0f);
        row->setHeight(44.0f);
        row->setMarginTop(5.0f);
        row->setBackgroundColor(rgb(30, 33, 40));
        row->setCornerRadius(6.0f);
        row->setPadding(6.0f);
        row->setJustifyContent(spec.value);

        const ImU32 colors[] = { IM_COL32(57, 96, 145, 255), IM_COL32(70, 125, 100, 255),
            IM_COL32(145, 94, 67, 255) };
        for (int i = 0; i < 3; ++i)
        {
            Box* block = new Box(Axis::COLUMN);
            block->setDimensions(46.0f, 32.0f);
            block->setBackgroundColor(colors[i]);
            block->setCornerRadius(5.0f);
            row->addView(block);
        }
        line->addView(row);
        justifyCard->addView(line);
    }
    page->addView(justifyCard);

    // 交叉轴对齐
    Box* alignCard = makeCard();
    alignCard->addView(makeText("alignItems", 16.0f, rgb(200, 208, 220)));

    struct AlignSpec
    {
        const char* name;
        AlignItems value;
    };
    const AlignSpec alignSpecs[] = {
        { "FLEX_START", AlignItems::FLEX_START },
        { "CENTER", AlignItems::CENTER },
        { "FLEX_END", AlignItems::FLEX_END },
    };

    for (const AlignSpec& spec : alignSpecs)
    {
        Box* line = new Box(Axis::COLUMN);
        line->setWidthPercentage(100.0f);
        line->setMarginTop(10.0f);
        line->addView(makeText(spec.name, 13.0f, rgb(160, 170, 185)));

        Box* row = makeRow(10.0f, spec.value);
        row->setHeight(70.0f);
        row->setMarginTop(5.0f);
        row->setBackgroundColor(rgb(30, 33, 40));
        row->setCornerRadius(6.0f);
        row->setPadding(6.0f);

        const float heights[] = { 56.0f, 30.0f, 44.0f };
        const ImU32 colors[]  = { IM_COL32(57, 96, 145, 255), IM_COL32(70, 125, 100, 255),
            IM_COL32(145, 94, 67, 255) };
        for (int i = 0; i < 3; ++i)
        {
            Box* block = new Box(Axis::COLUMN);
            block->setDimensions(46.0f, heights[i]);
            block->setBackgroundColor(colors[i]);
            block->setCornerRadius(5.0f);
            row->addView(block);
        }
        line->addView(row);
        alignCard->addView(line);
    }
    page->addView(alignCard);

    // grow
    Box* growCard = makeCard();
    growCard->addView(makeText("grow 撑满剩余空间", 16.0f, rgb(200, 208, 220)));

    Box* growRow = makeRow(10.0f);
    growRow->setHeight(80.0f);
    growRow->setMarginTop(10.0f);

    Box* side = new Box(Axis::COLUMN);
    side->setDimensions(110.0f, 80.0f);
    side->setShrink(0.0f);
    side->setBackgroundColor(rgb(57, 96, 145));
    side->setCornerRadius(8.0f);
    Label* sideText = makeText("固定 110", 13.0f, rgb(235, 238, 242), true);
    sideText->setHorizontalAlign(HorizontalAlign::CENTER);
    side->addView(sideText);
    growRow->addView(side);

    Box* grow1 = new Box(Axis::COLUMN);
    grow1->setHeight(80.0f);
    grow1->setGrow(1.0f);
    grow1->setBackgroundColor(rgb(70, 125, 100));
    grow1->setCornerRadius(8.0f);
    Label* growText = makeText("grow = 1", 13.0f, rgb(235, 238, 242), true);
    growText->setHorizontalAlign(HorizontalAlign::CENTER);
    grow1->addView(growText);
    growRow->addView(grow1);

    Box* grow2 = new Box(Axis::COLUMN);
    grow2->setHeight(80.0f);
    grow2->setGrow(2.0f);
    grow2->setBackgroundColor(rgb(145, 94, 67));
    grow2->setCornerRadius(8.0f);
    Label* grow2Text = makeText("grow = 2", 13.0f, rgb(235, 238, 242), true);
    grow2Text->setHorizontalAlign(HorizontalAlign::CENTER);
    grow2->addView(grow2Text);
    growRow->addView(grow2);

    growCard->addView(growRow);
    page->addView(growCard);

    return page;
}

} // namespace demo

#include "app/pages/pages.hpp"

#include <string>

#include "app/page_helpers.hpp"
#include "app/pages/list_items.hpp"
#include "app/widgets.hpp"
#include "bklv/core/input.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/progress_spinner.hpp"
#include "bklv/views/recycler.hpp"
#include "bklv/views/scrolling_frame.hpp"

using namespace bklv;

namespace demo
{

DebugPage buildDebugPage()
{
    DebugPage result;
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);

    page->addView(makeTitle("布局与调试"));
    page->addView(makeHint("这一页用来说明调试能力。按 F1 打开调试层，右上角会显示实时布局树、"
                           "当前焦点视图、帧率、图形后端与存活纹理数。"));

    Box* infoCard = makeCard();
    infoCard->addView(makeText("当前运行信息", 16.0f, rgb(200, 208, 220)));

    Label* stats = makeText("", 14.0f, rgb(180, 190, 205));
    stats->setLineHeight(1.6f);
    stats->setMarginTop(8.0f);
    infoCard->addView(stats);
    page->addView(infoCard);

    Box* tipsCard = makeCard();
    tipsCard->addView(makeText("调试技巧", 16.0f, rgb(200, 208, 220)));

    const char* tips[] = {
        "F1 开关调试层：高亮当前焦点路径，列出布局树每个节点的尺寸与坐标",
        "树里带 > 前缀的那一行就是当前焦点",
        "滚动 / 焦点不对时，先看调试层里那一项的 w/h 是不是 0",
        "recycler 的 cell 数量稳定在视口行数附近，说明回收生效；一直涨就是漏了复用",
    };
    for (const char* tip : tips)
    {
        Box* line = makeRow(8.0f, AlignItems::FLEX_START);
        line->setMarginTop(8.0f);

        Box* dot = new Box(Axis::COLUMN);
        dot->setDimensions(6.0f, 6.0f);
        dot->setMarginTop(7.0f);
        dot->setShrink(0.0f);
        dot->setBackgroundColor(rgb(120, 190, 255));
        dot->setCornerRadius(3.0f);
        line->addView(dot);

        Label* text = makeText(tip, 14.0f, rgb(180, 190, 205));
        text->setGrow(1.0f);
        line->addView(text);

        tipsCard->addView(line);
    }
    page->addView(tipsCard);

    // 顺便在页内嵌一个短列表，方便看回收情况
    Box* cellsCard = makeCard();
    cellsCard->addView(makeText("内嵌短列表（260 条）", 16.0f, rgb(200, 208, 220)));

    RecyclerFrame* inner = new RecyclerFrame();
    inner->setWidthPercentage(100.0f);
    inner->setHeight(220.0f);
    inner->setItemHeight(44.0f);
    inner->setPreFetchLine(1);
    inner->setMarginTop(10.0f);
    inner->registerCell(bklv::RecyclerDataSource::DEFAULT_CELL_ID, [] { return new SimpleCell(); });

    static ListSource innerSource(260, 44.0f);
    inner->setDataSource(&innerSource);
    cellsCard->addView(inner);
    page->addView(cellsCard);

    result.view  = page;
    result.list  = inner;
    result.stats = stats;
    return result;
}

} // namespace demo

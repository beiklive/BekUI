#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/bklv.hpp"

// 复用 demo 自己的回收列表单元（RecyclerCell 是库的扩展点）
#include "app/pages/list_items.hpp"

using namespace bklv;

namespace demo
{

ListPageResult buildListPage()
{
    ListPageResult result;

    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeightPercentage(100.0f);
    page->setGap(getTheme().number("metric.spacing.sm", 8.0f));

    page->addView(makeHint(getStr("list/hint")));

    // —— 网格区块（RecyclerFrame 的多列模式；Grid 不另设类型）——
    RecyclerFrame* grid = new RecyclerFrame();
    grid->setWidthPercentage(100.0f);
    grid->setHeight(210.0f); // 固定高度：上面网格、下面列表
    grid->setItemHeight(140.0f);
    grid->setColumns(3); // 多列模式
    grid->setPreFetchLine(1);
    grid->setShowScrollingIndicator(false);
    grid->registerCell(RecyclerDataSource::DEFAULT_CELL_ID, [] { return new GridCell(); });

    static ListSource gridSource(24, 140.0f); // 数据源要活到列表销毁
    grid->setDataSource(&gridSource);
    page->addView(grid);


    RecyclerFrame* list = new RecyclerFrame();
    list->setWidthPercentage(100.0f);
    list->setGrow(1.0f);
    list->setItemHeight(getTheme().number("metric.list.itemHeight", 64.0f));
    list->setPreFetchLine(1);
    list->setShowScrollingIndicator(true);
    list->registerCell(RecyclerDataSource::DEFAULT_CELL_ID, [] { return new ListCell(); });

    // 数据源要活到列表销毁
    static ListSource source(1000, getTheme().number("metric.list.itemHeight", 64.0f));
    list->setDataSource(&source);

    page->addView(list);

    result.view          = page;
    result.list          = list;
    return result;
}

} // namespace demo

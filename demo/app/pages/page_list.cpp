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

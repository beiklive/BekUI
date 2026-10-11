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

ListPage buildListPage()
{
    ListPage result;
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeightPercentage(100.0f);
    page->setGap(10.0f);

    page->addView(makeTitle("虚拟列表"));
    page->addView(makeHint("1000 条数据，滚出视口的行会被回收进对象池，"
                           "新进入视口的行复用池里的控件。左右或上下键切换焦点，滚动条会跟随。"));

    RecyclerFrame* list = new RecyclerFrame();
    list->setWidthPercentage(100.0f);
    list->setGrow(1.0f);
    list->setItemHeight(64.0f);
    list->setPreFetchLine(1);
    list->setShowScrollingIndicator(true);
    list->registerCell(bklv::RecyclerDataSource::DEFAULT_CELL_ID, [] { return new ListCell(); });

    // 数据源要活到列表销毁
    static ListSource source(1000, 64.0f);
    list->setDataSource(&source);

    page->addView(list);

    result.view          = page;
    result.list          = list;
    result.selfScrolling = true;
    return result;
}

} // namespace demo

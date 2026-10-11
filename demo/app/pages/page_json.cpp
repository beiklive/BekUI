#include "app/pages/pages.hpp"

#include <memory>
#include <string>

#include "app/page_helpers.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/views/json_cell.hpp"
#include "bklv/views/recycler.hpp"

using namespace bklv;

namespace demo
{

namespace
{

// 固定几行的数据源：这一页的重点是"模板来自 JSON + 绑定填值"，
// 数据本身随便造几条就够。
constexpr size_t JSON_ROW_COUNT = 8;

class JsonListSource : public RecyclerDataSource
{
  public:
    size_t getItemCount() override { return JSON_ROW_COUNT; }
    float getItemHeight(size_t) override { return 74.0f; } // 64 的行高 + 间距
};

// 绑定回调：JSON 只声明字段名，取值在 C++ 这边给。
// 换成任何数据来源都行，JSON 不用改，RecyclerDataSource 也不用改。
std::string binderFor(const std::string& field, size_t index)
{
    const int row = (int)index + 1;

    if (field == "thumb")
        return "resources/testcard.png";
    if (field == "title")
        return getStr("json/row_title", row);
    if (field == "subtitle")
        return getStr("json/row_subtitle", row);

    return {};
}

} // namespace

Box* buildJsonPage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setHeight(AUTO);

    page->addView(makeTitle(getStr("tab/json")));
    page->addView(makeHint(getStr("json/hint")));

    // ---- 1. 一整块界面来自 JSON ----
    Box* card = makeCard();
    card->addView(makeText(getStr("json/file_section"), 16.0f, rgb(200, 208, 220)));

    View* fromJson = createFromJSONFile("resources/views/page_json.json");
    if (fromJson != nullptr)
    {
        fromJson->setMarginTop(10.0f);
        card->addView(fromJson);
    }
    else
    {
        // 装载失败时不要留空白：日志里已经有具体原因（文件缺失/语法/属性）
        Label* failed = makeText(getStr("json/load_failed"), 14.0f, rgb(235, 130, 130));
        failed->setMarginTop(10.0f);
        card->addView(failed);
    }
    page->addView(card);

    // ---- 2. 列表项模板也来自 JSON ----
    Box* listCard = makeCard();
    listCard->addView(makeText(getStr("json/list_section"), 16.0f, rgb(200, 208, 220)));

    RecyclerFrame* list = new RecyclerFrame();
    list->setWidthPercentage(100.0f);
    list->setHeight(300.0f);
    list->setMarginTop(10.0f);
    list->setGap(10.0f);
    list->setDataSource(new JsonListSource());

    // 模板只装一次；之后每个 cell 都是"同一个模板再来一份"（实例化，不重新解析）
    const std::shared_ptr<const ViewTemplate> itemTemplate = ViewTemplate::load("resources/views/list_item.json");
    list->registerCell(RecyclerDataSource::DEFAULT_CELL_ID,
        [itemTemplate] { return JSONRecyclerCell::create(itemTemplate, binderFor); });

    listCard->addView(list);
    page->addView(listCard);

    return page;
}

} // namespace demo

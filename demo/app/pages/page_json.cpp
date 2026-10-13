#include <memory>

#include "app/pages.hpp"

#include "app/ui.hpp"
#include "bklv/bklv.hpp"

using namespace bklv;

namespace demo
{

namespace
{

// 固定几行的数据源：这一页的重点是"模板来自 JSON + 绑定填值"
constexpr size_t JSON_ROW_COUNT = 8;

class JsonListSource : public RecyclerDataSource
{
  public:
    size_t getItemCount() override { return JSON_ROW_COUNT; }
    float getItemHeight(size_t) override { return getTheme().number("metric.list.itemHeight", 64.0f); }
};

// 绑定回调：JSON 只声明字段名，取值在 C++ 这边给；换数据源不用改 JSON
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
    page->setHeightPercentage(100.0f);
    page->setGap(getTheme().number("metric.spacing.sm", 8.0f));

    // 整块界面（标题 + 说明 + 卡片）都来自这一个 JSON 文件
    // 视图描述现在放主题里（view/ 目录）：结构随主题走，缺了会回退 default 主题
    View* fromJson = createViewFromTheme("page_json");
    if (fromJson != nullptr)
        page->addView(fromJson);
    else
        page->addView(makeHint(getStr("json/failed")));

    // 列表项模板同样来自 JSON，用 JSONRecyclerCell 绑定字段
    RecyclerFrame* list = new RecyclerFrame();
    list->setWidthPercentage(100.0f);
    list->setGrow(1.0f);
    list->setItemHeight(getTheme().number("metric.list.itemHeight", 64.0f));
    list->setShowScrollingIndicator(true);

    std::shared_ptr<const ViewTemplate> itemTemplate =
        loadViewTemplateFromTheme("list_item");
    if (itemTemplate != nullptr)
    {
        list->registerCell(RecyclerDataSource::DEFAULT_CELL_ID,
            [itemTemplate] { return JSONRecyclerCell::create(itemTemplate, binderFor); });
    }

    static JsonListSource source;
    list->setDataSource(&source);
    page->addView(list);

    return page;
}

} // namespace demo

#include "bklv/widgets/recycler_view.hpp"

#include "bklv/core/json_view.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/views/data_source.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

// ---------------- RecyclerItemCell ----------------

RecyclerItemCell::RecyclerItemCell()
    : RecyclerCell(true)
{
    // 外观要靠 AppearanceView 画（RecyclerCell 是 Box 派生），所以内部持有一个
    this->content = new AppearanceView("base/rectangle.card", false);
    this->addView((View*)this->content);
}

RecyclerItemCell::~RecyclerItemCell() = default;

void RecyclerItemCell::setAppearanceName(const std::string& name)
{
    this->appearanceName = name;
    if (this->content != nullptr)
        this->content->setAppearanceName(name);
}

void RecyclerItemCell::setViewTemplateName(const std::string& name)
{
    // v1：记下来（模板实例化接在 ② 的绑定挂点里，和 bindItem 一起做）
    this->viewTemplateName = name;
}

void RecyclerItemCell::setIndex(size_t value)
{
    RecyclerCell::setIndex(value);
    if (this->binder && this->content != nullptr)
        this->binder(*(View*)this->content, value);
}

void RecyclerItemCell::prepareForReuse() { }

// ---------------- RecyclerView ----------------

RecyclerView::RecyclerView()
{
    this->rebuildItemCells();
}

void RecyclerView::setDataSourceName(const std::string& name)
{
    this->dataSourceName = name;
    RecyclerDataSource* source = data::createSource(name); // 没注册过会打警告并返回 nullptr
    if (source != nullptr)
        this->setDataSource(source);
}

void RecyclerView::setItemAppearance(const std::string& name)
{
    this->itemAppearance = name;
    this->rebuildItemCells();
}

void RecyclerView::setItemTemplate(const std::string& name)
{
    this->itemTemplate = name;
    this->rebuildItemCells();
}

void RecyclerView::setColumns(int count)
{
    if (count < 1)
    {
        BKLV_LOG_WARNING("Recycler 的 columns 至少是 1（收到 %d）", count);
        count = 1;
    }
    this->columns = count; // v1：记录；多列排布接在后面（见目标 ① 的 columns 项）
}

void RecyclerView::setMinItemWidth(float width) { this->minItemWidth = width; }
void RecyclerView::setSlotLoading(const std::string& name) { this->slotLoading = name; }
void RecyclerView::setSlotEmpty(const std::string& name) { this->slotEmpty = name; }
void RecyclerView::setOnEndName(const std::string& name) { this->onEndName = name; }

void RecyclerView::rebuildItemCells()
{
    const std::string appearance = this->itemAppearance;
    const std::string viewTemplate = this->itemTemplate;
    this->registerCell(RecyclerDataSource::DEFAULT_CELL_ID, [appearance, viewTemplate] {
        RecyclerItemCell* cell = new RecyclerItemCell();
        if (!appearance.empty())
            cell->setAppearanceName(appearance);
        if (!viewTemplate.empty())
            cell->setViewTemplateName(viewTemplate);
        return cell;
    });
}

const AttributeTable& RecyclerView::attributes() const
{
    // v1：属性表先留空（类型已可被 JSON 建出来）；字符串/数值属性下一轮补，
    // 需要先确认 ResolvedValue 的成员名，避免猜错。
    static const AttributeTable table = {};
    return table;
}

void registerRecyclerViewType()
{
    registerViewType("Recycler", [] { return (View*)new RecyclerView(); });
}

} // namespace bklv

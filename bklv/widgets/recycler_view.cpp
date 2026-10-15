#include "bklv/widgets/recycler_view.hpp"

#include "bklv/core/json_view.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/views/data_source.hpp"
#include "bklv/views/item_binder.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

// ---------------- RecyclerItemCell ----------------

RecyclerItemCell::RecyclerItemCell()
    : RecyclerCell(true)
{
    // 内容**惰性创建**：模板由 setAppearanceName / setViewTemplateName 决定
    // （都没有时用默认外观兜底）。外观要靠 AppearanceView 画，而 RecyclerCell 是 Box 派生，
    // 所以内部持有一个 AppearanceView。
}

void RecyclerItemCell::ensureContent(const std::string& appearanceFallback)
{
    if (this->content != nullptr)
        return;
    this->content = new AppearanceView(appearanceFallback, false);
    // 内容必须铺满 cell：RecyclerCell 的宽高由 layoutCell 给，内容自己没有尺寸就什么都画不出来
    this->content->setWidthPercentage(100.0f);
    this->content->setHeightPercentage(100.0f);
    this->addView((View*)this->content);
}

void RecyclerItemCell::setItemBinderName(const std::string& name)
{
    this->binder = binding::findItemBinder(name); // 没注册过会警告并给空 function
}

RecyclerItemCell::~RecyclerItemCell() = default;

void RecyclerItemCell::setAppearanceName(const std::string& name)
{
    this->appearanceName = name;
    this->ensureContent(name.empty() ? "base/rectangle.card" : name);
    if (this->content != nullptr && !name.empty())
        this->content->setAppearanceName(name);
}

void RecyclerItemCell::setViewTemplateName(const std::string& name)
{
    this->viewTemplateName = name;
    if (name.empty() || this->content != nullptr)
        return;

    // 视图模板：复用现成机制（可缓存、与语言无关），名字用主题里的视图名（不带 view/ 前缀）
    std::shared_ptr<const ViewTemplate> tmpl = loadViewTemplateFromTheme(name);
    if (tmpl == nullptr)
    {
        BKLV_LOG_WARNING("itemTemplate \"%s\" 找不到视图模板，回退到默认外观", name.c_str());
        this->ensureContent("base/rectangle.card");
        return;
    }
    View* view = tmpl->instantiate();
    if (view == nullptr)
    {
        BKLV_LOG_WARNING("itemTemplate \"%s\" 实例化失败，回退到默认外观", name.c_str());
        this->ensureContent("base/rectangle.card");
        return;
    }
    this->addView(view);
    this->templateContent = view;
}

void RecyclerItemCell::setIndex(size_t value)
{
    RecyclerCell::setIndex(value);
    this->ensureContent("base/rectangle.card"); // 没设模板时的兜底
    View* target = this->content != nullptr ? (View*)this->content : this->templateContent;
    if (this->binder && target != nullptr)
        this->binder(*target, value);
}

void RecyclerItemCell::setSectionIndex(size_t sectionValue, size_t indexValue)
{
    RecyclerCell::setSectionIndex(sectionValue, indexValue);
    // 绑定挂点：两个索引入口都要走，否则（分段路径下）绑定函数永远不会被调用
    this->ensureContent("base/rectangle.card");
    View* target = this->content != nullptr ? (View*)this->content : this->templateContent;
    if (this->binder && target != nullptr)
        this->binder(*target, indexValue);
}

void RecyclerItemCell::prepareForReuse() { }

// ---------------- RecyclerHeaderCell ----------------

RecyclerHeaderCell::RecyclerHeaderCell()
    : RecyclerSectionHeader()
{
}

RecyclerHeaderCell::~RecyclerHeaderCell() = default;

void RecyclerHeaderCell::setAppearanceName(const std::string& name)
{
    if (this->content == nullptr)
    {
        this->content = new AppearanceView(name.empty() ? "base/section" : name, false);
        this->content->setWidthPercentage(100.0f);
        this->content->setHeightPercentage(100.0f);
        this->addView((View*)this->content);
    }
    else if (!name.empty())
        this->content->setAppearanceName(name);
}

void RecyclerHeaderCell::onSectionTitle(size_t, const std::string& title)
{
    if (this->content != nullptr)
        this->content->setText(title); // 段标题由数据源给，这里套进段头模板
}

// ---------------- RecyclerView ----------------

RecyclerView::RecyclerView()
{
    // 与 demo 里 C++ 搭列表的用法保持一致（不然"看着一样、行为不一样"）
    this->setPreFetchLine(1);
    this->setShowScrollingIndicator(true);
    this->rebuildItemCells();
}

void RecyclerView::setDataSourceName(const std::string& name)
{
    // JSON 里可以写 "@data/pokedex"（与 @theme/ @i18n/ 同一套前缀习惯），也可以直接写名字
    std::string plain = name;
    if (plain.rfind("@data/", 0) == 0)
        plain = plain.substr(6);
    this->dataSourceName = plain;
    RecyclerDataSource* source = data::createSource(plain); // 没注册过会打警告并返回 nullptr
    if (source != nullptr)
    {
        this->ownedSource.reset(source); // 由本视图持有：JSON 建出来的对象总得有人管
        this->setDataSource(source);
    }
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
    // 真正生效的是视图的能力（views 层，纯布局）；这里只记录并转调
    this->columns = count < 1 ? 1 : count;
    RecyclerFrame::setColumns(this->columns); // >1 会在视图层警告并当 1 处理（多列未做）
}

void RecyclerView::setMinItemWidth(float width) { this->minItemWidth = width; }
namespace
{

// 槽位视图：从主题的视图模板实例化（和 itemTemplate 走同一套机制，可缓存、与语言无关）
View* makeSlotView(const std::string& templateName)
{
    if (templateName.empty())
        return nullptr;
    std::shared_ptr<const ViewTemplate> tmpl = loadViewTemplateFromTheme(templateName);
    if (tmpl == nullptr)
    {
        BKLV_LOG_WARNING("槽位模板 \"%s\" 找不到（slotLoading/slotEmpty 写的是主题里的视图名）",
            templateName.c_str());
        return nullptr;
    }
    return tmpl->instantiate();
}

} // namespace

void RecyclerView::setSlotLoading(const std::string& name)
{
    this->slotLoading = name;
    View* view = makeSlotView(name);
    if (view != nullptr)
    {
        this->slotLoadingView = view;
        this->addView(view);
        this->applySlotState(); // 建好就按当前状态套一次（否则初始 CONTENT 下槽位会一直显示）
    }
}
void RecyclerView::setSlotEmpty(const std::string& name)
{
    this->slotEmpty = name;
    View* view = makeSlotView(name);
    if (view != nullptr)
    {
        this->slotEmptyView = view;
        this->addView(view);
        this->applySlotState();
    }
}

void RecyclerView::update()
{
    RecyclerFrame::update();

    // 槽位由**数据源**决定显示哪个（JSON 只声明长什么样，这里不做任何语义判断）
    RecyclerSlotState state = RecyclerSlotState::CONTENT;
    if (this->getDataSource() != nullptr)
        state = this->getDataSource()->getSlotState();

    if (state != this->slotState)
    {
        this->slotState = state;
        this->applySlotState();
    }
}

void RecyclerView::applySlotState()
{
    const bool showLoading = this->slotState == RecyclerSlotState::LOADING;
    const bool showEmpty = this->slotState == RecyclerSlotState::EMPTY;
    if (this->slotLoadingView != nullptr)
        this->slotLoadingView->setVisibility(showLoading ? Visibility::VISIBLE : Visibility::GONE);
    if (this->slotEmptyView != nullptr)
        this->slotEmptyView->setVisibility(showEmpty ? Visibility::VISIBLE : Visibility::GONE);
}

bool RecyclerView::isSlotVisible(RecyclerSlotState state) const
{
    const View* view = nullptr;
    if (state == RecyclerSlotState::LOADING)
        view = this->slotLoadingView;
    else if (state == RecyclerSlotState::EMPTY)
        view = this->slotEmptyView;
    return view != nullptr && view->getVisibility() == Visibility::VISIBLE;
}
void RecyclerView::setOnEndName(const std::string& name) { this->onEndName = name; }

void RecyclerView::setSectionHeaderAppearance(const std::string& name)
{
    this->sectionHeaderAppearance = name;
    if (name.empty())
        return;

    // 段头：注册一个 identifier + 告诉回收器段头用哪个 identifier
    const std::string appearance = name;
    this->registerCell("section_header", [appearance] {
        RecyclerHeaderCell* header = new RecyclerHeaderCell();
        header->setAppearanceName(appearance);
        return header;
    });
    this->setSectionHeaderIdentifier("section_header");
}

void RecyclerView::setItemBinderName(const std::string& name)
{
    this->itemBinderName = name;
    this->rebuildItemCells();
}

void RecyclerView::rebuildItemCells()
{
    const std::string appearance = this->itemAppearance;
    const std::string viewTemplate = this->itemTemplate;
    const std::string binderName = this->itemBinderName;
    this->registerCell(RecyclerDataSource::DEFAULT_CELL_ID, [appearance, viewTemplate, binderName] {
        RecyclerItemCell* cell = new RecyclerItemCell();
        if (!appearance.empty())
            cell->setAppearanceName(appearance);
        if (!viewTemplate.empty())
            cell->setViewTemplateName(viewTemplate);
        if (!binderName.empty())
            cell->setItemBinderName(binderName);
        return cell;
    });
}

const AttributeTable& RecyclerView::attributes() const
{
    // 属性表：JSON 只写"名字/规则"，不写每一项（数据在 C++ 数据源里）
    static const AttributeTable table = {
        { "data", AttributeKind::STRING, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerView*>(v)->setDataSourceName(r.text);
         }, nullptr },
        { "itemAppearance", AttributeKind::STRING, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerView*>(v)->setItemAppearance(r.text);
         }, nullptr },
        { "itemTemplate", AttributeKind::STRING, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerView*>(v)->setItemTemplate(r.text);
         }, nullptr },
        { "columns", AttributeKind::NUMBER, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerView*>(v)->setColumns((int)r.number);
         }, nullptr },
        { "minItemWidth", AttributeKind::NUMBER, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerView*>(v)->setMinItemWidth(r.number);
         }, nullptr },
        { "slotLoading", AttributeKind::STRING, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerView*>(v)->setSlotLoading(r.text);
         }, nullptr },
        { "slotEmpty", AttributeKind::STRING, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerView*>(v)->setSlotEmpty(r.text);
         }, nullptr },
        { "onEnd", AttributeKind::STRING, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerView*>(v)->setOnEndName(r.text);
         }, nullptr },
        { "itemBinder", AttributeKind::STRING, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerView*>(v)->setItemBinderName(r.text);
         }, nullptr },
        { "sectionHeaderAppearance", AttributeKind::STRING, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerView*>(v)->setSectionHeaderAppearance(r.text);
         }, nullptr },
        { "itemHeight", AttributeKind::NUMBER, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerFrame*>(v)->setItemHeight(r.number);
         }, nullptr },
        { "preFetchLine", AttributeKind::NUMBER, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerFrame*>(v)->setPreFetchLine((size_t)r.number);
         }, nullptr },
        { "showScrollingIndicator", AttributeKind::BOOL, [](View* v, const ResolvedValue& r) {
             static_cast<RecyclerFrame*>(v)->setShowScrollingIndicator(r.boolean);
         }, nullptr },
        { "count", AttributeKind::STRING, [](View* v, const ResolvedValue& r) {
             // v1：只认 "infinite"（表示"总数由数据源决定、可能无限"），别的值警告
             if (r.text != "infinite")
                 BKLV_LOG_WARNING("Recycler 的 count 只支持 \"infinite\" 或省略（收到 \"%s\"）",
                     r.text.c_str());
             (void)v;
         }, nullptr },
    };
    return table;
}

void registerRecyclerViewType()
{
    registerViewType("Recycler", [] { return (View*)new RecyclerView(); });
}

} // namespace bklv

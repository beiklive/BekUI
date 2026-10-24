#include "bklv/ui/widgets/list/recycler.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/foundation/core/logger.hpp"
#include "bklv/ui/theme/theme_data.hpp"

namespace bklv
{

// 单帧最多新建多少个 cell。数据源刚加载完时视口里可能有很多行，
// 一次全建出来会造成明显卡顿，所以分摊到几帧。
static constexpr size_t MAX_CELLS_PER_FRAME = 12;

// ---------------------------------------------------------------------------
// RecyclerCell
// ---------------------------------------------------------------------------

RecyclerCell::RecyclerCell(bool absolutePosition)
    : Box(Axis::ROW)
{
    // 放进 RecyclerFrame 时由它手动摆位置，所以默认绝对定位、不参与流式布局。
    // 但子类也可能只是想复用"可复用控件"这套接口（比如 tab 栏的格子），
    // 那种情况要传 false，否则所有格子会叠在原点（这个坑实测踩过）。
    if (absolutePosition)
        this->setPositionType(PositionType::ABSOLUTE);
}

RecyclerSectionHeader::RecyclerSectionHeader()
    : RecyclerCell(false)
{
    // 段头是普通流式控件（由 RecyclerFrame 用 translation 摆位置），
    // 里面自己放内容；基类只管把标题存下来。
    this->setPositionType(PositionType::RELATIVE);
}

void RecyclerSectionHeader::onSectionTitle(size_t section, const std::string& title)
{
    this->headerSection = section;
    this->headerTitle   = title;
}

// ---------------------------------------------------------------------------
// RecyclerFrame
// ---------------------------------------------------------------------------

RecyclerFrame::RecyclerFrame()
{
    // 内容容器也是绝对定位：它的大小由 RecyclerFrame 直接设定。
    this->contentBox = new Box(Axis::COLUMN);
    this->contentBox->setPositionType(PositionType::ABSOLUTE);
    this->contentBox->setShrink(0.0f);

    this->setContentView(this->contentBox);
}

RecyclerFrame::~RecyclerFrame()
{
    // 先清掉池子之外的引用，池子里的 cell 不在树上，直接删。
    for (auto& entry : this->cellPool)
        for (RecyclerCell* cell : entry.second)
            delete cell;

    this->cellPool.clear();
    this->activeCells.clear();
}

void RecyclerFrame::registerCell(const std::string& identifier, std::function<RecyclerCell*()> allocation)
{
    this->cellAllocations[identifier] = std::move(allocation);
}

RecyclerCell* RecyclerFrame::dequeueReusableCell(const std::string& identifier)
{
    auto it = this->cellPool.find(identifier);
    if (it != this->cellPool.end() && !it->second.empty())
    {
        RecyclerCell* cell = it->second.back();
        it->second.pop_back();
        cell->prepareForReuse();
        return cell;
    }

    auto allocIt = this->cellAllocations.find(identifier);
    if (allocIt == this->cellAllocations.end())
    {
        Logger::error("RecyclerFrame: 未注册的 cell 类型 " + identifier);
        return nullptr;
    }

    RecyclerCell* cell = allocIt->second();
    if (cell != nullptr)
    {
        cell->reuseIdentifier = identifier;
        this->createdCellCount++;
    }

    return cell;
}

void RecyclerFrame::setDataSource(RecyclerDataSource* source)
{
    if (source != nullptr)
    {
        // 数据到了：解除单飞并把新条目**增量**接上（不全量重建）
        source->setLoadListener([this] {
            this->requestInFlight = false;
            this->notifyItemsAppended();
        });
    }

    this->dataSource = source;
    this->reloadData();
}

void RecyclerFrame::setItemHeight(float height)
{
    this->itemHeight = height;
    this->reloadData();
}

void RecyclerFrame::reloadData()
{
    if (this->contentBox == nullptr)
        return;

    // 旧的全部回收，避免尺寸变化后错位。
    // 先收集再回收：recycleCell 会 erase activeCells，边遍历边 erase 会让迭代器失效
    // （文件列表跳目录时踩过：列表项一多就崩在 reloadData 里）。
    std::vector<size_t> activeIndices;
    activeIndices.reserve(this->activeCells.size());
    for (const auto& entry : this->activeCells)
        activeIndices.push_back(entry.first);
    for (size_t index : activeIndices)
        this->recycleCell(index);

    this->buildRowOffsets();

    if (this->contentBox != nullptr)
        this->updateContentHeight();

    this->setContentOffsetY(0.0f, false);
}

void RecyclerFrame::notifyDataChanged()
{
    // 数据内容变了但条数没变：让所有已显示的 cell 重新绑定一次数据。
    std::vector<size_t> indices;
    indices.reserve(this->activeCells.size());
    for (const auto& entry : this->activeCells)
        indices.push_back(entry.first);

    for (size_t index : indices)
        this->recycleCell(index);

    this->buildRowOffsets();

    if (this->contentBox != nullptr)
        this->updateContentHeight();
}

// ---------------------------------------------------------------------------
// 行高与偏移
// ---------------------------------------------------------------------------

void RecyclerFrame::requestFocusIndex(size_t index)
{
    if (index < this->rows.size())
    {
        // 行已经在表里：不需要"等数据"，由调用方直接滚即可
        if (this->pendingFocusIndex == index)
            this->pendingFocusIndex = (size_t)-1;
        return;
    }

    this->pendingFocusIndex = index;
    this->maybeRequestMore(); // 先要数据
}

size_t RecyclerFrame::takePendingFocusIndex()
{
    // 只有"目标行已经出现"才交给调用方；否则继续等（数据分页可能要好几次）
    if (this->pendingFocusIndex == (size_t)-1 || this->pendingFocusIndex >= this->rows.size())
        return (size_t)-1;

    const size_t index = this->pendingFocusIndex;
    this->pendingFocusIndex = (size_t)-1;
    return index;
}

void RecyclerFrame::maybeRequestMore()
{
    if (this->dataSource == nullptr || this->requestInFlight || !this->dataSource->hasMore())
        return;

    // "末尾附近"：最后活跃行的行号 + 阈值 >= 总行数。没有活跃 cell（还没滚/首屏）也算够近。
    bool nearEnd = this->activeCells.empty();
    if (!this->activeCells.empty())
        nearEnd = this->activeCells.rbegin()->first + this->loadConfig.loadThreshold >= this->rows.size();
    if (!nearEnd)
        return;

    this->requestInFlight = true;
    this->dataSource->requestMore(this->loadConfig.pageSize, this->dataSource->getLoadedCount());
}

void RecyclerFrame::notifyItemsAppended()
{
    // 只有"单段 + 已经有行 + 条目变多"才是真追加；其余情况老实走全量重建。
    if (this->dataSource == nullptr || this->contentBox == nullptr)
    {
        this->notifyDataChanged();
        return;
    }
    if (this->dataSource->getSectionCount() > 1)
    {
        this->notifyDataChanged(); // 分段列表的段头/段内下标会变，增量不值当（也不假装支持）
        return;
    }

    size_t builtItems = 0;
    for (const Row& row : this->rows)
        if (row.kind == RowKind::ITEM)
            ++builtItems;

    const size_t totalItems = this->dataSource->getItemCount();
    if (this->rows.empty() || totalItems <= builtItems)
    {
        this->notifyDataChanged(); // 空表或"变少/不变" -> 这是重置，不是追加
        return;
    }

    for (size_t i = builtItems; i < totalItems; ++i)
    {
        Row row;
        row.kind      = RowKind::ITEM;
        row.section   = 0;
        row.itemIndex = i;
        row.top       = this->totalHeight;

        float height = this->dataSource->getItemHeightInSection(0, i);
        if (height <= 0.0f)
            height = this->itemHeight;

        row.height = height;
        this->totalHeight += height;
        this->rows.push_back(row);
    }

    // 视觉行顶：单列时根本不用（visualTopOf 走 rows[i].top），多列时才需要，且老实全量重算
    if (this->columns > 1)
        this->rebuildVisualTops();

    this->updateContentHeight();
    this->invalidate();
}

void RecyclerFrame::buildRowOffsets()
{
    this->rows.clear();
    this->totalHeight = 0.0f;

    if (this->dataSource == nullptr || this->contentBox == nullptr)
        return;

    const size_t sections = std::max<size_t>(1, this->dataSource->getSectionCount());

    for (size_t section = 0; section < sections; ++section)
    {
        // 段头：标题为空就不占行（不是画一个空表头）
        const std::string title = this->dataSource->getSectionTitle(section);
        if (!title.empty())
        {
            Row header;
            header.kind    = RowKind::HEADER;
            header.section = section;
            header.top     = this->totalHeight;
            header.height  = this->getSectionHeaderHeight(section);
            this->totalHeight += header.height;
            this->rows.push_back(header);
        }

        const size_t count = sections > 1 ? this->dataSource->getItemCountInSection(section)
                                          : this->dataSource->getItemCount();

        for (size_t i = 0; i < count; ++i)
        {
            Row row;
            row.kind      = RowKind::ITEM;
            row.section   = section;
            row.itemIndex = i;
            row.top       = this->totalHeight;

            float height = this->dataSource->getItemHeightInSection(section, i);
            if (height <= 0.0f)
                height = this->itemHeight;

            row.height = height;
            this->totalHeight += height;
            this->rows.push_back(row);
        }
    }
    this->rebuildVisualTops(); // 行高算完 -> 同步视觉行顶（多列用）
}

void RecyclerFrame::updateContentHeight()
{
    if (this->contentBox == nullptr)
        return;

    const float wanted = this->getEstimatedScrollHeight();
    if (wanted == this->appliedContentHeight)
        return; // 值没变就别动布局

    this->appliedContentHeight = wanted;
    this->contentBox->setHeight(wanted);
    this->invalidate();
}

float RecyclerFrame::getEstimatedScrollHeight() const
{
    if (this->dataSource == nullptr || !this->dataSource->hasMore())
        return this->totalHeight; // 没有更多：不虚报

    // 估算行高：优先用配置；没配就用"已加载项的平均高"；一个都没有就用 itemHeight
    float estimate = this->loadConfig.estimatedItemHeight;
    if (estimate <= 0.0f)
    {
        size_t itemCount = 0;
        float sum = 0.0f;
        for (const Row& row : this->rows)
            if (row.kind == RowKind::ITEM)
            {
                sum += row.height;
                ++itemCount;
            }
        estimate = itemCount > 0 ? sum / (float)itemCount : this->itemHeight;
    }
    if (estimate < 0.0f)
        estimate = 0.0f;

    // 估算行数：下一次大概会来多少（pageSize；0 时按 1 行算，保证"还有更多"时高度是增长的）
    const float rows = this->loadConfig.pageSize > 0 ? (float)this->loadConfig.pageSize : 1.0f;
    return this->totalHeight + estimate * rows;
}

size_t RecyclerFrame::getPooledCellCount() const
{
    size_t total = 0;
    for (const auto& entry : this->cellPool)
        total += entry.second.size();
    return total;
}

size_t RecyclerFrame::getPooledCellCount(const std::string& identifier) const
{
    const auto it = this->cellPool.find(identifier);
    return it == this->cellPool.end() ? 0 : it->second.size();
}

float RecyclerFrame::getSectionHeaderHeight(size_t section) const
{
    const float fromSource = this->dataSource->getSectionHeaderHeight(section);
    if (fromSource > 0.0f)
        return fromSource;

    return getTheme().number("metric.list.sectionHeaderHeight", 36.0f);
}

RecyclerFrame::RowKind RecyclerFrame::getRowKind(size_t row) const
{
    return row < this->rows.size() ? this->rows[row].kind : RowKind::ITEM;
}

size_t RecyclerFrame::getRowSection(size_t row) const
{
    return row < this->rows.size() ? this->rows[row].section : 0;
}

size_t RecyclerFrame::getRowItemIndex(size_t row) const
{
    return row < this->rows.size() ? this->rows[row].itemIndex : 0;
}

float RecyclerFrame::getRowHeight(size_t row) const
{
    return row < this->rows.size() ? this->rows[row].height : 0.0f;
}

std::string RecyclerFrame::rowIdentifier(size_t row) const
{
    if (row >= this->rows.size())
        return RecyclerDataSource::DEFAULT_CELL_ID;

    const Row& entry = this->rows[row];
    if (entry.kind == RowKind::HEADER)
        return this->sectionHeaderIdentifier;

    return this->dataSource->getItemIdentifierInSection(entry.section, entry.itemIndex);
}

void RecyclerFrame::bindCell(RecyclerCell* cell, size_t row) const
{
    if (cell == nullptr || row >= this->rows.size())
        return;

    const Row& entry = this->rows[row];

    if (entry.kind == RowKind::HEADER)
    {
        cell->setIndex(entry.section);
        // 虚函数钩子：段头 cell 自己决定怎么显示（无 RTTI）
        cell->onSectionTitle(entry.section, this->dataSource->getSectionTitle(entry.section));
        return;
    }

    cell->setSectionIndex(entry.section, entry.itemIndex);
}

float RecyclerFrame::getRowTop(size_t row) const
{
    if (row >= this->rows.size())
        return this->totalHeight;

    return this->rows[row].top;
}

size_t RecyclerFrame::findRowAtOffset(float y) const
{
    if (this->rows.empty())
        return 0;

    // rows 按 top 单调递增，直接二分。
    auto it = std::upper_bound(this->rows.begin(), this->rows.end(), y,
        [](float value, const Row& row) { return value < row.top; });
    if (it == this->rows.begin())
        return 0;

    return static_cast<size_t>(std::distance(this->rows.begin(), it) - 1);
}

// ---------------------------------------------------------------------------
// cell 摆放与回收
// ---------------------------------------------------------------------------

void RecyclerFrame::setColumns(int count)
{
    if (count < 1)
    {
        BKLV_LOG_WARNING("Recycler 的 columns 至少是 1（收到 %d）", count);
        count = 1;
    }
    this->columns = count;
    this->rebuildVisualTops();
}

// columns > 1 时：第 r 个**视觉行**的高度 = 该行内各 item 高度的最大值（保留不等高），
// top 依次累加。columns == 1 时清空（走 rows[index].top，与从前逐像素等价）。
void RecyclerFrame::rebuildVisualTops()
{
    this->visualTops.clear();
    if (this->columns <= 1)
        return;

    const size_t columns = (size_t)this->columns;
    float top = 0.0f;
    for (size_t rowStart = 0; rowStart < this->rows.size(); rowStart += columns)
    {
        float rowHeight = 0.0f;
        const size_t rowEnd = std::min(rowStart + columns, this->rows.size());
        for (size_t i = rowStart; i < rowEnd; i++)
            rowHeight = std::max(rowHeight, this->rows[i].height);

        for (size_t i = rowStart; i < rowEnd; i++)
            this->visualTops.push_back(top);
        top += rowHeight;
    }

    // 多列：真实内容高度是"视觉行高度之和"，不是逐条目累加
    // （否则滚动范围会偏大 columns 倍 —— 网格滚到底之前就出现大段空白）
    this->totalHeight = top;
}

float RecyclerFrame::visualTopOf(size_t index) const
{
    // columns = 1：视觉位置就是流式行的 top -> 与引入本函数之前**逐像素等价**
    if (this->columns <= 1)
        return index < this->rows.size() ? this->rows[index].top : 0.0f;

    // 多列：用预先算好的视觉行顶。
    // 已知限制：滚动范围仍按 rows 累加，所以"多列 + 不等高"时总高会偏大（等高于时完全一致）。
    return index < this->visualTops.size() ? this->visualTops[index] : 0.0f;
}

void RecyclerFrame::layoutCell(RecyclerCell* cell, size_t row)
{
    if (cell == nullptr || row >= this->rows.size())
        return;

    // 宽度跟随可视宽度（单列时等于整宽），纵向位置用 translation 直接摆（绝对定位已跳过 Yoga 布局）。
    const float cellWidth = this->columns > 0 ? this->getWidth() / (float)this->columns : this->getWidth();
    cell->setWidth(cellWidth);

    cell->setHeight(this->rows[row].height);
    cell->setTranslation((float)(row % (size_t)(this->columns > 0 ? this->columns : 1)) * cellWidth,
        this->visualTopOf(row));
}

void RecyclerFrame::recycleCell(size_t index)
{
    auto it = this->activeCells.find(index);
    if (it == this->activeCells.end())
        return;

    RecyclerCell* cell = it->second;
    this->activeCells.erase(it);

    if (cell == nullptr)
        return;

    // 从树上摘下来但不删除，放进池子里等下次复用。
    if (this->contentBox != nullptr)
        this->contentBox->removeView(cell, false);

    // 池上限：超出就**真正销毁**（内存有上限，不会滑过千行后堆上千个 cell）
    std::vector<RecyclerCell*>& pool = this->cellPool[cell->reuseIdentifier];
    if (shouldPoolCell(pool.size(), this->poolLimitPerIdentifier))
        pool.push_back(cell);
    else
        delete cell;
}

size_t RecyclerFrame::getReusableCellCount() const
{
    size_t total = 0;
    for (const auto& entry : this->cellPool)
        total += entry.second.size();
    return total;
}

// ---------------------------------------------------------------------------
// 每帧推进
// ---------------------------------------------------------------------------

void RecyclerFrame::update()
{
    this->maybeRequestMore();
    this->updateContentHeight();
    // 先把滚动、惯性、焦点跟随处理掉，这样下面读到的偏移才是本帧最终值。
    ScrollingFrame::update();

    if (this->dataSource == nullptr || this->contentBox == nullptr)
        return;

    const size_t count = this->rows.size();
    if (count == 0)
    {
        std::vector<size_t> all;
        all.reserve(this->activeCells.size());
        for (const auto& entry : this->activeCells)
            all.push_back(entry.first);
        for (size_t index : all)
            this->recycleCell(index);
        return;
    }

    const float offset   = this->getContentOffsetY();
    const float viewport = this->getHeight();
    // 预取量按行高估（固定行高时和以前一样；不定行高时是个保守估计）
    float averageHeight = this->itemHeight;
    if (count > 0)
        averageHeight = this->totalHeight / static_cast<float>(count);
    const float preFetch = averageHeight * static_cast<float>(this->preFetchLine);

    // 多列（Grid）时一个"视觉行"承载 columns 个条目：起点对齐到行首、终点补齐整行，
    // 否则一行只会被建出左半边（探针量到过：3 列只构造 2 个单元）。
    const size_t columns = this->columns > 0 ? static_cast<size_t>(this->columns) : 1;
    size_t first = this->findRowAtOffset(std::max(0.0f, offset - preFetch));
    if (columns > 1)
        first = (first / columns) * columns;

    size_t last = std::min(count - 1, this->findRowAtOffset(offset + viewport + preFetch));
    if (columns > 1 && last < count - 1)
        last = std::min(count - 1, ((last / columns) + 1) * columns - 1);

    // 1. 回收滚出可视区的
    //    先记住当前焦点：如果它对应的 cell 被回收了，清除"已自动滚动过"的标记，
    //    否则焦点回到这一项时不会再次滚动。
    View* focus   = getInputManager().getCurrentFocus();
    bool focusRecycled = false;
    {
        std::vector<size_t> toRecycle;
        for (const auto& entry : this->activeCells)
        {
            if (entry.first < first || entry.first > last)
                toRecycle.push_back(entry.first);
        }
        for (size_t index : toRecycle)
        {
            if (focus != nullptr && this->activeCells.count(index) > 0
                && this->activeCells[index] == focus)
                focusRecycled = true;
            this->recycleCell(index);
        }
    }

    if (focusRecycled)
        this->lastAutoScrolledView = nullptr;

    // 2. 补齐可视区内的
    size_t created = 0;
    for (size_t index = first; index <= last && index < count; ++index)
    {
        if (this->activeCells.find(index) != this->activeCells.end())
        {
            // 已经在显示，只需保证位置正确。
            this->layoutCell(this->activeCells[index], index);
            continue;
        }

        const std::string identifier = this->rowIdentifier(index);

        // 池子里有就直接复用；没有则受"单帧新建上限"限制，剩下的下一帧补。
        const bool reusable = this->getReusableCellCount() > 0;
        if (!reusable && created >= MAX_CELLS_PER_FRAME)
            continue;

        RecyclerCell* cell = this->dequeueReusableCell(identifier);
        if (cell == nullptr)
            continue;

        if (!reusable)
            created++;

        this->bindCell(cell, index);
        this->layoutCell(cell, index);
        this->contentBox->addView(cell, this->contentBox->getChildrenCount());
        this->activeCells[index] = cell;
    }
}

} // namespace bklv

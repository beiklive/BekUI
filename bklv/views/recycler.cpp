#include "bklv/views/recycler.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/core/logger.hpp"

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

    // 旧的全部回收，避免尺寸变化后错位
    for (auto& entry : this->activeCells)
        this->recycleCell(entry.first);

    this->buildRowOffsets();

    if (this->contentBox != nullptr)
        this->contentBox->setHeight(this->totalHeight);

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
        this->contentBox->setHeight(this->totalHeight);
}

// ---------------------------------------------------------------------------
// 行高与偏移
// ---------------------------------------------------------------------------

void RecyclerFrame::buildRowOffsets()
{
    this->rowOffsets.clear();
    this->totalHeight = 0.0f;

    if (this->dataSource == nullptr || this->contentBox == nullptr)
        return;

    const size_t count = this->dataSource->getItemCount();
    this->rowOffsets.reserve(count + 1);

    for (size_t i = 0; i < count; ++i)
    {
        this->rowOffsets.push_back(this->totalHeight);

        float height = this->dataSource->getItemHeight(i);
        if (height <= 0.0f)
            height = this->itemHeight;

        this->totalHeight += height;
    }

    // 末尾补一个哨兵，方便二分与"最后一行底部"计算
    this->rowOffsets.push_back(this->totalHeight);
}

float RecyclerFrame::getRowTop(size_t index) const
{
    if (index >= this->rowOffsets.size() - 1)
        return this->totalHeight;
    return this->rowOffsets[index];
}

size_t RecyclerFrame::findRowAtOffset(float y) const
{
    if (this->rowOffsets.size() < 2)
        return 0;

    // rowOffsets 单调递增，直接二分。
    auto it = std::upper_bound(this->rowOffsets.begin(), this->rowOffsets.end(), y);
    if (it == this->rowOffsets.begin())
        return 0;

    return static_cast<size_t>(std::distance(this->rowOffsets.begin(), it) - 1);
}

// ---------------------------------------------------------------------------
// cell 摆放与回收
// ---------------------------------------------------------------------------

void RecyclerFrame::layoutCell(RecyclerCell* cell, size_t index)
{
    if (cell == nullptr)
        return;

    // 宽度跟随可视宽度，纵向位置用 translation 直接摆（绝对定位已跳过 Yoga 布局）。
    cell->setWidth(this->getWidth());

    const float top    = this->getRowTop(index);
    const float height = this->getRowTop(index + 1) - top;

    cell->setHeight(height);
    cell->setTranslation(0.0f, top);
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

    this->cellPool[cell->reuseIdentifier].push_back(cell);
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
    // 先把滚动、惯性、焦点跟随处理掉，这样下面读到的偏移才是本帧最终值。
    ScrollingFrame::update();

    if (this->dataSource == nullptr || this->contentBox == nullptr)
        return;

    const size_t count = this->dataSource->getItemCount();
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
    const float preFetch = this->itemHeight * static_cast<float>(this->preFetchLine);

    const size_t first = this->findRowAtOffset(std::max(0.0f, offset - preFetch));
    const size_t lastRow = this->findRowAtOffset(offset + viewport + preFetch);
    const size_t last    = std::min(count - 1, lastRow);

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

        const std::string identifier = this->dataSource->getItemIdentifier(index);

        // 池子里有就直接复用；没有则受"单帧新建上限"限制，剩下的下一帧补。
        const bool reusable = this->getReusableCellCount() > 0;
        if (!reusable && created >= MAX_CELLS_PER_FRAME)
            continue;

        RecyclerCell* cell = this->dequeueReusableCell(identifier);
        if (cell == nullptr)
            continue;

        if (!reusable)
            created++;

        cell->setIndex(index);
        this->layoutCell(cell, index);
        this->contentBox->addView(cell, this->contentBox->getChildrenCount());
        this->activeCells[index] = cell;
    }
}

} // namespace bklv

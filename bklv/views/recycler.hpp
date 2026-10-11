#pragma once

// 虚拟列表，对应 borealis 的 views/recycler.hpp。
//
// 核心机制：
//   - 对象池：滚出视口的 cell 不销毁，回收进队列，下次复用
//   - 复用标识：按 reuseIdentifier 分池，支持多种 cell 类型混排
//   - prepareForReuse()：复用前重置内容，避免残留上一条数据
//   - 预取：视口上下各多准备 preFetchLine 行，滚动时不出现白块
//
// 滚动由 ScrollingFrame 提供，cell 用绝对定位摆放（不参与普通流式布局），
// 每帧只调整"哪些 index 需要显示"，不重建整棵树。

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "bklv/views/scrolling_frame.hpp"

namespace bklv
{

// 列表项基类。用户继承它来定义自己的 cell。
class RecyclerCell : public Box
{
  public:
    // absolutePosition = true 时由 RecyclerFrame 手动摆位置（默认行为）。
    // 子类如果拿它当普通流式控件用（而不是放进 RecyclerFrame），传 false。
    explicit RecyclerCell(bool absolutePosition = true);

    // 这个 cell 当前承载的数据下标。
    size_t getIndex() const { return this->index; }

    // 复用时由 RecyclerFrame 调用。
    virtual void prepareForReuse() { }
    virtual void setIndex(size_t value) { this->index = value; }

    // 用户在子类里设置，用于区分不同类型的 cell。
    std::string reuseIdentifier;

  private:
    size_t index = 0;
};

// 数据源：列表需要知道"有多少条"以及"第 i 条用哪种 cell"。
class RecyclerDataSource
{
  public:
    virtual ~RecyclerDataSource() { }

    virtual size_t getItemCount() = 0;

    // 返回第 index 条的复用标识；默认全部用 DEFAULT_CELL_ID。
    virtual std::string getItemIdentifier(size_t) { return DEFAULT_CELL_ID; }

    // 可选：返回这一行的固定高度；返回 <= 0 表示用全局 itemHeight。
    virtual float getItemHeight(size_t) { return 0.0f; }

    // 复用标识的默认值。必须是 constexpr const char*，不能写成
    // `static inline const std::string`：std::string 需要动态初始化，
    // 而 inline const 的存储会被编译器放进只读段（__TEXT,__const），
    // 全局初始化器一写就 SIGBUS（-O3 下必现，现象是 main 之前就崩）。
    // const char* 没有动态初始化，隐式转成 std::string 供调用方使用。
    static constexpr const char* DEFAULT_CELL_ID = "default";
};

class RecyclerFrame : public ScrollingFrame
{
  public:
    RecyclerFrame();
    ~RecyclerFrame() override;

    // 注册一种 cell 的构造函数。
    void registerCell(const std::string& identifier, std::function<RecyclerCell*()> allocation);

    // 从池里取一个可复用的 cell。
    RecyclerCell* dequeueReusableCell(const std::string& identifier);

    void setDataSource(RecyclerDataSource* source);
    RecyclerDataSource* getDataSource() const { return this->dataSource; }

    // 每行高度（固定行高模式）。
    void setItemHeight(float height);
    float getItemHeight() const { return this->itemHeight; }

    // 视口上下各多预取几行。
    void setPreFetchLine(size_t lines) { this->preFetchLine = lines; }

    // 数据变了：重建内容尺寸并重新取数。
    void reloadData();
    void notifyDataChanged();

    // 当前正在显示的 cell 数量（调试用）。
    size_t getVisibleCellCount() const { return this->activeCells.size(); }

    size_t getReusableCellCount() const;

    // 从首次加载到现在总共新建过多少个 cell。
    // 它应该稳定在"视口内行数 + 预取"附近；如果随着滚动一直涨，说明回收没生效。
    size_t getCreatedCellCount() const { return this->createdCellCount; }

    void update() override;

    std::string getClassString() const override { return "bklv::RecyclerFrame"; }

  protected:
    // 内容高度完全由"数据条数 × 行高"决定，不需要 ScrollingFrame 去猜。
    void onContentSizeChanged() override { }

  private:
    // 计算某一行的顶部位置（累计前面的行高）。
    float getRowTop(size_t index) const;

    // 二分找第一个 top 大于等于 y 的行号。
    size_t findRowAtOffset(float y) const;

    // 把 cell 摆到第 index 行的位置。
    void layoutCell(RecyclerCell* cell, size_t index);

    // 取出 / 放回对象池。
    void recycleCell(size_t index);
    void buildRowOffsets();

    RecyclerDataSource* dataSource = nullptr;

    Box* contentBox = nullptr;

    float itemHeight = 60.0f;
    size_t preFetchLine = 1;

    // index -> cell（当前显示的）
    std::map<size_t, RecyclerCell*> activeCells;

    // reuseIdentifier -> 空闲 cell
    std::map<std::string, std::vector<RecyclerCell*>> cellPool;

    // 注册的构造函数
    std::map<std::string, std::function<RecyclerCell*()>> cellAllocations;

    // 每行的顶部偏移与总高度（行高不固定时用）
    std::vector<float> rowOffsets;
    float totalHeight = 0.0f;

    size_t createdCellCount = 0;
};

} // namespace bklv

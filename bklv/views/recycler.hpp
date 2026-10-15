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

    // 这个 cell 当前承载的数据：段号 + 段内下标。
    // 单段列表里段号恒为 0，getIndex() 就是行号（老用法不变）。
    size_t getIndex() const { return this->index; }
    size_t getSection() const { return this->section; }

    // 复用时由 RecyclerFrame 调用。
    virtual void prepareForReuse() { }
    virtual void setIndex(size_t value)
    {
        this->index   = value;
        this->section = 0;
    }
    virtual void setSectionIndex(size_t sectionValue, size_t indexValue)
    {
        this->section = sectionValue;
        this->index   = indexValue;
    }

    // 段头行绑定数据时会调它（默认什么都不做）。
    // 用它而不是 dynamic_cast：Switch 构建带 -fno-rtti，项目里禁止 RTTI。
    virtual void onSectionTitle(size_t section, const std::string& title)
    {
        (void)section;
        (void)title;
    }

    // 用户在子类里设置，用于区分不同类型的 cell。
    std::string reuseIdentifier;

  private:
    size_t index   = 0;
    size_t section = 0;
};

// 分段表头。默认什么都不画，子类按需要塞内容（一般放一个 Label）。
class RecyclerSectionHeader : public RecyclerCell
{
  public:
    RecyclerSectionHeader();

    size_t getSection() const { return this->headerSection; }

    // 子类重写它来显示标题（基类只存下来）
    void onSectionTitle(size_t section, const std::string& title) override;

  protected:
    std::string getSectionTitle() const { return this->headerTitle; }

  private:
    size_t headerSection = 0;
    std::string headerTitle;
};

// 数据源：列表需要知道"有多少条"以及"第 i 条用哪种 cell"。
// 加载与回收的配置（**上线可调**的那组旋钮）：每个字段都有安全默认值，不设也能跑。
// 第 1 刀只引入"存储"，行为在后续刀里接（这样每一步都能单独验收）。
struct RecyclerLoadConfig
{
    size_t pageSize = 0;               // 每次要多少条；0 = 由数据源自己决定
    size_t loadThreshold = 3;          // 距"已加载末尾"还剩几条就触发加载
    size_t prefetchLines = 1;          // 可视区之外预取几行
    size_t maxPoolPerIdentifier = 32;  // 每种 cell 的池上限，超出**真正销毁**（第 3 刀用）
    float estimatedItemHeight = 0.0f;  // 未知区域占位高度；0 = 用已加载项平均值
};

// 列表的"三态"：内容 / 加载中 / 空。由**数据源**决定现在该显示哪个（JSON 只声明槽位长什么样）。
enum class RecyclerSlotState
{
    CONTENT,
    LOADING,
    EMPTY,
};

class RecyclerDataSource
{
  public:
    virtual ~RecyclerDataSource() { }

    // ---- 单段用法（老写法，段数默认是 1）----
    virtual size_t getItemCount() { return 0; }
    // 现在该显示哪个槽位（默认 CONTENT）。JSON 里 slotLoading/slotEmpty 声明"长什么样"，
    // 到底显示哪个由这里决定 —— 不把判断塞进 JSON 当解释器。
    virtual RecyclerSlotState getSlotState() { return RecyclerSlotState::CONTENT; }

    // —— 增量加载（全部可选；默认实现 = "没有更多"，所以旧数据源零改动）——
    virtual bool hasMore() { return false; }                       // 还能不能再要
    virtual size_t getLoadedCount() { return this->getItemCount(); } // 当前已加载多少
    virtual void requestMore(size_t want, size_t from) { (void)want; (void)from; } // 异步发起
    virtual void setLoadListener(std::function<void()> listener) { (void)listener; } // 数据到了通知刷新

    // 返回第 index 条的复用标识；默认全部用 DEFAULT_CELL_ID。
    virtual std::string getItemIdentifier(size_t) { return DEFAULT_CELL_ID; }

    // 可选：返回这一行的固定高度；返回 <= 0 表示用全局 itemHeight。
    virtual float getItemHeight(size_t) { return 0.0f; }

    // ---- 分段用法（对标 borealis 的 numberOfSections / getItemCount(section)）----
    //
    // 命名说明：borealis 用的是重载 `getItemCount()` / `getItemCount(section)`，
    // 派生类只重写其中一个会把另一个**名字隐藏**掉（C++ 的名字隐藏规则），
    // 属于典型的坑。这里改成 `*InSection` 后缀：不会隐藏，单段代码也一行不用改。
    virtual size_t getSectionCount() { return 1; }

    // 第 section 段有多少条。默认落到单段实现。
    virtual size_t getItemCountInSection(size_t) { return this->getItemCount(); }

    // 段标题。**返回空串表示这一段没有表头行**（不是画一个空表头）。
    virtual std::string getSectionTitle(size_t) { return std::string(); }

    // 第 section 段第 index 条的复用标识；默认落到单段实现。
    virtual std::string getItemIdentifierInSection(size_t, size_t index)
    {
        return this->getItemIdentifier(index);
    }

    // 第 section 段第 index 条的高度；<= 0 表示用全局 itemHeight。
    virtual float getItemHeightInSection(size_t, size_t index)
    {
        return this->getItemHeight(index);
    }

    // 段头高度；<= 0 表示用主题里的 metric.list.sectionHeaderHeight。
    virtual float getSectionHeaderHeight(size_t) { return 0.0f; }

    // 复用标识的默认值。必须是 constexpr const char*，不能写成
    // `static inline const std::string`：std::string 需要动态初始化，
    // 而 inline const 的存储会被编译器放进只读段（__TEXT,__const），
    // 全局初始化器一写就 SIGBUS（-O3 下必现，现象是 main 之前就崩）。
    // const char* 没有动态初始化，隐式转成 std::string 供调用方使用。
    static constexpr const char* DEFAULT_CELL_ID = "default";
};

// 复用池的入池判定（纯函数，可单测）：limit == 0 表示"从不复用"，超出上限就销毁。
inline bool shouldPoolCell(size_t pooledCount, size_t limit)
{
    return limit > 0 && pooledCount < limit;
}

class RecyclerFrame : public ScrollingFrame
{
  public:
    // —— 多列（Grid）的**纯视图能力** ——
    // columns = 1 时一切与从前逐像素等价；> 1 的多列排布尚未启用（会警告）。
    void setColumns(int count);
    int getColumns() const { return this->columns; }

    // 第 index 个 item 的**视觉纵向位置**。columns = 1 时等于 rows[index].top（等价重构）。
    float visualTopOf(size_t index) const;

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

    // 段头用哪种 cell（不注册就不画段头）
    void setSectionHeaderIdentifier(const std::string& identifier)
    {
        this->sectionHeaderIdentifier = identifier;
    }

    // ---- 行模型（表头行 + 条目行展开成一张平表，测试与调试用）----
    enum class RowKind
    {
        HEADER,
        ITEM,
    };

    size_t getRowCount() const { return this->rows.size(); }
    RowKind getRowKind(size_t row) const;
    size_t getRowSection(size_t row) const;
    size_t getRowItemIndex(size_t row) const; // 段内下标
    float getRowTop(size_t row) const;
    float getRowHeight(size_t row) const;
    float getTotalHeight() const { return this->totalHeight; }

    // **未知区域的估算滚动高度**（给滚动条/边界用，不参与布局）：
    // 还有更多时 = 真实高度 + 估算行高 × 估算行数；没有更多时 == 真实高度。
    float getEstimatedScrollHeight() const;

    // 数据变了：重建内容尺寸并重新取数。
    void reloadData();
    void notifyDataChanged();

    // 加载触发：滚到"末尾附近"就向数据源要下一页（单飞去抖；数据到了走增量追加）。
    // 条件用"最后活跃行 + loadThreshold >= 行数"，所以不依赖滚动器的内部状态。
    void maybeRequestMore();
    bool isLoadingMore() const { return this->requestInFlight; }

    // **增量追加**：数据源多出了条目时用它，只补尾巴（单段走真增量；其余情况内部回落到全量）。
    // 与 notifyDataChanged 的区别就是"不重算前面的行" —— 分页加载场景每页调一次。
    void notifyItemsAppended();

    // 加载/回收的行为旋钮（可 JSON 可 C++ 设置；默认值见 RecyclerLoadConfig）。
    // 第 1 刀只存不接：行为在后续刀里接上，这样每一刀都能单独验收。
    void setLoadConfig(const RecyclerLoadConfig& config)
    {
        this->loadConfig = config;
        this->poolLimitPerIdentifier = config.maxPoolPerIdentifier; // 上限跟着配置走
    }
    const RecyclerLoadConfig& getLoadConfig() const { return this->loadConfig; }

    // 当前正在显示的 cell 数量（调试用）。
    size_t getVisibleCellCount() const { return this->activeCells.size(); }

    // 池子里现在留着多少 cell（诊断/测试用：上限生效时它不该无限涨）
    size_t getPooledCellCount() const;
    size_t getPooledCellCount(const std::string& identifier) const;

    // 行号 -> 当前显示的 cell（调试与测试用；正常业务代码用不到）
    const std::map<size_t, RecyclerCell*>& getActiveCellsForTest() const
    {
        return this->activeCells;
    }

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
    // 展开后的行（表头行或条目行）
    struct Row
    {
        RowKind kind = RowKind::ITEM;
        size_t section = 0;
        size_t itemIndex = 0; // 段内下标（表头行不用）
        float top = 0.0f;
        float height = 0.0f;
    };

    // 二分找第一个 top 大于等于 y 的行号。
    size_t findRowAtOffset(float y) const;

    // 把 cell 摆到第 row 行的位置。
    void layoutCell(RecyclerCell* cell, size_t row);

    // 取第 row 行该用哪种 cell
    std::string rowIdentifier(size_t row) const;

    // 绑定数据（表头/条目走不同接口）
    void bindCell(RecyclerCell* cell, size_t row) const;

    float getSectionHeaderHeight(size_t section) const;

    // 取出 / 放回对象池。
    void recycleCell(size_t index);
    void buildRowOffsets();

    int columns = 1;
    RecyclerLoadConfig loadConfig;
    size_t poolLimitPerIdentifier = 32;
    bool requestInFlight = false;
    std::vector<float> visualTops; // columns > 1 时每个 item 的视觉纵向位置
    void rebuildVisualTops();
    RecyclerDataSource* dataSource = nullptr;

    Box* contentBox = nullptr;

    float itemHeight = 60.0f;
    size_t preFetchLine = 1;
    std::string sectionHeaderIdentifier = "section_header";

    // row -> cell（当前显示的，行号是展开后的平表下标）
    std::map<size_t, RecyclerCell*> activeCells;

    // reuseIdentifier -> 空闲 cell
    std::map<std::string, std::vector<RecyclerCell*>> cellPool;

    // 注册的构造函数
    std::map<std::string, std::function<RecyclerCell*()>> cellAllocations;

    // 展开后的行（表头 + 条目），行高不固定时靠它算偏移
    std::vector<Row> rows;
    float totalHeight = 0.0f;

    size_t createdCellCount = 0;
};

} // namespace bklv

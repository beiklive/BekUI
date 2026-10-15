#pragma once

// JSON 可描述的回收列表（`"type": "Recycler"`）—— **桥接层**。
//
// 分工（见 docs/json-syntax.md）：
//   JSON 负责：容器类型、数据源**名字**、item 的**一份**模板、列规则、loading/empty 槽位、onEnd 名字
//   C++  负责：数据源实现（总数/取第 N 项/绑定）、行为（点击/加载更多）、状态选择
//
// 分层：`views/recycler.*` 保持纯数据源接口（不认识 JSON）；
// 这里（widgets）做 JSON 属性表与模板实例化 —— 和 registerViewType 的既有惯例一致。

#include <functional>
#include <memory>
#include <string>

#include "bklv/core/attributes.hpp"
#include "bklv/views/recycler.hpp"

namespace bklv
{

class AppearanceView;

// item 模板的载体：RecyclerCell 是 Box 派生的，而外观要靠 AppearanceView 画，
// 所以用一个薄适配器在内部持有一个 AppearanceView（桥接层该干的事）。
class RecyclerItemCell : public RecyclerCell
{
  public:
    RecyclerItemCell();
    ~RecyclerItemCell() override;

    // 模板的两种写法：外观名（"itemAppearance"）或视图模板名（"itemTemplate"）
    void setAppearanceName(const std::string& name);
    void setViewTemplateName(const std::string& name);

    void setIndex(size_t value) override;
    // 分段列表（默认 1 段也算）走的是这个入口 —— 少了它绑定函数根本不会被调用（踩过）
    void setSectionIndex(size_t sectionValue, size_t indexValue) override;
    void prepareForReuse() override;

    // 数据绑定挂点：JSON 只写**名字**，实现由 C++ 注册（views/item_binder）
    void setItemBinderName(const std::string& name);
    void setBinder(std::function<void(View&, size_t)> binder) { this->binder = std::move(binder); }

    View* getContent() const { return (View*)this->content; }

  private:
    void ensureContent(const std::string& appearanceFallback);

    AppearanceView* content = nullptr;
    View* templateContent = nullptr;
    std::string appearanceName;
    std::string viewTemplateName;
    std::function<void(View&, size_t)> binder;
};

// 段头的载体：RecyclerSectionHeader 也是 Box 派生，外观同样要靠 AppearanceView 画
class RecyclerHeaderCell : public RecyclerSectionHeader
{
  public:
    RecyclerHeaderCell();
    ~RecyclerHeaderCell() override;
    void setAppearanceName(const std::string& name);
    void onSectionTitle(size_t section, const std::string& title) override;

  private:
    AppearanceView* content = nullptr;
};

class RecyclerView : public RecyclerFrame
{
  public:
    RecyclerView();

    const AttributeTable& attributes() const override;

    // —— JSON 属性落到这些 setter 上 ——
    void setDataSourceName(const std::string& name); // "data": 只写名字
    void setItemAppearance(const std::string& name); // "itemAppearance": 一份模板
    void setItemTemplate(const std::string& name);   // "itemTemplate": 视图模板
    void setColumns(int count);                      // "columns": 列数（1 = 普通列表）
    void setMinItemWidth(float width);               // "minItemWidth": 自动列数（0 = 用 columns）
    void setSlotLoading(const std::string& name);    // "slotLoading": 具名槽位（视图模板）
    void setSlotEmpty(const std::string& name);      // "slotEmpty"
    void setOnEndName(const std::string& name);      // "onEnd": 只存名字（行为在 ② 接）
    void setItemBinderName(const std::string& name); // "itemBinder": 只写名字
    // "sectionHeaderAppearance": 段头模板（一份外观）。数据源提供"有几段/段标题"，
    // JSON 只描述段头长什么样。
    void setSectionHeaderAppearance(const std::string& name);

    int getColumns() const { return this->columns; }
    float getMinItemWidth() const { return this->minItemWidth; }
    const std::string& getDataSourceName() const { return this->dataSourceName; }
    const std::string& getOnEndName() const { return this->onEndName; }
    const std::string& getItemBinderName() const { return this->itemBinderName; }

    // 建 item cell（registerCell 的工厂）；由属性表在设置模板后自动调用一次
    void rebuildItemCells();

  private:
    // JSON 路径下数据源是注册表的工厂 new 出来的 -> 由本视图持有并负责释放
    // （C++ 页面用的是 static 局部对象，那条路不经过这里）
    std::unique_ptr<RecyclerDataSource> ownedSource;
    std::string dataSourceName;
    std::string itemAppearance;
    std::string itemTemplate;
    std::string slotLoading;
    std::string slotEmpty;
    std::string onEndName;
    std::string itemBinderName;
    std::string sectionHeaderAppearance;
    int columns = 1;
    float minItemWidth = 0.0f;
};

// 注册 JSON 类型（由 widget_types 调用一次）
void registerRecyclerViewType();

} // namespace bklv

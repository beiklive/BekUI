#pragma once

// 用 JSON 模板驱动的列表项（对应 borealis 里"XML 定义 cell"的用法）。
//
// 为什么需要它：bklv 的 RecyclerCell 是靠 `setIndex(size_t)` 自己去取数据的
// （见 demo 的 ListCell），所以"JSON 定义的列表项"必须回答"第 37 行显示什么"。
// 这里的做法是让 JSON 只声明**字段名**，取值交给 app 传进来的 binder：
//
//   { "type": "JSONCell", ... }        列表项模板：resources/views/*.json
//   { "type": "Label", "bind": "title" }
//   { "type": "Image", "bind": "thumb" }
//
//   auto* cell = JSONRecyclerCell::create(tmpl, [](const std::string& field, size_t index) {
//       if (field == "title") return titles[index];
//       return std::string();
//   });
//
// 这样 JSON 不需要知道数据结构，RecyclerDataSource 也一行不用改。

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "bklv/core/json_view.hpp"
#include "bklv/views/recycler.hpp"

namespace bklv
{

class JSONRecyclerCell : public RecyclerCell
{
  public:
    std::string getClassString() const override { return "bklv::JSONRecyclerCell"; }

    // 给字段名与序号，返回该格子该字段要显示的值。
    using Binder = std::function<std::string(const std::string& field, size_t index)>;

    // 用模板建一个列表项。模板为空 / binder 为空 / 实例化失败都返回 nullptr
    // （RecyclerFrame 的分配回调对 nullptr 有保护）。
    static JSONRecyclerCell* create(const std::shared_ptr<const ViewTemplate>& tmpl, Binder binder);

    // 滚动到这一行时被调用：按绑定把 binder 给的值写进对应视图。
    void setIndex(size_t value) override;

    // 模板里的绑定项（测试与诊断用）。
    const std::vector<Binding>& getBindings() const { return this->bindings; }

    // 列表项要能被摇杆选中。焦点底色由 cell 管（模板根节点别设 backgroundColor，
    // 否则聚焦时会被这里覆盖）。
    bool isFocusable() const override { return true; }
    void onFocusGained() override { this->applyFocusLook(true); }
    void onFocusLost() override { this->applyFocusLook(false); }

  private:
    JSONRecyclerCell() = default;

    void applyFocusLook(bool focused);

    std::shared_ptr<const ViewTemplate> tmpl;
    std::vector<Binding> bindings;
    Binder binder;
    View* content = nullptr; // 模板根节点
};

} // namespace bklv

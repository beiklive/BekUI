#pragma once

// JSON 视图的轻量数据模型与响应式绑定。
//
// JSON 里仍写 "bind": "字段名" / "visibleIf": "字段名"；
// 宿主用 JsonData 设值，再由 JsonBindingRuntime::refresh() 把值写回视图属性/可见性。
// 不引入字符串模板或表达式，保持"结构归 JSON、数据与行为归 C++"的边界。

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "bklv/ui/view/json_view.hpp"

namespace bklv
{

class JsonData
{
  public:
    void set(const std::string& field, const std::string& value);
    void set(const std::string& field, const char* value);
    void set(const std::string& field, double value);
    void set(const std::string& field, bool value);
    void setList(const std::string& field, std::vector<std::string> values);
    bool hasList(const std::string& field) const;
    const std::vector<std::string>& getList(const std::string& field) const;
    void erase(const std::string& field);
    void clear();

    bool has(const std::string& field) const;
    std::string get(const std::string& field, const std::string& fallback = std::string()) const;
    bool isTruthy(const std::string& field) const;

  private:
    std::map<std::string, std::string> values;
    std::map<std::string, std::vector<std::string>> lists;
};

// 把 bindings / visibleIf 绑定到一棵已经实例化的视图树上。
// 生命周期：runtime 必须在视图树销毁前销毁（它只持有 View*，不拥有视图）。
class JsonBindingRuntime
{
  public:
    void bind(std::vector<Binding> bindings, std::vector<JsonCondition> conditions);
    void setData(std::shared_ptr<JsonData> data);
    std::shared_ptr<JsonData> getData() const { return this->data; }

    // 重新把数据写到视图上；数据为空时 visibleIf 一律 GONE。
    void refresh();

  private:
    std::vector<Binding> bindings;
    std::vector<JsonCondition> conditions;
    std::shared_ptr<JsonData> data;
};

// 便捷入口：从模板实例化并绑定数据（没有数据时只建树）。
View* createViewFromThemeWithData(const std::string& name, std::shared_ptr<JsonData> data,
    JsonBindingRuntime* runtime = nullptr);

} // namespace bklv

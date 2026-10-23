#include "bklv/core/json_data.hpp"

#include "bklv/core/attributes.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

void JsonData::set(const std::string& field, const std::string& value)
{
    this->values[field] = value;
}

void JsonData::set(const std::string& field, const char* value)
{
    this->values[field] = value != nullptr ? value : "";
}

void JsonData::set(const std::string& field, double value)
{
    this->values[field] = std::to_string(value);
}

void JsonData::set(const std::string& field, bool value)
{
    this->values[field] = value ? "true" : "false";
}

void JsonData::erase(const std::string& field)
{
    this->values.erase(field);
}

void JsonData::clear()
{
    this->values.clear();
}

bool JsonData::has(const std::string& field) const
{
    return this->values.find(field) != this->values.end();
}

std::string JsonData::get(const std::string& field, const std::string& fallback) const
{
    const auto it = this->values.find(field);
    return it != this->values.end() ? it->second : fallback;
}

bool JsonData::isTruthy(const std::string& field) const
{
    const auto it = this->values.find(field);
    if (it == this->values.end())
        return false;
    const std::string& value = it->second;
    return !value.empty() && value != "false" && value != "0";
}

void JsonBindingRuntime::bind(std::vector<Binding> bindingsIn,
    std::vector<JsonCondition> conditionsIn)
{
    this->bindings   = std::move(bindingsIn);
    this->conditions = std::move(conditionsIn);
}

void JsonBindingRuntime::setData(std::shared_ptr<JsonData> dataIn)
{
    this->data = std::move(dataIn);
}

void JsonBindingRuntime::refresh()
{
    if (!this->data)
        return;

    for (const Binding& binding : this->bindings)
    {
        if (binding.view == nullptr)
            continue;
        const std::string value = this->data->get(binding.field);
        std::string error;
        const ApplyResult result
            = binding.view->applyAttribute(binding.attribute, Value(value), error);
        if (result != ApplyResult::APPLIED)
            BKLV_LOG_WARNING("JSON 绑定 %s -> %s 失败：%s", binding.field.c_str(),
                binding.attribute.c_str(), error.c_str());
    }

    for (const JsonCondition& condition : this->conditions)
    {
        if (condition.view == nullptr)
            continue;

        bool visible = false;
        if (this->data->has(condition.field))
        {
            if (condition.hasEquals)
                visible = this->data->get(condition.field) == condition.equals;
            else
                visible = this->data->isTruthy(condition.field);
        }
        condition.view->setVisibility(visible ? Visibility::VISIBLE : Visibility::GONE);
    }
}

View* createViewFromThemeWithData(const std::string& name, std::shared_ptr<JsonData> data,
    JsonBindingRuntime* runtime)
{
    std::shared_ptr<const ViewTemplate> tmpl = loadViewTemplateFromTheme(name);
    if (!tmpl)
        return nullptr;

    std::vector<Binding> bindings;
    std::vector<JsonCondition> conditions;
    View* view = tmpl->instantiate(&bindings, &conditions);
    if (view == nullptr)
        return nullptr;

    if (runtime != nullptr)
    {
        runtime->bind(std::move(bindings), std::move(conditions));
        runtime->setData(std::move(data));
        runtime->refresh();
    }
    return view;
}

} // namespace bklv

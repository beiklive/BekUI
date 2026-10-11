#include "bklv/views/json_cell.hpp"

#include <memory>
#include <utility>

#include "bklv/core/attributes.hpp"
#include "bklv/core/logger.hpp"

namespace bklv
{

JSONRecyclerCell* JSONRecyclerCell::create(const std::shared_ptr<const ViewTemplate>& tmpl, Binder binder)
{
    if (tmpl == nullptr)
    {
        BKLV_LOG_ERROR("JSONRecyclerCell::create 收到空模板");
        return nullptr;
    }
    if (!binder)
    {
        BKLV_LOG_ERROR("JSONRecyclerCell::create 收到空 binder");
        return nullptr;
    }

    std::vector<Binding> bindings;
    View* content = tmpl->instantiate(&bindings);
    if (content == nullptr)
    {
        BKLV_LOG_ERROR("JSON 列表项模板实例化失败：" + tmpl->getOrigin());
        return nullptr;
    }

    // unique_ptr 持有：addView 之后所有权才移交，中途抛出不会漏
    std::unique_ptr<JSONRecyclerCell> cell(new JSONRecyclerCell());
    cell->tmpl     = tmpl;
    cell->bindings = std::move(bindings);
    cell->binder   = std::move(binder);
    cell->content  = content;
    cell->addView(content);

    return cell.release();
}

void JSONRecyclerCell::applyFocusLook(bool focused)
{
    if (this->content == nullptr)
        return;

    // 走属性系统设底色，和 JSON 里写 backgroundColor 是同一条路
    std::string error;
    this->content->applyAttribute("backgroundColor",
        Value(focused ? "#30568c" : "#262a32"), error);
}

void JSONRecyclerCell::setIndex(size_t value)
{
    RecyclerCell::setIndex(value);

    if (!this->binder)
        return;

    for (const Binding& binding : this->bindings)
    {
        if (binding.view == nullptr)
            continue;

        const std::string raw = this->binder(binding.field, value);

        std::string error;
        const ApplyResult result = binding.view->applyAttribute(binding.attribute, Value(raw), error);
        if (result != ApplyResult::Applied)
        {
            // 取不到值或类型不符都只警告：一个格子显示不出来不该让整页崩掉
            BKLV_LOG_WARNING("JSON 列表项绑定失败（字段 \"%s\" -> 属性 \"%s\"）：%s",
                binding.field.c_str(), binding.attribute.c_str(),
                result == ApplyResult::NotHandled ? "属性不认识" : error.c_str());
        }
    }
}

} // namespace bklv

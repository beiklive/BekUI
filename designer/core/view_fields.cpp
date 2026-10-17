#include "designer/core/view_fields.hpp"

#include <memory>

#include "bklv/core/json_view.hpp"
#include "bklv/core/view.hpp"

namespace designer
{

std::vector<ViewFieldDesc> viewFields(const std::string& type)
{
    std::vector<ViewFieldDesc> fields;
    if (type.empty())
        return fields;

    // 最小节点：只写 type，其余属性都不给（实例化不需要数据）
    const std::string json = "{\"type\":\"" + type + "\"}";
    const auto        tmpl = bklv::ViewTemplate::fromString(json, "<designer-field-scan>");
    if (tmpl == nullptr)
        return fields;

    std::unique_ptr<bklv::View> probe(tmpl->instantiate());
    if (probe == nullptr)
        return fields;

    for (const bklv::AttributeDef& def : probe->attributes().all())
        fields.push_back({ def.name, def.kind, def.allowed != nullptr ? def.allowed : "", false });

    // 公共属性：子类已经声明过的同名键以子类为准（不重复列出）
    for (const bklv::AttributeDef& def : bklv::commonAttributesTable().all())
    {
        bool exists = false;
        for (const ViewFieldDesc& field : fields)
            exists = exists || field.key == def.name;
        if (!exists)
            fields.push_back({ def.name, def.kind, def.allowed != nullptr ? def.allowed : "", true });
    }
    return fields;
}

} // namespace designer

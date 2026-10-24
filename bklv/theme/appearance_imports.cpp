#include "bklv/json/json_vars.hpp"
#include "bklv/theme/appearance_imports.hpp"

#include "bklv/core/logger.hpp"

#include <algorithm>

namespace bklv
{
namespace compile
{

Json mergeJson(const Json& base, const Json& over)
{
    if (!base.is_object() || !over.is_object())
        return over;

    Json result = base;
    for (auto it = over.begin(); it != over.end(); ++it)
    {
        const std::string& key = it.key();
        if (key == "layers" && it.value().is_array() && result.contains("layers")
            && result["layers"].is_array())
        {
            // 按 id 合并层：相同 id 的合并字段，新 id 追加
            Json merged = Json::array();
            Json consumed = Json::array();

            // 层 import 指令没有 id，按"导入目标"当键合并：
            //   父级 { "import": "base/layer/shadow" }
            //   子级 { "import": "base/layer/shadow", "cutout": true }
            // 应该合成一条（子级覆盖参数），而不是两条展开后 id 重复。
            auto importTarget = [](const Json& layer) -> std::string {
                if (layer.is_object() && !layer.contains("op") && layer.contains("import")
                    && layer["import"].is_string())
                    return layer["import"].get<std::string>();
                return std::string();
            };

            for (const Json& baseLayer : result["layers"])
            {
                Json layer = baseLayer;
                if (baseLayer.is_object() && baseLayer.contains("id"))
                {
                    for (const Json& overLayer : it.value())
                    {
                        if (overLayer.is_object() && overLayer.contains("id")
                            && overLayer["id"] == baseLayer["id"])
                        {
                            layer = mergeJson(baseLayer, overLayer);
                            consumed.push_back(overLayer["id"]);
                        }
                    }
                }
                else
                {
                    const std::string target = importTarget(baseLayer);
                    if (!target.empty())
                    {
                        for (const Json& overLayer : it.value())
                        {
                            if (importTarget(overLayer) == target)
                            {
                                layer = mergeJson(baseLayer, overLayer);
                                consumed.push_back("@" + target);
                            }
                        }
                    }
                }
                merged.push_back(layer);
            }

            for (const Json& overLayer : it.value())
            {
                bool already = false;
                if (overLayer.is_object() && overLayer.contains("id"))
                    for (const Json& id : consumed)
                        if (id == overLayer["id"])
                            already = true;
                const std::string target = importTarget(overLayer);
                if (!target.empty())
                    for (const Json& key : consumed)
                        if (key == "@" + target)
                            already = true;
                if (!already)
                    merged.push_back(overLayer);
            }

            result["layers"] = merged;
            continue;
        }

        if (result.contains(key) && result[key].is_object() && it.value().is_object())
            result[key] = mergeJson(result[key], it.value());
        else
            result[key] = it.value();
    }
    return result;
}

bool resolveImports(const Json& doc, const ThemeData& theme, std::vector<std::string>& visited,
    Json& out, std::string& error)
{
    if (!doc.contains("import"))
    {
        out = doc;
        return true;
    }

    std::vector<std::string> names;
    if (doc["import"].is_string())
        names.push_back(doc["import"].get<std::string>());
    else if (doc["import"].is_array())
    {
        for (const Json& entry : doc["import"])
            if (entry.is_string())
                names.push_back(entry.get<std::string>());
    }
    else
    {
        error = "import 必须是字符串或字符串数组";
        return false;
    }

    if (visited.size() >= 8)
    {
        error = "import 链太深（超过 8 层）：" + names.front();
        return false;
    }

    Json base = Json::object();
    for (const std::string& name : names)
    {
        if (std::find(visited.begin(), visited.end(), name) != visited.end())
        {
            error = "import 出现循环引用：" + name;
            return false;
        }

        const std::string* importedJson = theme.widgetAppearanceJson(name);
        if (importedJson == nullptr)
        {
            error = "import 的外观不存在：\"" + name + "\"（检查名字与 theme.json 的 files[]）";
            return false;
        }

        Json importedDoc;
        try
        {
            importedDoc = Json::parse(*importedJson);
        }
        catch (const Json::parse_error& e)
        {
            error = "import 的 " + name + " 语法错误：" + e.what();
            return false;
        }

        // 被导入的文件可能有自己的 vars：先按**它自己的**变量展开，再合并（$ 不跨文件）
        if (!expandJsonVars(importedDoc, error))
        {
            error = "import 的 " + name + " 的变量块有问题：" + error;
            return false;
        }

        visited.push_back(name);
        Json resolved;
        const bool ok = resolveImports(importedDoc, theme, visited, resolved, error);
        visited.pop_back();
        if (!ok)
        {
            error = name + " -> " + error;
            return false;
        }

        base = mergeJson(base, resolved);
    }

    Json self = doc;
    self.erase("import");
    out = mergeJson(base, self);
    return true;
}

} // namespace compile
} // namespace bklv

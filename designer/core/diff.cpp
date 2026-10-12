#include "designer/core/diff.hpp"

#include <cstdlib>

#include <map>

namespace designer
{

std::string compactValue(const nlohmann::ordered_json& value)
{
    if (value.is_string())
        return value.get<std::string>();
    if (value.is_boolean())
        return value.get<bool>() ? "true" : "false";
    if (value.is_null())
        return "null";
    return value.dump();
}

namespace
{

void flatten(const nlohmann::ordered_json& node, const std::string& prefix,
    std::map<std::string, std::string>& out)
{
    if (node.is_object())
    {
        if (node.empty())
        {
            out[prefix] = "{}";
            return;
        }
        for (auto it = node.begin(); it != node.end(); ++it)
        {
            const std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();
            flatten(it.value(), key, out);
        }
        return;
    }
    if (node.is_array())
    {
        if (node.empty())
        {
            out[prefix] = "[]";
            return;
        }
        for (size_t i = 0; i < node.size(); i++)
            flatten(node[i], prefix + "[" + std::to_string(i) + "]", out);
        return;
    }
    out[prefix] = compactValue(node);
}

} // namespace

std::vector<JsonChange> diffJson(const nlohmann::ordered_json& before,
    const nlohmann::ordered_json& after)
{
    std::map<std::string, std::string> oldValues;
    std::map<std::string, std::string> newValues;
    flatten(before, "", oldValues);
    flatten(after, "", newValues);

    std::vector<JsonChange> changes;

    for (const auto& entry : newValues)
    {
        const auto old = oldValues.find(entry.first);
        if (old == oldValues.end())
            changes.push_back({ entry.first, std::string(), entry.second });
        else if (old->second != entry.second)
            changes.push_back({ entry.first, old->second, entry.second });
    }
    for (const auto& entry : oldValues)
        if (newValues.find(entry.first) == newValues.end())
            changes.push_back({ entry.first, entry.second, std::string() });

    return changes;
}

std::set<std::string> changedPaths(const nlohmann::ordered_json& before,
    const nlohmann::ordered_json& after)
{
    std::set<std::string> paths;
    for (const JsonChange& change : diffJson(before, after))
        paths.insert(change.path);
    return paths;
}

PathTarget parseChangePath(const std::string& path)
{
    PathTarget target;
    if (path.empty())
        return target;

    if (path.rfind("box.", 0) == 0)
    {
        target.kind = PathTarget::Kind::BOX_FIELD;
        target.key  = path.substr(4);
        return target;
    }
    if (path == "box")
    {
        target.kind = PathTarget::Kind::BOX;
        return target;
    }

    if (path.rfind("layers[", 0) == 0)
    {
        const size_t close = path.find(']');
        if (close == std::string::npos)
        {
            target.kind = PathTarget::Kind::OTHER;
            return target;
        }

        target.layerIndex = std::atoi(path.substr(7, close - 7).c_str());
        if (close + 1 == path.size())
        {
            target.kind = PathTarget::Kind::LAYER;
            return target;
        }
        if (path[close + 1] == '.' && close + 2 < path.size())
        {
            target.kind = PathTarget::Kind::LAYER_FIELD;
            target.key  = path.substr(close + 2);
            return target;
        }
    }

    target.kind = PathTarget::Kind::OTHER;
    return target;
}

std::string changeSummary(const JsonChange& change)
{
    if (change.before.empty())
        return change.path + ": (新增) " + change.after;
    if (change.after.empty())
        return change.path + ": " + change.before + " (删除)";
    return change.path + ": " + change.before + " → " + change.after;
}

} // namespace designer

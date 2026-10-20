#include "bklv/compile/appearance_children.hpp"

#include "bklv/compile/appearance_box.hpp" // box 段解析（与外观主 box 段共用）
#include "bklv/compile/json_util.hpp"

#include "bklv/core/logger.hpp"


namespace bklv
{
namespace compile
{

namespace
{

// 视图 JSON 的深度上限是 32；控件组合嵌套不该那么深，这里更保守（§12.5 规则 5）
constexpr int MAX_CHILD_DEPTH = 16;

bool parseOneChild(const Json& node, const ThemeData& theme, AppearanceChild& out,
    std::string& error, const std::string& where, int depth)
{
    if (depth > MAX_CHILD_DEPTH)
    {
        error = where + "：children 嵌套超过 " + std::to_string(MAX_CHILD_DEPTH) + " 层";
        return false;
    }
    if (!node.is_object())
    {
        error = where + "：children 的每一项都必须是对象";
        return false;
    }

    const bool hasAppearance = node.contains("appearance");
    const bool hasRef        = node.contains("ref");
    const bool hasBox        = node.contains("box");

    if (hasAppearance && hasRef)
    {
        error = where + "：不能同时写 appearance 与 ref（二选一）";
        return false;
    }
    if (!hasAppearance && !hasRef && !hasBox)
    {
        error = where + "：至少要有 appearance / ref / box 之一，否则不知道该建什么";
        return false;
    }

    if (hasAppearance)
    {
        out.kind = AppearanceChild::Kind::APPEARANCE;
        out.name = node["appearance"].get<std::string>();
    }
    else if (hasRef)
    {
        out.kind = AppearanceChild::Kind::VIEW_TEMPLATE;
        out.name = node["ref"].get<std::string>();
    }
    else
    {
        out.kind = AppearanceChild::Kind::BOX;
    }

    if (node.contains("id") && node["id"].is_string())
        out.id = node["id"].get<std::string>();

    if (hasBox && !parseBoxNode(node["box"], theme, out.box, error, where + ".box"))
        return false;

    for (const char* slot : { "text", "icon", "info" })
    {
        if (node.contains(slot))
        {
            const std::string value = node[slot].get<std::string>();
            if (std::string(slot) == "text")
                out.text = value;
            else if (std::string(slot) == "icon")
                out.icon = value;
            else
                out.info = value;
        }
    }

    if (node.contains("children"))
    {
        if (out.kind == AppearanceChild::Kind::VIEW_TEMPLATE)
        {
            error = where + "：ref 引用的是视图模板，它的 children 写在模板里，这里不能再写";
            return false;
        }
        if (!node["children"].is_array())
        {
            error = where + ".children 必须是数组";
            return false;
        }
        int index = 0;
        for (const Json& child : node["children"])
        {
            AppearanceChild parsed;
            if (!parseOneChild(child, theme, parsed, error, where + ".children[" + std::to_string(index) + "]", depth + 1))
                return false;
            out.children.push_back(std::move(parsed));
            index++;
        }
    }

    return true;
}

} // namespace

bool parseChildren(const Json& doc, const ThemeData& theme, std::vector<AppearanceChild>& out,
    std::string& error)
{
    if (!doc.contains("children"))
        return true;
    if (!doc["children"].is_array())
    {
        error = "children 必须是数组";
        return false;
    }

    int index = 0;
    for (const Json& node : doc["children"])
    {
        AppearanceChild child;
        if (!parseOneChild(node, theme, child, error, "children[" + std::to_string(index) + "]", 1))
            return false;
        out.push_back(std::move(child));
        index++;
    }
    return true;
}

} // namespace compile
} // namespace bklv

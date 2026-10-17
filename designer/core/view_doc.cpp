#include "designer/core/view_doc.hpp"

namespace designer
{

namespace
{

std::string joinPointer(const std::string& base, const std::string& child)
{
    return base + "/children/" + child;
}

void collect(const Json& node, const std::string& pointer, int depth, int maxDepth,
    std::vector<ViewNodeInfo>& out)
{
    if (!node.is_object())
        return;

    ViewNodeInfo info;
    info.pointer = pointer;
    info.depth   = depth;
    if (node.contains("type") && node["type"].is_string())
        info.type = node["type"].get<std::string>();
    out.push_back(std::move(info));

    if (depth >= maxDepth || !node.contains("children") || !node["children"].is_array())
        return;
    const Json& children = node["children"];
    for (size_t i = 0; i < children.size(); i++)
        collect(children[i], joinPointer(pointer, std::to_string(i)), depth + 1, maxDepth, out);
}

} // namespace

std::vector<ViewNodeInfo> ViewDoc::listNodes(int maxDepth) const
{
    std::vector<ViewNodeInfo> out;
    collect(this->root(), std::string(), 0, maxDepth, out);
    return out;
}

bool ViewDoc::setAttribute(const std::string& pointer, const std::string& key, Json value,
    std::string& error)
{
    const std::string target = pointer.empty() ? "/" + key : pointer + "/" + key;
    return this->set(target, std::move(value), error);
}

bool ViewDoc::removeAttribute(const std::string& pointer, const std::string& key,
    std::string& error)
{
    const std::string target = pointer.empty() ? "/" + key : pointer + "/" + key;
    return this->remove(target, error);
}

std::string ViewDoc::attributeText(const std::string& pointer, const std::string& key) const
{
    const std::string target = pointer.empty() ? "/" + key : pointer + "/" + key;
    const Json*       value  = this->get(target);
    if (value == nullptr)
        return {};
    return value->is_string() ? value->get<std::string>() : value->dump();
}

} // namespace designer

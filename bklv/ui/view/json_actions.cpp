#include "bklv/ui/view/json_actions.hpp"

#include <unordered_map>

#include "bklv/foundation/core/logger.hpp"

namespace bklv
{

namespace
{
std::unordered_map<std::string, JsonAction>& registry()
{
    static std::unordered_map<std::string, JsonAction> actions;
    return actions;
}
} // namespace

bool registerJsonAction(const std::string& name, JsonAction action, std::string& error)
{
    if (name.empty() || !action)
    {
        error = "registerJsonAction 需要非空名字与有效回调";
        return false;
    }
    auto& actions = registry();
    if (actions.find(name) != actions.end())
    {
        error = "JSON 行为名重复：\"" + name + "\"";
        return false;
    }
    actions.emplace(name, std::move(action));
    return true;
}

void registerJsonAction(const std::string& name, JsonAction action)
{
    std::string error;
    if (!registerJsonAction(name, std::move(action), error))
        BKLV_LOG_WARNING("%s", error.c_str());
}

bool hasJsonAction(const std::string& name)
{
    return registry().find(name) != registry().end();
}

bool invokeJsonAction(const std::string& name, View& view, std::string& error)
{
    auto it = registry().find(name);
    if (it == registry().end())
    {
        error = "找不到 JSON 行为：\"" + name + "\"";
        return false;
    }
    it->second(view);
    return true;
}

void clearJsonActionsForTest()
{
    registry().clear();
}

} // namespace bklv

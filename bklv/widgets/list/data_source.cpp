#include "bklv/widgets/list/data_source.hpp"

#include <algorithm>
#include <utility>

#include "bklv/core/globals.hpp"
#include "bklv/core/logger.hpp"

namespace bklv
{
namespace data
{

namespace
{

struct Entry
{
    std::string name;
    SourceFactory factory;
};

std::vector<Entry>& registry()
{
    static std::vector<Entry> entries;
    return entries;
}

} // namespace

bool registerSource(const std::string& name, SourceFactory factory, std::string& error)
{
    if (name.empty())
    {
        error = "数据源名字不能为空";
        return false;
    }
    if (!factory)
    {
        error = "数据源 \"" + name + "\" 的工厂为空";
        return false;
    }
    if (hasSource(name))
    {
        error = "数据源 \"" + name + "\" 重复注册";
        return false;
    }

    registry().push_back({ name, std::move(factory) });

    // 注册表的状态是自己的静态表，按 core/globals.hpp 的约定登记重置钩子
    // （名字去重，随手调没有副作用）
    globals::getGlobalStore().addResetHook("dataSources", &clearSourcesForTest);
    return true;
}

RecyclerDataSource* createSource(const std::string& name)
{
    for (const Entry& entry : registry())
    {
        if (entry.name == name)
        {
            RecyclerDataSource* source = entry.factory();
            if (source == nullptr)
                BKLV_LOG_WARNING("数据源 \"%s\" 的工厂返回了空指针", name.c_str());
            return source;
        }
    }

    BKLV_LOG_WARNING("没有注册过数据源 \"%s\"（JSON 里写 \"data\": \"%s\" 需要先在 C++ 里注册）",
        name.c_str(), name.c_str());
    return nullptr;
}

bool hasSource(const std::string& name)
{
    const std::vector<Entry>& entries = registry();
    return std::any_of(entries.begin(), entries.end(),
        [&name](const Entry& entry) { return entry.name == name; });
}

std::vector<std::string> sourceNames()
{
    std::vector<std::string> names;
    names.reserve(registry().size());
    for (const Entry& entry : registry())
        names.push_back(entry.name);
    return names;
}

void clearSourcesForTest()
{
    registry().clear();
}

} // namespace data
} // namespace bklv

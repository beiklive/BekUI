#include "bklv/views/item_binder.hpp"

#include <algorithm>
#include <utility>

#include "bklv/core/logger.hpp"

namespace bklv
{
namespace binding
{

namespace
{

struct Entry
{
    std::string name;
    ItemBinder binder;
};

std::vector<Entry>& registry()
{
    static std::vector<Entry> entries;
    return entries;
}

} // namespace

bool registerItemBinder(const std::string& name, ItemBinder binder, std::string& error)
{
    if (name.empty())
    {
        error = "item 绑定函数的名字不能为空";
        return false;
    }
    if (!binder)
    {
        error = "item 绑定函数 \"" + name + "\" 为空";
        return false;
    }
    if (hasItemBinder(name))
    {
        error = "item 绑定函数 \"" + name + "\" 重复注册";
        return false;
    }

    registry().push_back({ name, std::move(binder) });
    return true;
}

ItemBinder findItemBinder(const std::string& name)
{
    for (const Entry& entry : registry())
        if (entry.name == name)
            return entry.binder;

    BKLV_LOG_WARNING("没有注册过 item 绑定函数 \"%s\"（JSON 里写 itemBinder 需要先在 C++ 里注册）",
        name.c_str());
    return ItemBinder();
}

bool hasItemBinder(const std::string& name)
{
    const std::vector<Entry>& entries = registry();
    return std::any_of(entries.begin(), entries.end(),
        [&name](const Entry& entry) { return entry.name == name; });
}

std::vector<std::string> itemBinderNames()
{
    std::vector<std::string> names;
    names.reserve(registry().size());
    for (const Entry& entry : registry())
        names.push_back(entry.name);
    return names;
}

void clearItemBindersForTest()
{
    registry().clear();
}

} // namespace binding
} // namespace bklv

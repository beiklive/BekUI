#include "designer/core/json_cache.hpp"

#include "designer/core/theme_files.hpp"

namespace designer
{

const JsonCache::Json* JsonCache::get(const std::string& path)
{
    const auto found = this->entries.find(path);
    if (found != this->entries.end())
        return found->second.is_discarded() ? nullptr : &found->second;

    std::string text;
    std::string error;
    Json parsed;
    if (!readTextFile(path, text, error))
        parsed = Json::parse("", nullptr, false); // 失败也缓存（discarded）
    else
        parsed = Json::parse(text, nullptr, false);

    const auto inserted = this->entries.emplace(path, std::move(parsed));
    return inserted.first->second.is_discarded() ? nullptr : &inserted.first->second;
}

} // namespace designer

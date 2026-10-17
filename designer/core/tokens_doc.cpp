#include "designer/core/tokens_doc.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace designer
{

namespace fs = std::filesystem;

namespace
{

void flatten(const Json& node, const std::string& prefix, std::vector<TokenEntry>& out)
{
    for (auto it = node.begin(); it != node.end(); ++it)
    {
        const std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();
        if (it->is_object())
            flatten(*it, key, out);
        else if (it->is_string())
            out.push_back({ key, it->get<std::string>() });
        else if (it->is_number())
            out.push_back({ key, it->dump() });
        else if (it->is_boolean())
            out.push_back({ key, *it ? "true" : "false" });
    }
}

std::string pointerForKey(const std::string& key)
{
    std::string pointer;
    size_t      start = 0;
    while (start <= key.size())
    {
        const size_t dot = key.find('.', start);
        const size_t end = (dot == std::string::npos) ? key.size() : dot;
        pointer += "/" + key.substr(start, end - start);
        if (dot == std::string::npos)
            break;
        start = dot + 1;
    }
    return pointer;
}

void collectRefs(const Json& node, const std::string& pointer, const std::string& needle,
    std::vector<TokenRef>& out, const std::string& file)
{
    if (node.is_string())
    {
        const std::string text = node.get<std::string>();
        if (text.find(needle) != std::string::npos)
            out.push_back({ file, pointer, text });
        return;
    }
    if (node.is_object())
    {
        for (auto it = node.begin(); it != node.end(); ++it)
            collectRefs(*it, pointer + "/" + it.key(), needle, out, file);
        return;
    }
    if (node.is_array())
        for (size_t i = 0; i < node.size(); i++)
            collectRefs(node[i], pointer + "/" + std::to_string(i), needle, out, file);
}

} // namespace

std::vector<TokenEntry> TokensDoc::list() const
{
    std::vector<TokenEntry> out;
    flatten(this->root(), std::string(), out);
    return out;
}

const TokenEntry* TokensDoc::find(const std::string& key) const
{
    static thread_local TokenEntry found;
    for (const TokenEntry& entry : this->list())
    {
        if (entry.key == key)
        {
            found = entry;
            return &found;
        }
    }
    return nullptr;
}

bool TokensDoc::write(const std::string& key, const std::string& value, std::string& error)
{
    // 数字/布尔按字面量写回，其余按字符串（颜色、字体名）
    Json parsed;
    try
    {
        parsed = Json::parse(value, nullptr, true, true);
    }
    catch (...)
    {
        parsed = Json(value);
    }
    return this->set(pointerForKey(key), std::move(parsed), error);
}

bool TokensDoc::removeKey(const std::string& key, std::string& error)
{
    return this->remove(pointerForKey(key), error);
}

std::vector<TokenRef> scanThemeReferences(
    const std::string& themeDir, const std::string& kind, const std::string& key)
{
    std::vector<TokenRef> refs;
    const std::string     needle = "@theme/" + kind + "." + key;
    std::error_code       ec;

    for (const char* dir : { "base", "widget" })
    {
        const fs::path path = fs::path(themeDir) / dir;
        if (!fs::exists(path, ec))
            continue;
        for (const auto& entry : fs::directory_iterator(path, ec))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;
            std::ifstream in(entry.path(), std::ios::binary);
            if (!in)
                continue;
            std::ostringstream buffer;
            buffer << in.rdbuf();
            Json document;
            try
            {
                document = Json::parse(buffer.str(), nullptr, true, true);
            }
            catch (...)
            {
                continue;
            }
            const std::string rel = std::string(dir) + "/" + entry.path().stem().string();
            collectRefs(document, std::string(), needle, refs, rel);
        }
    }
    return refs;
}

} // namespace designer

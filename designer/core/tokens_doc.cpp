#include "designer/core/tokens_doc.hpp"

#include <cctype>
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
        if (!it.key().empty() && it.key()[0] == '$')
            continue;
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

bool validTokenKey(const std::string& key)
{
    if (key.empty())
        return false;
    size_t start = 0;
    while (start < key.size())
    {
        const size_t end = key.find('.', start);
        const size_t length = (end == std::string::npos ? key.size() : end) - start;
        if (length == 0 || key[start] == '$')
            return false;
        for (size_t i = start; i < start + length; ++i)
        {
            const unsigned char c = static_cast<unsigned char>(key[i]);
            if (!std::isalnum(c) && c != '_' && c != '-')
                return false;
        }
        if (end == std::string::npos)
            return true;
        start = end + 1;
    }
    return false;
}

void collectRefs(const Json& node, const std::string& pointer, const std::string& needle,
    std::vector<TokenRef>& out, const std::string& file)
{
    if (node.is_string())
    {
        const std::string text = node.get<std::string>();
        if (text == needle)
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
    if (!validTokenKey(key))
    {
        error = "变量名只能使用字母、数字、_、- 和点分层级，且不能以 $ 开头";
        return false;
    }
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
    TokensDoc candidate = *this;
    const std::string pointer = pointerForKey(key);
    const Pointer path = Pointer::parse(pointer);
    Pointer parent;
    for (size_t i = 0; i + 1 < path.tokens.size(); ++i)
    {
        parent.tokens.assign(path.tokens.begin(), path.tokens.begin() + (std::ptrdiff_t)(i + 1));
        if (candidate.get(parent) == nullptr && !candidate.set(parent, Json::object(), error))
            return false;
    }
    if (!candidate.set(pointer, std::move(parsed), error))
        return false;
    *this = std::move(candidate);
    return true;
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

    for (const char* dir : { "base", "widget", "view" })
    {
        const fs::path path = fs::path(themeDir) / dir;
        if (!fs::exists(path, ec))
            continue;
        for (const auto& entry : fs::recursive_directory_iterator(path, ec))
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
            const std::string rel = fs::relative(entry.path(), themeDir, ec).replace_extension().generic_string();
            collectRefs(document, std::string(), needle, refs, rel);
        }
    }
    return refs;
}

} // namespace designer

#include "designer/core/json_patch.hpp"

#include <cctype>
#include <vector>

#include <nlohmann/json.hpp>

using Json = nlohmann::ordered_json;

namespace designer
{

namespace
{

struct Member
{
    std::string key;
    size_t valueBegin = 0; // 值的第一个字节
    size_t valueEnd   = 0; // 值的最后一个字节 + 1
};

void skipWhitespace(const std::string& text, size_t& pos)
{
    while (pos < text.size() && std::isspace((unsigned char)text[pos]))
        pos++;
}

// 从 pos（指向第一个引号）解析一个 JSON 字符串，返回内容并推进 pos
bool parseString(const std::string& text, size_t& pos, std::string& out)
{
    if (pos >= text.size() || text[pos] != '"')
        return false;
    pos++;
    out.clear();
    while (pos < text.size())
    {
        const char c = text[pos];
        if (c == '\\')
        {
            if (pos + 1 >= text.size())
                return false;
            const char escaped = text[pos + 1];
            switch (escaped)
            {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'u':
                {
                    if (pos + 5 >= text.size())
                        return false;
                    out.push_back('\\');
                    out.push_back('u');
                    for (int i = 2; i < 6; i++)
                        out.push_back(text[pos + i]);
                    pos += 4;
                    break;
                }
                default: out.push_back(escaped); break;
            }
            pos += 2;
            continue;
        }
        if (c == '"')
        {
            pos++;
            return true;
        }
        out.push_back(c);
        pos++;
    }
    return false;
}

// 跳过任意 JSON 值（对象/数组要配对，字符串要跳过转义）
bool skipValue(const std::string& text, size_t& pos)
{
    skipWhitespace(text, pos);
    if (pos >= text.size())
        return false;

    const char c = text[pos];
    if (c == '"')
    {
        std::string ignored;
        return parseString(text, pos, ignored);
    }
    if (c == '{' || c == '[')
    {
        const char open  = c;
        const char close = (c == '{') ? '}' : ']';
        int depth        = 0;
        while (pos < text.size())
        {
            const char cur = text[pos];
            if (cur == '"')
            {
                std::string ignored;
                if (!parseString(text, pos, ignored))
                    return false;
                continue;
            }
            if (cur == open)
                depth++;
            else if (cur == close)
            {
                depth--;
                pos++;
                if (depth == 0)
                    return true;
                continue;
            }
            pos++;
        }
        return false;
    }

    // 数字 / true / false / null
    while (pos < text.size() && text[pos] != ',' && text[pos] != '}' && text[pos] != ']')
        pos++;
    return true;
}

// 扫出顶层成员（只处理最外层对象）
bool scanTopLevel(const std::string& text, std::vector<Member>& out, std::string& error)
{
    size_t pos = 0;
    skipWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != '{')
    {
        error = "不是以对象开头";
        return false;
    }
    pos++;
    skipWhitespace(text, pos);
    if (pos < text.size() && text[pos] == '}')
        return true;

    while (pos < text.size())
    {
        skipWhitespace(text, pos);
        Member member;
        if (!parseString(text, pos, member.key))
        {
            error = "顶层键解析失败";
            return false;
        }
        skipWhitespace(text, pos);
        if (pos >= text.size() || text[pos] != ':')
        {
            error = "缺少冒号";
            return false;
        }
        pos++;
        skipWhitespace(text, pos);
        member.valueBegin = pos;
        if (!skipValue(text, pos))
        {
            error = "值解析失败（" + member.key + "）";
            return false;
        }
        member.valueEnd = pos;
        out.push_back(member);

        skipWhitespace(text, pos);
        if (pos < text.size() && text[pos] == ',')
        {
            pos++;
            continue;
        }
        if (pos < text.size() && text[pos] == '}')
            return true;
        error = "顶层成员之间缺少逗号";
        return false;
    }
    return true;
}

} // namespace

bool patchTopLevel(const std::string& originalText, const std::string& updatedText,
    std::string& out, std::string& error)
{
    std::vector<Member> original;
    std::vector<Member> updated;
    if (!scanTopLevel(originalText, original, error) || !scanTopLevel(updatedText, updated, error))
        return false;

    if (original.size() != updated.size())
    {
        error = "顶层键数量变了";
        return false;
    }
    for (size_t i = 0; i < original.size(); i++)
        if (original[i].key != updated[i].key)
        {
            error = "顶层键顺序或名字变了（" + original[i].key + " → " + updated[i].key + "）";
            return false;
        }

    std::string result;
    result.reserve(originalText.size());
    size_t copied = 0;
    for (size_t i = 0; i < original.size(); i++)
    {
        const std::string oldValue
            = originalText.substr(original[i].valueBegin, original[i].valueEnd - original[i].valueBegin);
        const std::string newValue
            = updatedText.substr(updated[i].valueBegin, updated[i].valueEnd - updated[i].valueBegin);

        // 值语义相同（只是排版不同）就不用动
        const Json oldJson = Json::parse(oldValue, nullptr, false);
        const Json newJson = Json::parse(newValue, nullptr, false);
        if (!oldJson.is_discarded() && !newJson.is_discarded() && oldJson == newJson)
            continue;

        result.append(originalText, copied, original[i].valueBegin - copied);
        result.append(newValue);
        copied = original[i].valueEnd;
    }
    result.append(originalText, copied, originalText.size() - copied);
    out = result;
    return true;
}

} // namespace designer

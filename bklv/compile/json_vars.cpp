#include "bklv/compile/json_vars.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace bklv
{
namespace compile
{

namespace
{

// 链式引用的深度上限（vars 之间互相引用）：超过就是写错了，别让它无限套
constexpr int kMaxChainDepth = 4;

// 一个已解析的变量：键是 "color.x" / "number.y"，值是最终要填进去的东西
struct ResolvedVars
{
    std::map<std::string, Json> values;
};

// `$color.x` / `$number.y` -> block="color"/"number"，name="x"
bool splitVarReference(const std::string& text, std::string& block, std::string& name)
{
    if (text.rfind("$color.", 0) == 0)
    {
        block = "color";
        name  = text.substr(7);
    }
    else if (text.rfind("$number.", 0) == 0)
    {
        block = "number";
        name  = text.substr(8);
    }
    else
    {
        return false;
    }

    return !name.empty();
}

// 递归找"非顶层"的 vars：只有顶层那一个允许存在
bool findNestedVars(const Json& node, const std::string& path, std::string& where)
{
    if (node.is_object())
    {
        for (const auto& [key, value] : node.items())
        {
            if (key == "vars")
            {
                where = path.empty() ? std::string("顶层的下一层") : path;
                return true;
            }
            if (findNestedVars(value, path.empty() ? key : path + "." + key, where))
                return true;
        }
    }
    else if (node.is_array())
    {
        for (size_t i = 0; i < node.size(); i++)
        {
            if (findNestedVars(node[i], path + "[" + std::to_string(i) + "]", where))
                return true;
        }
    }
    return false;
}

// 解析一个变量（允许链式 `$color.a` -> `$color.b` -> 值）
bool resolveOne(const std::string& block, const std::string& name, const Json& colors,
    const Json& numbers, ResolvedVars& out, std::set<std::string>& visiting, int depth,
    std::string& error)
{
    const std::string key = block + "." + name;

    if (const auto found = out.values.find(key); found != out.values.end())
        return true; // 已经算过

    if (!visiting.insert(key).second)
    {
        error = "vars 出现循环引用：$" + key + "（引用链回到了自己）";
        return false;
    }

    if (depth > kMaxChainDepth)
    {
        error = "vars 的引用链超过 " + std::to_string(kMaxChainDepth) + " 层：$" + key;
        return false;
    }

    const Json& source = (block == "color") ? colors : numbers;
    if (!source.is_object() || !source.contains(name))
    {
        error = "引用了不存在的变量：$" + key + "（写法是 $color.名字 / $number.名字）";
        return false;
    }

    const Json& raw = source[name];
    Json value      = raw;

    // 值本身是 `$...`：链式解析（类型必须一致 —— 颜色不能指向数值）
    if (raw.is_string())
    {
        const std::string text = raw.get<std::string>();
        if (!text.empty() && text[0] == '$')
        {
            std::string nextBlock;
            std::string nextName;
            if (!splitVarReference(text, nextBlock, nextName))
            {
                error = "变量 $" + key + " 的引用写法不对：\"" + text
                    + "\"（写法是 $color.名字 / $number.名字）";
                return false;
            }
            if (nextBlock != block)
            {
                error = "变量 $" + key + "（" + block + "）不能引用另一个类型的变量：\"" + text + "\"";
                return false;
            }
            if (!resolveOne(nextBlock, nextName, colors, numbers, out, visiting, depth + 1, error))
                return false;

            value = out.values[nextBlock + "." + nextName];
        }
    }

    // 类型检查：颜色块必须是字符串，数值块必须是数字或字符串（@theme/metric.x）
    if (block == "color" && !value.is_string())
    {
        error = "颜色变量 $color." + name + " 必须是字符串（\"#RRGGBB[AA]\" 或 \"@theme/color.x.y\"）";
        return false;
    }
    if (block == "number" && !value.is_number() && !value.is_string())
    {
        error = "数值变量 $number." + name
            + " 必须是数字或 \"@theme/metric.x.y\" 这样的引用";
        return false;
    }

    visiting.erase(key);
    out.values[key] = value;
    return true;
}

// 全文替换（只碰"值"，不碰键；递归进对象与数组）
bool replaceVars(Json& node, const ResolvedVars& vars, const std::string& path, std::string& error)
{
    if (node.is_string())
    {
        const std::string text = node.get<std::string>();
        if (text.empty() || text[0] != '$')
            return true; // 不是变量引用，原样保留

        std::string block;
        std::string name;
        if (!splitVarReference(text, block, name))
        {
            error = "\"" + text + "\"（在 " + (path.empty() ? "顶层" : path)
                + "）不是合法的变量引用：写法是 $color.名字 / $number.名字";
            return false;
        }

        const auto found = vars.values.find(block + "." + name);
        if (found == vars.values.end())
        {
            error = "引用了不存在的变量：" + text + "（在 " + (path.empty() ? "顶层" : path) + "）";
            return false;
        }

        node = found->second;
        return true;
    }

    if (node.is_object())
    {
        for (auto& [key, value] : node.items())
        {
            const std::string childPath = path.empty() ? key : path + "." + key;
            if (!replaceVars(value, vars, childPath, error))
                return false;
        }
        return true;
    }

    if (node.is_array())
    {
        for (size_t i = 0; i < node.size(); i++)
        {
            if (!replaceVars(node[i], vars, path + "[" + std::to_string(i) + "]", error))
                return false;
        }
        return true;
    }

    return true;
}

} // namespace

bool expandJsonVars(Json& doc, std::string& error)
{
    if (!doc.is_object())
        return true;

    // ---- ① 取顶层 vars ----
    Json vars = Json::object();
    if (doc.contains("vars"))
    {
        if (!doc["vars"].is_object())
        {
            error = "vars 必须是对象：{ \"colors\": {...}, \"numbers\": {...} }";
            return false;
        }
        vars = doc["vars"];
        doc.erase("vars");
    }

    // 嵌套的 vars 一律报错（"一个文件只有一个 vars 块"）
    std::string nestedWhere;
    if (findNestedVars(doc, std::string(), nestedWhere))
    {
        error = "vars 只能有一个、且必须在顶层（在多出来的这个位置：" + nestedWhere + "）";
        return false;
    }

    if (!vars.empty())
    {
        Json colors  = Json::object();
        Json numbers = Json::object();
        for (const auto& [key, value] : vars.items())
        {
            if (key == "colors")
            {
                if (!value.is_object())
                {
                    error = "vars.colors 必须是对象";
                    return false;
                }
                colors = value;
            }
            else if (key == "numbers")
            {
                if (!value.is_object())
                {
                    error = "vars.numbers 必须是对象";
                    return false;
                }
                numbers = value;
            }
            else
            {
                error = "vars 里只能有 colors / numbers 两个子块，出现了 \"" + key + "\"";
                return false;
            }
        }

        // ---- ② 解析每个变量（支持链式，循环/类型不匹配都报错）----
        ResolvedVars resolved;
        std::vector<std::string> names;
        for (const auto& [name, value] : colors.items())
        {
            (void)value;
            names.push_back("color." + name);
        }
        for (const auto& [name, value] : numbers.items())
        {
            (void)value;
            names.push_back("number." + name);
        }

        for (const std::string& key : names)
        {
            const size_t dot   = key.find('.');
            const std::string block = key.substr(0, dot);
            const std::string name  = key.substr(dot + 1);

            std::set<std::string> visiting;
            if (!resolveOne(block, name, colors, numbers, resolved, visiting, 1, error))
                return false;
        }

        // ---- ③ 全文替换 ----
        if (!replaceVars(doc, resolved, std::string(), error))
            return false;
    }
    else
    {
        // 没有 vars 却写了 $color.x：仍然按"未知变量"报错（别让人以为是别的语法）
        ResolvedVars empty;
        if (!replaceVars(doc, empty, std::string(), error))
            return false;
    }

    return true;
}

} // namespace compile
} // namespace bklv

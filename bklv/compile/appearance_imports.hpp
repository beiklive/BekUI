#pragma once

// 外观的 `import` 机制（外观引用外观）：逐字段合并 + 循环/深度/父级检查。
// 这是**解析**关注点，所以从 core/appearance.cpp（运行时 TU）搬到这里。

#include <string>
#include <vector>

#include "bklv/compile/json_util.hpp"

namespace bklv
{
namespace compile
{

// 逐字段合并：对象递归合并、layers 按 id 合并、其它数组整体替换、标量覆盖（当前文件优先）
Json mergeJson(const Json& base, const Json& over);

// 解析 doc 里的 import（字符串或字符串数组）。循环引用 / 深度 > 8 / 父级不存在都报错。
bool resolveImports(const Json& doc, const ThemeData& theme, std::vector<std::string>& visited,
    Json& out, std::string& error);

} // namespace compile
} // namespace bklv

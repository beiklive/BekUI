#pragma once

// 外观的 `children` 段解析（把"子节点声明"读成 AppearanceChild 树）。
// 只看 JSON，不建视图 —— 建视图在 widgets/appearance_children.cpp。
// 规则见 docs/parse-refactor-review.md §12.5（互斥、深度 16、写错必报错）。

#include <string>
#include <vector>

#include "bklv/json/json_util.hpp" // Json 别名 + 取值/报错
#include "bklv/theme/appearance.hpp"
#include "bklv/theme/theme_data.hpp"

namespace bklv
{
namespace compile
{

// 解析 doc["children"]（没有该段就是空、返回 true）。失败写 error 返回 false。
bool parseChildren(const Json& doc, const ThemeData& theme,
    std::vector<AppearanceChild>& out, std::string& error);

} // namespace compile
} // namespace bklv

#pragma once

// 解析一个 **box 对象**（外观的 `box` 段、`children` 里子节点的 `box` 节点都走它）。
//
// 为什么单独抽：这段逻辑（含 readLength：数字 / "auto" / "50%" / 主题引用，以及全套 hasXxx
// 存在标记）原来内联在 `parseLayers` 里（382 行里占了 150 行），于是 children 只能手搓一份
// 残缺的解析。抽出来之后两边共用同一份，字段支持与"没写不动"的语义天然一致。

#include <string>

#include "bklv/compile/json_util.hpp"
#include "bklv/core/appearance.hpp" // AppearanceBox

namespace bklv
{
namespace compile
{

// boxNode 不是对象时返回 true（等于"没有 box 段"，什么都不覆盖）。
bool parseBoxNode(const Json& boxNode, const ThemeData& theme, AppearanceBox& box,
    std::string& error, const std::string& where);

} // namespace compile
} // namespace bklv

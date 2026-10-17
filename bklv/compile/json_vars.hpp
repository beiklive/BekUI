#pragma once

// JSON 的**变量块**：一份文件在顶层写一个 `vars`，用 `$color.x` / `$number.y` 引用它。
//
// 约定的分工（2026-10 用户拍板）：
//   · `@前缀` = 引用**其他文件**：`@theme/...`（颜色/数值）、`@i18n/...`、`@res/...`
//   · `$前缀` = 引用**同文件**的 vars：`$color.accent`、`$number.row`
//
// 五条规则（都不静默降级，写错就报错并指出位置）：
//   ① 一个文件**只有一个** vars，且必须在**顶层**；嵌套出现 = 报错
//   ② vars 只有两个子块 `colors` / `numbers`，别的子块 = 报错
//   ③ 值可以是字面量（`"#FF8800CC"` / `12`），也可以是 `@theme/...`（交给后面的解析器）
//   ④ vars 之间可以用 `$` 互相引用（链式，最多 4 层；循环 = 报错）
//   ⑤ `$` 的作用域是**整份文档**：box / layers / states / transitions / children 里都能直接用，
//      不需要在子节点里再写一次 vars（也不允许）
//
// 展开只是"把 `$x` 换成那个值"，**不做任何数值加工** —— 缩放（uiScale）、透明度不缩放、
// 报错口径全部由现有的 resolveColor / resolveNumber 决定。所以"写 vars"与"直接写那个值"
// 必须完全等价。

#include "bklv/compile/json_util.hpp"

namespace bklv
{
namespace compile
{

// 就地展开 doc：
//   1. 取出顶层 vars（并检查没有嵌套 vars），解析出最终值（字面量 / @theme 引用）
//   2. 把整份文档里所有值位置的 `$color.x` / `$number.y` 换成最终值
//   3. 删掉 vars（避免下游把它当成未知键报警告）
// 失败时写 error（带 JSON 路径）返回 false；doc 未被修改时不保证部分展开。
bool expandJsonVars(Json& doc, std::string& error);

} // namespace compile
} // namespace bklv

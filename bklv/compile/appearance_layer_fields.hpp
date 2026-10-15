#pragma once

// 图层的**通用字段**解析（颜色 / 数值字段表 + cutout + inset + offset）。
//
// 注意：这里是"表驱动"的 —— 两张字段表被**所有 op 共用**，不是按 op 分支。
// 每种 op 的个性（默认值 / 键表 / 层序 / 钩子）在 compile/layer_registry.cpp 里。
// 所以这一层按"关注点"拆，不按 op 拆（见 docs/parse-refactor-review.md §12.12）。

#include <string>

#include "bklv/compile/json_util.hpp"
#include "bklv/core/appearance.hpp" // AppearanceLayer

namespace bklv
{
namespace compile
{

bool parseLayerFields(const Json& node, const ThemeData& theme, AppearanceLayer& layer,
    std::string& error);

} // namespace compile
} // namespace bklv

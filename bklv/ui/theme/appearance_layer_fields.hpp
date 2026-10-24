#pragma once

// 图层的**通用字段**解析（颜色 / 数值字段表 + cutout + inset + offset）。
//
// 注意：这里是"表驱动"的 —— 两张字段表被**所有 op 共用**，不是按 op 分支。
// 每种 op 的个性（默认值 / 键表 / 层序 / 钩子）在 compile/layer_registry.cpp 里。
// 所以这一层按"关注点"拆，不按 op 拆（见 docs/parse-refactor-review.md §12.12）。
//
// 这里还负责**扩展层自己声明的参数**（parseDeclaredParams）：注册时声明了参数模式，
// 引擎就按模式自动解析，第三方不用为标量参数写任何代码。

#include <string>

#include "bklv/foundation/json/json_util.hpp"
#include "bklv/ui/theme/layer_registry.hpp"
#include "bklv/ui/theme/appearance.hpp" // AppearanceLayer

namespace bklv
{
namespace compile
{

bool parseLayerFields(const Json& node, const ThemeData& theme, AppearanceLayer& layer,
    std::string& error);

// 按注册时声明的参数模式解析这一层的参数槽（`AppearanceLayer::params`）。
//
// 六种类型全部自动：NUMBER / PIXELS（乘 uiScale）/ COLOR（含 @theme/ 引用）/ BOOL /
// ENUM（拼错警告并保持默认值）/ STRING。JSON 里没写就用模式里的默认值。
// 注册表里没声明参数的类型（内置 6 种）直接返回 true，什么都不做。
bool parseDeclaredParams(const Json& node, const LayerType& type, const ThemeData& theme,
    AppearanceLayer& layer, std::string& error, const std::string& where);

} // namespace compile
} // namespace bklv

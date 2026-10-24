#pragma once

// `rect` 层的渐变解析（`gradient{ type, angle, center, stops[] }`）。
// 校验从严：停靠点数量 / `at` 递增 / 越界都**报错**，不静默降级（见 docs/rect-gradient.md）。

#include <string>

#include "bklv/foundation/json/json_util.hpp"
#include "bklv/ui/theme/appearance.hpp" // AppearanceLayer

namespace bklv
{
namespace compile
{

// node 里没有 gradient 时返回 true（等于"这一层没有渐变"）。
bool parseGradient(const Json& node, const ThemeData& theme, AppearanceLayer& layer,
    std::string& error);

} // namespace compile
} // namespace bklv

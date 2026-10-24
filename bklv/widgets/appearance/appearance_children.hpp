#pragma once

// 按外观的 `children` 段**建子视图**（规格见 docs/parse-refactor-review.md §12）。
//
// 为什么放 widgets 而不是 compile：`compile/` 只把 JSON 变成纯数据（`AppearanceChild`），
// 建视图是控件层的事（要 new / addView / 套 box），这两层分开才好测、也不互相拖依赖。

#include <vector>

#include "bklv/theme/appearance.hpp"

namespace bklv
{

class AppearanceView;

namespace appearance
{

// 按 spec 建子视图并挂到 parent 上。已支持：APPEARANCE（外观）、BOX（纯布局盒）、
// 两者都递归；VIEW_TEMPLATE（ref）留下一里程碑（会打一条 warning，不会静默）。
void buildChildren(AppearanceView& parent, const std::vector<AppearanceChild>& spec);

} // namespace appearance
} // namespace bklv

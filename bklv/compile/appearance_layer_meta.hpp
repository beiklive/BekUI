#pragma once

// 单个图层的**元信息**解析：op 串 -> 枚举（含 EXTENSION 兜底）/ 默认 id / source / align /
// valign / scaling / 枚举拼错的编译期警告。
// 注意：它**造出**一层（输出参数），往 layers 里 push 是循环的职责。
// 每种 op 的个性（键表、层序、默认值）在 layer_registry.cpp（见 §12.12）。

#include <cstddef>
#include <string>

#include "bklv/compile/json_util.hpp"
#include "bklv/compile/layer_registry.hpp" // LayerType（输出参数用它）
#include "bklv/core/appearance.hpp"

namespace bklv
{
namespace compile
{

// op 串 -> 枚举。不认识的 op：注册表里有同名扩展就返回 EXTENSION，否则 ok = false。
AppearanceLayer::Op parseOp(const std::string& text, bool& ok);

// 解析一层的元信息。`outType` 输出这一层在注册表里的类型（内置或扩展），
// 调用方拿它继续解析参数模式（parseDeclaredParams）与绘制（LayerDrawContext::type）。
// 行为与之前一致：失败返回 false 并写 error。
bool parseLayerMeta(const Json& node, size_t index, const ThemeData& theme, AppearanceLayer& out,
    const LayerType*& outType, std::string& error);

} // namespace compile
} // namespace bklv

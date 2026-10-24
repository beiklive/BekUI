#pragma once

// 内置图层的绘制实现（由注册表分派）。
// 钩子类型在 compile/layer_registry.hpp；实现放 widgets，由 ensureLayerDrawsRegistered() 注册。

#include "bklv/theme/layer_registry.hpp"

namespace bklv
{
namespace layers
{

void drawRect(const compile::LayerDrawContext& ctx);
void drawShadow(const compile::LayerDrawContext& ctx);
void drawGlass(const compile::LayerDrawContext& ctx);
void drawText(const compile::LayerDrawContext& ctx);
void drawImage(const compile::LayerDrawContext& ctx);
void drawCustom(const compile::LayerDrawContext& ctx);

void ensureLayerDrawsRegistered();

} // namespace layers
} // namespace bklv

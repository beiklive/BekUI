#include "bklv/ui/layer/layer_draw.hpp"

#include "bklv/3rd/imgui/imgui.h"

namespace bklv
{
namespace layers
{

// custom 层：外观只给几何与底色，真正的语义绘制交给控件自己。
// 这是"装饰走 JSON、语义留 C++"那条约定的落点 —— 控件在构造上下文时把回调塞进
// `ctx.customDraw`，所以这一层不需要认识任何控件类型（layer/ 不认识 widgets/）。
void drawCustom(const compile::LayerDrawContext& ctx)
{
    if (ctx.customDraw == nullptr)
        return;
    ctx.customDraw(ctx);
}

} // namespace layers
} // namespace bklv

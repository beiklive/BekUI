#include "bklv/widgets/layer_draw.hpp"

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{
namespace layers
{

// custom 层：外观只给几何与底色，真正的语义绘制交给控件自己（drawCustomLayer 虚函数）。
// 这是"装饰走 JSON、语义留 C++"那条约定的落点。
void drawCustom(const compile::LayerDrawContext& ctx)
{
    AppearanceView* view         = static_cast<AppearanceView*>(ctx.view);
    ImDrawList* drawList         = static_cast<ImDrawList*>(ctx.drawList);
    const AppearanceLayer& layer = ctx.layer;
    const float rect[4]          = { ctx.rect.minX, ctx.rect.minY, ctx.rect.maxX, ctx.rect.maxY };

    view->drawCustomLayer(drawList, layer, rect[0], rect[1], rect[2] - rect[0],
    rect[3] - rect[1], ctx.instance);
}

} // namespace layers
} // namespace bklv

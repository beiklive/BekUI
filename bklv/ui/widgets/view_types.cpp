#include "bklv/ui/view/json_view.hpp"

#include "bklv/ui/view/box.hpp"
#include "bklv/ui/view/view.hpp"
#include "bklv/ui/widgets/basic/image.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/basic/padding.hpp"
#include "bklv/ui/widgets/basic/progress_spinner.hpp"
#include "bklv/ui/widgets/container/h_scrolling_frame.hpp"
#include "bklv/ui/widgets/container/scrolling_frame.hpp"

namespace bklv
{

// “内置视图类型”的注册实现：Box / Row / Label / Image / 两个滚动容器 / 加载指示 / Padding。
//
// 放在 widgets/ 而不是解析层（view/view_parse.cpp）：解析层不该认识具体控件实现，
// 否则就是 view → widgets 的反向依赖。入口仍叫 registerBuiltinViewTypes()（在
// view/view_parse.cpp），由它做“只注册一次”的幂等与重置钩子登记。
void registerBuiltinWidgetViewTypes()
{
    registerViewType("Box", [] { return (View*)new Box(Axis::COLUMN); });
    registerViewType("Row", [] { return (View*)new Box(Axis::ROW); });
    registerViewType("Label", [] { return (View*)new Label(); });
    registerViewType("Image", [] { return (View*)new Image(); });
    registerViewType("ScrollingFrame", [] { return (View*)new ScrollingFrame(); });
    registerViewType("HScrollingFrame", [] { return (View*)new HScrollingFrame(); });
    registerViewType("ProgressSpinner", [] { return (View*)new ProgressSpinner(); });
    // 占位挤压（对标 borealis 的 brls:Padding）：[左][Padding][右] 把右边顶到另一头
    registerViewType("Padding", [] { return (View*)new Padding(); });
    // RecyclerFrame 不注册：它必须先有数据源才能用，光有类型没有意义。
}

} // namespace bklv

#include "bklv/widgets/widget_types.hpp"

#include "bklv/core/globals.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/widgets/bottom_bar.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/cell.hpp"
#include "bklv/widgets/dropdown.hpp"
#include "bklv/widgets/header.hpp"
#include "bklv/widgets/hints_bar.hpp"
#include "bklv/widgets/rectangle.hpp"
#include "bklv/widgets/recycler_view.hpp"
#include "bklv/widgets/shell.hpp"
#include "bklv/widgets/slider.hpp"
#include "bklv/widgets/status_area.hpp"
#include "bklv/widgets/tab_frame.hpp"

namespace bklv
{

namespace
{

// 这些控件类型是否已经注册过。放命名空间作用域（不是函数内 static）：
// globals 的重置要能把它翻回来，之后 registerWidgetViewTypes() 才会真的重新注册
//（否则 resetGlobals() 清掉注册表后，JSON 里的 Button / Shell 会静默消失）。
bool g_widgetTypesRegistered = false;

void resetWidgetViewTypes()
{
    g_widgetTypesRegistered = false;
}

} // namespace

void registerWidgetViewTypes()
{
    registerRecyclerViewType(); // JSON 可描述的回收列表（桥接层）

    // 注册状态在全局表的"重置钩子"里也要复位（约定见 core/globals.hpp）
    globals::getGlobalStore().addResetHook("widgetViewTypes", &resetWidgetViewTypes);

    if (g_widgetTypesRegistered)
        return;
    g_widgetTypesRegistered = true;
    // 装饰与容器
    registerViewType("Rectangle", [] { return (View*)new Rectangle(); });
    // 旧的 "AppletFrame" 三段式（页头/内容/底栏）已删除，但**类型名保留**：
    // 用户已有 JSON 里的 "type": "AppletFrame" 仍然能用（内部构造 Shell，
    // 子节点按普通列布局排下来，和旧的三段式排布一致）。新写 JSON 也可以用 "Shell"。
    registerViewType("AppletFrame", [] { return (View*)new Shell(); });
    registerViewType("Shell", [] { return (View*)new Shell(); });
    registerViewType("Header", [] { return (View*)new Header(); });
    registerViewType("BottomBar", [] { return (View*)new BottomBar(); });
    registerViewType("HintsBar", [] { return (View*)new HintsBar(); });
    registerViewType("StatusArea", [] { return (View*)new StatusArea(); });
    registerViewType("TabButton", [] { return (View*)new TabButton(); });

    // 可交互的（属性表里能配出可用的默认值）
    registerViewType("Button", [] { return (View*)new Button(); });
    registerViewType("TabFrame", [] { return (View*)new TabFrame(); });
    registerViewType("TabItem", [] { return (View*)new TabItem(); });
    registerViewType("Slider", [] { return (View*)new Slider(); });
    registerViewType("Dropdown", [] { return (View*)new Dropdown(); });

    // 单元格里只有"不需要数据源"的那些能裸用：单选 / 开关 / 详情 / 输入
    registerViewType("RadioCell", [] { return (View*)new RadioCell(); });
    registerViewType("BooleanCell", [] { return (View*)new BooleanCell(); });
    registerViewType("DetailCell", [] { return (View*)new DetailCell(); });
    registerViewType("SliderCell", [] { return (View*)new SliderCell(); });
    registerViewType("SelectorCell", [] { return (View*)new SelectorCell(); });
    registerViewType("InputCell", [] { return (View*)new InputCell(); });
}

} // namespace bklv

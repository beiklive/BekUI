#include "bklv/widgets/widget_types.hpp"

#include "bklv/core/globals.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/widgets/button.hpp"

namespace bklv
{

namespace
{

// Button 类型是否已经注册过。放命名空间作用域（不是函数内 static）：
// globals 的重置要能把它翻回来，之后 registerWidgetViewTypes() 才会真的重新注册
//（否则 resetGlobals() 清掉注册表后，JSON 里的 Button 会静默消失）。
bool g_widgetTypesRegistered = false;

void resetWidgetViewTypes()
{
    g_widgetTypesRegistered = false;
}

} // namespace

void registerWidgetViewTypes()
{
    // 注册状态在全局表的"重置钩子"里也要复位（约定见 core/globals.hpp）
    globals::getGlobalStore().addResetHook("widgetViewTypes", &resetWidgetViewTypes);

    if (g_widgetTypesRegistered)
        return;
    g_widgetTypesRegistered = true;

    // 只剩 Button 一种外观驱动的控件（其余控件已随 demo 重做一起移除）。
    // Box / Label 属于 core 与 views，JSON 里用它们自己的类型名构造。
    registerViewType("Button", [] { return (View*)new Button(); });
}

} // namespace bklv

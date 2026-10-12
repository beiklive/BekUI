#include "bklv/widgets/widget_types.hpp"

#include "bklv/core/json_view.hpp"
#include "bklv/widgets/applet_frame.hpp"
#include "bklv/widgets/bottom_bar.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/cell.hpp"
#include "bklv/widgets/dropdown.hpp"
#include "bklv/widgets/header.hpp"
#include "bklv/widgets/hints_bar.hpp"
#include "bklv/widgets/rectangle.hpp"
#include "bklv/widgets/sidebar.hpp"
#include "bklv/widgets/slider.hpp"
#include "bklv/widgets/status_area.hpp"
#include "bklv/widgets/tab_frame.hpp"

namespace bklv
{

void registerWidgetViewTypes()
{
    static bool done = false;
    if (done)
        return;
    done = true;

    // 装饰与容器
    registerViewType("Rectangle", [] { return (View*)new Rectangle(); });
    registerViewType("AppletFrame", [] { return (View*)new AppletFrame(); });
    registerViewType("Header", [] { return (View*)new Header(); });
    registerViewType("BottomBar", [] { return (View*)new BottomBar(); });
    registerViewType("Sidebar", [] { return (View*)new Sidebar(); });
    registerViewType("HintsBar", [] { return (View*)new HintsBar(); });
    registerViewType("StatusArea", [] { return (View*)new StatusArea(); });
    registerViewType("TabCell", [] { return (View*)new TabCell(); });

    // 可交互的（属性表里能配出可用的默认值）
    registerViewType("Button", [] { return (View*)new Button(); });
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

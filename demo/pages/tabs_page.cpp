#include "tabs_page.hpp"

#include <string>

#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/container/list_box.hpp"
#include "bklv/ui/widgets/container/tab_view.hpp"
#include "bklv/ui/widgets/controls/button.hpp"
#include "bklv/ui/widgets/controls/tab_bar.hpp"
#include "demo/demo_app.hpp"
#include "demo/style.hpp"

namespace demo
{

namespace
{

bklv::View* makeTextTab(const std::string& title, const std::string& body)
{
    auto* page = new bklv::Box(bklv::Axis::COLUMN);
    page->setGap(12.0f);
    page->setPadding(24.0f);

    auto* heading = makeTitle(title);
    auto* text    = makeParagraph(body);
    page->addView(heading);
    page->addView(text);
    return page;
}

bklv::View* makeListTab()
{
    auto* page = new bklv::Box(bklv::Axis::COLUMN);
    page->setGap(12.0f);
    page->setPadding(24.0f);

    page->addView(makeTitle("列表"));
    page->addView(makeNote("滚一段、把焦点移到第 10 条附近，然后切到别的标签再切回来。"));

    auto* list = new bklv::ListBox();
    list->setGrow(1.0f);
    list->setWidthPercentage(100.0f);
    for (int i = 1; i <= 20; i++)
        list->addItemText("第 " + std::to_string(i) + " 条（切回来时这里应该还停在同一处）");

    page->addView(list);
    return page;
}

} // namespace

bklv::View* TabsPage::build(DemoApp& app)
{
    auto* root = new bklv::Box(bklv::Axis::COLUMN);
    root->setGap(12.0f);

    root->addView(makeTitle("标签页"));
    root->addView(makeNote("顶栏的「切换排列」按钮可以在「顶部水平」和「左侧竖直」之间来回切；"
                           "切标签时列表的滚动位置与焦点会记住。"));

    // 排列容器：水平条压在内容上（COLUMN），竖直条放左边（ROW）。
    // 两种排列里子节点顺序都是「条 → 内容」，所以只翻 axis 就够。
    auto* layout = new bklv::Box(this->vertical ? bklv::Axis::ROW : bklv::Axis::COLUMN);
    layout->setGrow(1.0f);
    layout->setGap(0.0f);

    auto* bar = new bklv::TabBar(this->vertical ? bklv::TabBar::Orientation::VERTICAL
                                           : bklv::TabBar::Orientation::HORIZONTAL);
    bar->addItem("说明", "\uE001", "intro");
    bar->addItem("列表", "\uE002", "list");
    bar->addItem("交互", "\uE003", "action");

    auto* tabs = new bklv::TabView();
    tabs->addTab("intro", makeTextTab("说明", "标签页是同级切换：非当前页只是 GONE，对象和状态都还在。"));
    tabs->addTab("list", makeListTab());
    tabs->addTab("action", makeTextTab("交互", "每个标签页里都可以继续放按钮、列表，甚至再下钻一层（用 ContentStack）。"));

    // 双向同步：点/按确认切标签 → TabView 生效；TabView 被代码切 → 条上的选中态跟着变
    bar->setOnTabSelected([tabs](int index) { tabs->setActiveTab(index); });
    tabs->setOnTabChanged([bar](int index, bklv::View*) { bar->setActiveIndex(index, false); });
    tabs->setActiveTab(0);
    bar->setActiveIndex(0, false);

    layout->addView(bar);
    layout->addView(tabs);
    root->addView(layout);

    auto* toggle = new bklv::TextButton("切换排列（顶部 / 左侧）");
    toggle->setOnClick([layout, bar, &app]() {
        const bool vertical = !bar->isVertical();
        bar->setOrientation(vertical ? bklv::TabBar::Orientation::VERTICAL
                                     : bklv::TabBar::Orientation::HORIZONTAL);
        layout->setAxis(vertical ? bklv::Axis::ROW : bklv::Axis::COLUMN);
        app.app().notify(vertical ? "标签条：左侧竖直" : "标签条：顶部水平");
    });
    root->addView(toggle);

    return root;
}

} // namespace demo

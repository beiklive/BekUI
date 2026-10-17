// 无窗口测试：底部导航条 TabBar（苹果风外壳的那条栏）。
//
// 它和 Sidebar 的区别只有两条：横排 + "A/点击才切页"（iOS 语义）。
// 这里钉住：条目/选中态、A 触发回调、越界安全、两套主题都能取到外观与尺寸。

#include <cstdio>
#include <string>

#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/tab_bar.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

TabCell* cellAt(TabBar& bar, size_t index)
{
    return static_cast<TabCell*>(bar.getItemView(index));
}

} // namespace

int main()
{
    bklv::initResources();
    setTheme("default");

    std::printf("\n== 1. 条目与选中态 ==\n");
    {
        TabBar bar;
        int fired = -1;

        bar.addItem("全部", [&fired] { fired = 0; });
        bar.addItem("最近", [&fired] { fired = 1; });
        bar.addItem("已下载", [&fired] { fired = 2; });

        checkEqualInt((int)bar.getItemCount(), 3, "三个条目");
        checkEqualInt((int)bar.getSelectedIndex(), 0, "默认选中第 0 个");
        check(cellAt(bar, 0) != nullptr && cellAt(bar, 0)->isSelected(), "第 0 个是选中态");
        check(cellAt(bar, 1) != nullptr && !cellAt(bar, 1)->isSelected(), "第 1 个不是");

        bar.setSelectedIndex(2);
        checkEqualInt((int)bar.getSelectedIndex(), 2, "手动选第 2 个");
        check(cellAt(bar, 2)->isSelected(), "第 2 个变选中");
        check(!cellAt(bar, 0)->isSelected(), "第 0 个取消选中");
        checkEqualInt(fired, -1, "setSelectedIndex 不触发点击回调（那是 A/点击的事）");

        // A / 点击：TabCell::trigger()
        cellAt(bar, 1)->trigger();
        checkEqualInt((int)bar.getSelectedIndex(), 1, "触发第 1 个就选中它");
        checkEqualInt(fired, 1, "回调收到了正确下标");
        check(cellAt(bar, 1)->isSelected(), "触发之后它是选中态");

        bar.setSelectedIndex(99);
        checkEqualInt((int)bar.getSelectedIndex(), 1, "越界下标被忽略（不崩、不改状态）");
        check(bar.getItemView(99) == nullptr, "越界取条目返回 nullptr");

        bar.clearItems();
        checkEqualInt((int)bar.getItemCount(), 0, "清空之后没有条目");
        checkEqualInt((int)bar.getSelectedIndex(), 0, "选中下标回到 0");
    }

    std::printf("\n== 2. 外观：两套主题都能编译，尺寸来自主题 ==\n");
    {
        setTheme("default");
        TabBar def;
        def.addItem("全部", nullptr);
        const float defaultHeight = def.getActiveAppearance().getBox().height;
        checkFloat(defaultHeight, getTheme().number("metric.tabBar.height", 0.0f),
            "default：条高来自 metric.tabBar.height");

        setTheme("ios");
        TabBar ios;
        ios.addItem("全部", nullptr);
        checkFloat(ios.getActiveAppearance().getBox().height, getTheme().number("metric.tabBar.height", 0.0f),
            "ios：条高也来自主题（数值可以不同）");
        check(getTheme().number("metric.tabBar.height", 0.0f) > 0.0f, "ios 主题声明了 tabBar.height");

        setTheme("default");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

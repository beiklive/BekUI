#include "debug_tab.hpp"

#include <string>

#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/container/scroll_box.hpp"
#include "bklv/ui/widgets/controls/button.hpp"
#include "demo/demo_app.hpp"
#include "demo/layout_kit.hpp"
#include "demo/style.hpp"

namespace demo
{

namespace
{

bklv::Box* makeRow()
{
    auto* row = new bklv::Box(bklv::Axis::ROW);
    row->setGap(12.0f);
    row->setAlignItems(bklv::AlignItems::CENTER);
    return row;
}

} // namespace

bklv::View* buildDebugTab(DemoApp& app)
{
    auto* scroll = new bklv::ScrollBox(bklv::Axis::COLUMN);
    scroll->setGrow(1.0f);

    auto* content = scroll->getContentBox();
    content->setGap(26.0f);
    content->setPadding(24.0f);

    // 状态行：每次点按钮后刷新
    auto* status = makeNote(app.describeDebugState());

    // 1) 应用内日志
    {
        auto* row = makeRow();

        auto* toggle = new bklv::TextButton("显示 / 隐藏日志");
        toggle->setOnClick([&app, status] {
            app.toggleLogView();
            status->setText(app.describeDebugState());
        });

        row->addView(toggle);
        content->addView(makeSection("应用内日志", row, "DebugLogView · 挂在外壳调试层"));
    }

    // 2) FPS 浮层
    {
        auto* row = makeRow();

        auto* toggle = new bklv::TextButton("显示 / 隐藏 FPS");
        toggle->setOnClick([&app, status] {
            app.toggleFpsOverlay();
            status->setText(app.describeDebugState());
        });

        row->addView(toggle);
        content->addView(makeSection("FPS / 帧时间", row, "FpsOverlay · p50 / p99"));
    }

    // 3) 调试边框
    {
        auto* row = makeRow();

        auto* toggle = new bklv::TextButton("调试边框开 / 关");
        toggle->setOnClick([&app, status] {
            app.toggleDebugLayer();
            status->setText(app.describeDebugState());
        });

        row->addView(toggle);
        content->addView(makeSection("调试边框", row, "DebugLayer · 画出所有视图边界"));
    }

    // 4) 按页背景
    {
        auto* row = makeRow();

        auto* cycle = new bklv::TextButton("换一种背景");
        cycle->setOnClick([&app, status] {
            app.cycleBackdrop();
            status->setText(app.describeDebugState());
        });

        row->addView(cycle);
        content->addView(makeSection("按页背景", row, "PresetBackdrop · 网格 / 漂浮 / 渐变 / 棋盘 / 多边形 / 关"));
    }

    // 5) 安全区
    {
        auto* row = makeRow();

        auto* cycle = new bklv::TextButton("切换安全区内缩");
        cycle->setOnClick([&app, status] {
            app.cycleSafeArea();
            status->setText(app.describeDebugState());
        });

        row->addView(cycle);
        content->addView(makeSection("安全区", row, "Shell::setSafeAreaInsets · 0 / 24 / 48"));
    }

    // 6) 玻璃开关与质量
    {
        auto* row = makeRow();

        auto* cycle = new bklv::TextButton("切换玻璃档位");
        cycle->setOnClick([&app, status] {
            app.cycleGlass();
            status->setText(app.describeDebugState());
        });

        row->addView(cycle);
        content->addView(makeSection("玻璃", row, "Shell::setGlassEnabled / setGlassQuality"));
    }

    content->addView(makeSection("当前状态", status, "点上面的按钮会实时更新"));
    return scroll;
}

} // namespace demo

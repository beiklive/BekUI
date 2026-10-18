// bklv 图标总览：只演示图标字体里实际存在的全部字形。
//
// 操作：滚轮/拖拽滚动；F1 无（其它演示页已清理）。

#include "app/demo_app.hpp"
#include "bklv/bklv.hpp"

int main(int argc, char** argv)
{
    bklv::App app;

    demo::DemoApp host;
    app.setRootView(host.getView());
    app.onUpdate = [&host]() { host.update(); };

    (void)argc;
    (void)argv;

    return app.run(bklv::getStr("app/title"), 1280, 720);
}

// bklv 演示：主页 + 三个内容页（图标 / 水平 TabFrame / 竖直 TabFrame）。
//
// 操作：方向键移动焦点，A / 点击进入，B / 「返回」回主页。
// 自动化：--open N 启动后直接打开第 N 个演示页；--screenshot <png> --frames N 截图退出。

#include <cstring>
#include <cstdlib>

#include "app/demo_app.hpp"
#include "bklv/bklv.hpp"

int main(int argc, char** argv)
{
    std::string screenshot;
    int         frames = 0;
    int         open   = 0;
    int         bg     = -1; // -1 = 没传 --bg，保留 DemoApp 里的默认值

    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
            screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            frames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--open") == 0 && i + 1 < argc)
            open = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--bg") == 0 && i + 1 < argc)
            bg = std::atoi(argv[++i]);
    }

    bklv::App app;

    demo::DemoApp host;
    app.setRootView(host.getView());
    // 只在**显式传了 --bg** 时覆盖；否则用 DemoApp 构造函数里的默认值
    if (bg >= 0)
        host.setBackdropPreset(bg);
    app.onUpdate = [&host]() { host.update(); };

    if (!screenshot.empty())
        app.requestScreenshot(screenshot, frames > 0 ? frames : 10);

    if (open > 0)
    {
        // 首帧建好主页后再打开（字体图集此时才就绪）
        app.onUpdate = [&host, open, done = false]() mutable {
            host.update();
            if (!done)
            {
                done = true;
                host.openDemo(open);
            }
        };
    }

    return app.run(bklv::getStr("app/title"), 1280, 720);
}

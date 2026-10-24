// Switch 最小宿主：devkitA64 编出来的 .nro 里就这一个界面。
//
// demo 已移除（重做中），但 scripts/build_switch.sh 和 CI 的 switch 任务还得有
// 一个可执行入口，所以放一个只依赖基础框架（Box / Button / Label）的样例：
// 它验证的是"库在 Switch 上能起窗口、能读 romfs 里的字体与主题、按钮能响应"，
// 不含任何演示页面。
//
// 与示例例子的区别：这里不注入宿主 ImGui，也不接受 --screenshot ——
// Switch 上没有窗口系统，截帧得靠 nxlink 之外的调试手段。

#include "bklv/bklv.hpp"

int main(int, char**)
{
    bklv::App app;

    bklv::Box root(bklv::Axis::COLUMN);
    root.setJustifyContent(bklv::JustifyContent::CENTER);
    root.setAlignItems(bklv::AlignItems::CENTER);
    root.setGap(16.0f);
    root.setPadding(32.0f);

    // 按下时 App 会播 CLICK 音效、弹一条通知：一次点击验证输入、音频、通知三条链路。
    bklv::Button button("BekUI");
    button.setOnClick([&app]() { app.notify("Hello, Switch"); });

    bklv::Label hint;
    hint.setText("BekUI on Nintendo Switch");

    root.addView(&button);
    root.addView(&hint);

    app.setRootView(&root);

    return app.run("BekUI", 1280, 720);
}

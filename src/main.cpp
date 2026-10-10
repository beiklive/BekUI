// bklv 能力总览：Tab 页面。
//
// 用 bklv 自己的控件搭出 tab 导航 + 多页内容，逐页展示已经实现的框架能力。
// 没有任何 ImGui 窗口调用，也没有任何图形 API 调用——全部走 View 树。
//
// 读写入口：
//   app/page_host.*      页面宿主：标题 + tab 栏 + 内容区，负责切页
//   app/tab_bar.*        tab 栏与单个 tab 格子
//   app/pages/*          五个页面，各自独立
//   app/widgets.*        自定义控件（开关、环形/横向进度）
//   app/page_helpers.*   排版小工具（文字、卡片、行）
//
// 操作：
//   左右 / AD            切换 tab
//   上下 / WS            在页面内容里移动焦点
//   回车 / 空格 / 手柄 A 触发当前焦点控件的动作
//   鼠标点击             直接选中 tab 或控件
//   滚轮 / 拖拽          滚动页面
//   F1                  开关调试层（布局树 + 当前焦点 + 后端信息）

#include <string>

#include "app/page_host.hpp"
#include "bklv/core/app.hpp"

int main(int argc, char** argv)
{
    bklv::App app;

    demo::PageHost host;
    app.setRootView(host.getView());

    // 每帧回调：刷新调试页上的统计信息
    app.onUpdate = [&host]() { host.update(); };

    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "-v")
            app.setDebugLayerEnabled(true);
    }

    return app.run("bklv 能力总览", 1280, 800);
}

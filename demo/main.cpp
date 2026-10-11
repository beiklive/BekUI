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
//   右上角「显示日志」   开关 ImGui 日志窗口（可拖动、可缩放、可筛选级别）

#include "app/page_host.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/app.hpp"

int main(int argc, char** argv)
{
    bklv::App app;

    BKLV_LOG_INFO("开始构建界面");
    demo::PageHost host;
    app.setRootView(host.getView());
    BKLV_LOG_INFO("界面构建完成");

    // 每帧回调：刷新调试页上的统计信息
    app.onUpdate = [&host]() { host.update(); };

    // ImGui 窗口（日志窗口）必须在视图树画完后、ImGui::Render() 之前下发
    app.onDrawWindows = [&host]() { host.drawWindows(); };

    (void)argc;
    (void)argv;

    return app.run(bklv::getStr("app/title"), 1280, 800);
}

// Demo 07：bklv 完整能力演示。
//
// 用到的模块：
//   RecyclerFrame   虚拟列表（1000 条数据只创建 8 个 cell）
//   ScrollingFrame  列表的滚动、惯性、滚动条
//   HScrollingFrame 顶部分类条的横向滚动
//   Image           图片 + 圆角 + 缩放模式
//   ProgressSpinner 加载指示器
//   输入与焦点       方向键/WASD 导航，鼠标点击也可切换焦点
//   提示栏          当前焦点注册的动作
//   (调试窗口见 src/bklv/views/debug_window.hpp)
//
// 场景内容在 demo_07_scene.h，方便无窗口验证复用同一棵树。

#include <string>

#include "bklv/core/app.hpp"
#include "demo_07_scene.h"

int main(int argc, char** argv)
{
    bklv::App app;
    app.setRootView(buildScene());

    // 图形后端是编译期确定的，没有运行时开关。
    // 自检构建（不绘制）用：./build.sh 里加 -DBKLV_NULL_BACKEND
    (void)argc;
    (void)argv;

    return app.run("Demo 07 - bklv showcase", 1280, 800);
}

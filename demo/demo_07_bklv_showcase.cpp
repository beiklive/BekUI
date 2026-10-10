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
//   DebugLayer      F1 开关，显示布局树和当前焦点
//
// 场景内容在 demo_07_scene.h，方便无窗口验证复用同一棵树。

#include <string>

#include "bklv/core/app.hpp"
#include "demo_07_scene.h"

int main(int argc, char** argv)
{
    bklv::App app;
    app.setRootView(buildScene());

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        // -v 启动时直接打开调试层
        if (arg == "-v")
            app.setDebugLayerEnabled(true);

        // -b <后端> 选择渲染后端：opengl / none（none 用于无图形环境下自检）
        if (arg == "-b" && i + 1 < argc)
        {
            const std::string name = argv[++i];
            if (name == "opengl")
                app.setGraphicsBackend(bklv::GraphicsBackendType::OPENGL);
            else if (name == "none")
                app.setGraphicsBackend(bklv::GraphicsBackendType::NONE);
            else if (name == "vulkan")
                app.setGraphicsBackend(bklv::GraphicsBackendType::VULKAN); // 尚未实现
        }
    }

    return app.run("Demo 07 - bklv showcase", 1280, 800);
}

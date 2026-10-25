// demo 入口：只做三件事 —— 解析参数、构造外壳、进帧循环。
//
// 参数：
//   --theme <name>        启动主题（默认 default；demo 里可切 default / glass）
//   --page <name>         启动页面：home（默认）/ settings
//   --screenshot <png>    渲染若干帧后截图存成 PNG 并退出（无人值守验证用）
//   --frames <n>          截图前等多少帧（默认 3）
//   --size <宽x高>        窗口尺寸（默认 1280x720）
//
// 页面与外壳在 demo_app.*，页面都在 demo/pages/ 下。

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "demo/demo_app.hpp"

namespace
{

void printUsage(const char* argv0)
{
    std::printf("用法：%s [--theme <name>] [--page <home|settings|stack|tabs|tabs-left>] [--screenshot <png>] [--frames <n>] [--size <宽x高>]\n",
                argv0);
}

// 解析 "--size 1280x720" 这种写法；返回 false 表示格式不对。
bool parseSize(const char* text, int& width, int& height)
{
    if (text == nullptr)
        return false;

    int parsedWidth  = 0;
    int parsedHeight = 0;
    if (std::sscanf(text, "%dx%d", &parsedWidth, &parsedHeight) != 2)
        return false;

    if (parsedWidth <= 0 || parsedHeight <= 0)
        return false;

    width  = parsedWidth;
    height = parsedHeight;
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    std::string theme;
    std::string page = "home";
    std::string screenshotPath;
    int screenshotFrames = 3;
    int width            = 1280;
    int height           = 720;

    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];

        if (std::strcmp(arg, "--theme") == 0 && i + 1 < argc)
        {
            theme = argv[++i];
        }
        else if (std::strcmp(arg, "--screenshot") == 0 && i + 1 < argc)
        {
            screenshotPath = argv[++i];
        }
        else if (std::strcmp(arg, "--page") == 0 && i + 1 < argc)
        {
            page = argv[++i];
        }
        else if (std::strcmp(arg, "--frames") == 0 && i + 1 < argc)
        {
            screenshotFrames = std::atoi(argv[++i]);
        }
        else if (std::strcmp(arg, "--size") == 0 && i + 1 < argc)
        {
            if (!parseSize(argv[++i], width, height))
            {
                std::fprintf(stderr, "--size 格式不对，应为 宽x高（例如 1280x720）\n");
                return 2;
            }
        }
        else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0)
        {
            printUsage(argv[0]);
            return 0;
        }
        else
        {
            std::fprintf(stderr, "未知参数：%s\n", arg);
            printUsage(argv[0]);
            return 2;
        }
    }

    demo::DemoApp app;

    // 主题要在 run() 之前装好：窗口建起来时第一帧就该是目标主题。
    if (!theme.empty() && !app.setTheme(theme))
    {
        std::fprintf(stderr, "主题打不开：%s（可用的见 resources/theme/themes.json）\n", theme.c_str());
        return 1;
    }

    if (page == "tabs")
    {
        // 直接压一层"标签页"演示（TabView + TabBar）
        app.openTabs(false);
    }
    else if (page == "tabs-left")
    {
        app.openTabs(true);
    }
    else if (page == "stack")
    {
        // 直接压一层"层级页"（用来验证内容层栈的入场/出场与焦点、滚动记忆）
        app.openStackList();
    }
    else if (page == "settings")
    {
        app.openSettings();
    }
    else if (page != "home")
    {
        std::fprintf(stderr, "未知页面：%s（可用 home / settings / stack / tabs）\n", page.c_str());
        return 2;
    }

    if (!screenshotPath.empty())
    {
        if (!app.app().requestScreenshot(screenshotPath, screenshotFrames))
        {
            std::fprintf(stderr, "当前后端不支持帧读回，无法截图\n");
            return 1;
        }
    }

    return app.run("BekUI demo", width, height);
}

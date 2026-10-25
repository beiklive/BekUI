// demo 入口：只做三件事 —— 解析参数、构造外壳、进帧循环。
//
// 参数：
//   --theme <name>        启动主题（默认 default；demo 里可切 default / glass）
//   --tab <name>          启动标签页：buttons（默认）/ layout / list / container / debug
//   --tabbar <top|left>   标签条排列：top（默认，顶部水平）/ left（左侧竖直）
//   --screenshot <png>    渲染若干帧后截图存成 PNG 并退出（无人值守验证用）
//   --frames <n>          截图前等多少帧（默认 3）
//   --size <宽x高>        窗口尺寸（默认 1280x720）
//
// 外壳与标签页在 demo_app.*，五个标签页的内容在 demo/tabs/ 下。

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "demo/demo_app.hpp"

namespace
{

void printUsage(const char* argv0)
{
    std::printf("用法：%s [--theme <name>] [--tab <buttons|layout|list|container|debug>] [--tabbar <top|left>] "
                "[--screenshot <png>] [--frames <n>] [--size <宽x高>]\n",
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
    std::string tab          = "buttons";
    bool tabbarVertical      = false;
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
        else if (std::strcmp(arg, "--tab") == 0 && i + 1 < argc)
        {
            tab = argv[++i];
        }
        else if (std::strcmp(arg, "--tabbar") == 0 && i + 1 < argc)
        {
            const std::string layout = argv[++i];
            if (layout == "left" || layout == "vertical")
                tabbarVertical = true;
            else if (layout != "top" && layout != "horizontal")
            {
                std::fprintf(stderr, "--tabbar 只认 top / left\n");
                return 2;
            }
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

    static const char* const kTabNames[] = { "buttons", "layout", "list", "container", "debug" };
    int initialTab                       = 0;
    if (tab == "0" || tab == "1" || tab == "2" || tab == "3" || tab == "4")
    {
        initialTab = tab[0] - '0';
    }
    else
    {
        for (int index = 0; index < 5; ++index)
        {
            if (tab == kTabNames[index])
            {
                initialTab = index;
                break;
            }
        }

        if (tab != kTabNames[initialTab])
        {
            std::fprintf(stderr, "未知标签页：%s（可用 buttons / layout / list / container / debug）\n", tab.c_str());
            return 2;
        }
    }

    demo::DemoApp app;
    app.setInitialTab(initialTab, tabbarVertical);

    // 主题要在 run() 之前装好：窗口建起来时第一帧就该是目标主题。
    if (!theme.empty() && !app.setTheme(theme))
    {
        std::fprintf(stderr, "主题打不开：%s（可用的见 resources/theme/themes.json）\n", theme.c_str());
        return 1;
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

// 外观设计器入口（见 designer/DESIGN.md）。
//
//   dist/designer [--theme <dir>] [--theme-name <name>] [--screenshot <png>] [--frames <n>]
//
// --screenshot 用于无人值守验证：渲染 N 帧后截图退出（GL 后端；VK 下截图走同一接口但
// 需要后端支持 readPixels，见 DESIGN.md 的验证约定）。

#include <cstdio>
#include <cstring>
#include <string>

#include "bklv/bklv.hpp"
#include "designer/ui/designer_app.hpp"

namespace
{

const char* argValue(int argc, char** argv, const char* flag)
{
    for (int i = 1; i + 1 < argc; i++)
        if (std::strcmp(argv[i], flag) == 0)
            return argv[i + 1];
    return nullptr;
}

} // namespace

int main(int argc, char** argv)
{
    designer::DesignerApp::Options options;
    if (const char* theme = argValue(argc, argv, "--theme"))
        options.themeDir = theme;
    if (const char* name = argValue(argc, argv, "--theme-name"))
        options.themeName = name;
    if (const char* shot = argValue(argc, argv, "--screenshot"))
        options.screenshotPath = shot;
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--source") == 0)
            options.startOnSource = true;
        if (std::strcmp(argv[i], "--palette") == 0)
            options.openPalette = true;
    }
    if (const char* name = argValue(argc, argv, "--create-project"))
        options.createProject = name;
    if (const char* zoom = argValue(argc, argv, "--zoom"))
        options.zoom = (float)std::atof(zoom);
    if (const char* click = argValue(argc, argv, "--click"))
    {
        options.clickX = (float)std::atof(click);
        const char* comma = std::strchr(click, ',');
        if (comma != nullptr)
            options.clickY = (float)std::atof(comma + 1);
    }
    if (const char* file = argValue(argc, argv, "--file"))
        options.initialFile = file;
    if (const char* frames = argValue(argc, argv, "--frames"))
        options.frames = std::atoi(frames);

    designer::DesignerApp host;
    std::string               error;
    if (!host.init(options, error))
    {
        std::fprintf(stderr, "设计器启动失败：%s\n", error.c_str());
        return 1;
    }

    if (!options.screenshotPath.empty())
    {
        host.getApp().requestScreenshot(options.screenshotPath, options.frames);
    }

    return host.getApp().run("外观设计器", 1280, 720);
}

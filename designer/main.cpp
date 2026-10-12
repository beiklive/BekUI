// bklv 设计器：读 / 改 / 存 主题的外观 JSON。
//
// 形态（和 demo 是两个程序，互不依赖）：
//   · 界面本体用 **ImGui 控件** 画（四块：文件列表 / 节点树 / 预览区 / 属性表 + 左下统计）
//   · 预览区用 **bklv 的真实视图**渲染：真编译外观、真控件、真整页视图
//
// 用法：dist/designer 启动后先用仓库自带的 default 主题浏览（只读），
// 要动手改就点「新建项目」：模板新建一份自己的主题，或直接编辑已有自建主题。

#include <cstdlib>
#include <string>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/bklv.hpp"
#include "designer/core/project.hpp"
#include "designer/core/theme_files.hpp"
#include "designer/ui/designer_window.hpp"

int main(int argc, char** argv)
{
    bklv::App app;

    // 设计器的界面全在 ImGui 窗口里，bklv 根视图留一个空盒子即可
    // （预览视图不进这棵树，由设计器自己布局+绘制，见 DesignerWindow::drawPreview）
    bklv::Box* root = new bklv::Box(bklv::Axis::COLUMN);
    root->setWidthPercentage(100.0f);
    root->setHeightPercentage(100.0f);
    app.setRootView(root);

    designer::DesignerWindow window;
    window.attach(&app);

    // 启动时用仓库自带的 default 主题先给用户看（只读：基准主题不许就地改）
    const std::string themeRoot = bklv::resourcePath("resources/theme");
    designer::Project initial;
    initial.mode      = designer::ProjectMode::INPLACE;
    initial.name      = "default";
    initial.themeRoot = themeRoot;

    // BKLV_DESIGNER_PROJECT=<主题名>：直接打开仓库里的某个主题（自查/脚本用）
    bool readOnly = true;
    if (const char* wanted = std::getenv("BKLV_DESIGNER_PROJECT"))
    {
        initial.name = wanted;
        readOnly     = designer::isBaseTheme(initial.name);
    }

    // 自查/脚本用：BKLV_DESIGNER_FORK=<新名> 先按模板新建一份，再打开它
    if (const char* forkName = std::getenv("BKLV_DESIGNER_FORK"))
    {
        designer::Project created;
        std::string forkError;
        if (designer::createProjectFromTemplate(themeRoot, "default", forkName, created, forkError))
        {
            initial  = created;
            readOnly = false;
        }
        else
            BKLV_LOG_WARNING("建项目失败：%s", forkError.c_str());
    }

    std::string error;
    if (!window.setProject(initial, readOnly, error))
        BKLV_LOG_WARNING("初始主题装载失败：%s（可以点「新建项目」换一个目录）", error.c_str());

    // 自查：BKLV_DESIGNER_FILE=<外观名> / SELECT=<file|box|layer<N>|state:名> / VIEW=<views 里的文件名>
    if (const char* wantedFile = std::getenv("BKLV_DESIGNER_FILE"))
    {
        std::string openError;
        window.openAppearance(wantedFile, openError);
    }
    if (const char* wantedView = std::getenv("BKLV_DESIGNER_VIEW"))
        window.openViewFile(wantedView);
    if (const char* wantedSelect = std::getenv("BKLV_DESIGNER_SELECT"))
        window.selectBySpec(wantedSelect);

    // 调试/自查：BKLV_DESIGNER_SHOT=<png> 渲染几帧后离屏截图并退出
    if (const char* shot = std::getenv("BKLV_DESIGNER_SHOT"))
    {
        int frames = 20;
        if (const char* requested = std::getenv("BKLV_DESIGNER_SHOT_FRAMES"))
            frames = std::atoi(requested);
        app.requestScreenshot(shot, frames);
    }

    (void)argc;
    (void)argv;
    return app.run("bklv 设计器", 1280, 820);
}

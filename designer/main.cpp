// bklv 设计器：读 / 改 / 存 主题的外观 JSON。
//
// 形态（和 demo 是两个程序，互不依赖）：
//   · 界面本体用 **ImGui 控件** 画（四块：文件列表 / 节点树 / 预览区 / 属性表 + 左下统计）
//   · 预览区用 **bklv 的真实视图**渲染：真编译外观、真控件、真整页视图
//
// 用法：dist/designer 启动后先用仓库自带的 default 主题浏览（只读），
// 要动手改就点「新建项目」：模板新建一份自己的主题，或直接编辑已有自建主题。

#include <algorithm>
#include <cstdlib>
#include <string>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/bklv.hpp"
#include "designer/core/image_diff.hpp"
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

    // 上次打开的项目（设计器自己的小记录，放在可执行文件目录）
    if (std::getenv("BKLV_DESIGNER_FORK") == nullptr && std::getenv("BKLV_DESIGNER_PROJECT") == nullptr)
    {
        designer::Project remembered;
        std::string rememberError;
        const std::string record = bklv::resourcePath("designer_project.json");
        if (designer::loadProjectFile(record, remembered, rememberError)
            && designer::directoryExists(remembered.themeDir()))
        {
            initial  = remembered;
            readOnly = designer::isBaseTheme(remembered.name);
        }
    }

    std::string error;
    if (!window.setProject(initial, readOnly, error))
        BKLV_LOG_WARNING("初始主题装载失败：%s（可以点「新建项目」换一个目录）", error.c_str());

    // 自查：BKLV_DESIGNER_FILE=<外观名> / SELECT=<file|box|layer<N>|state:名> / VIEW=<views 里的文件名>
    bool filePicked = false;
    if (const char* wantedFile = std::getenv("BKLV_DESIGNER_FILE"))
    {
        std::string openError;
        window.openAppearance(wantedFile, openError);
        filePicked = true;
    }
    if (const char* wantedView = std::getenv("BKLV_DESIGNER_VIEW"))
    {
        window.openViewFile(wantedView);
        filePicked = true;
    }
    if (const char* wantedTable = std::getenv("BKLV_DESIGNER_TABLE"))
    {
        window.openTableFile(wantedTable);
        filePicked = true; // 否则后面的"恢复上次文件"会把它顶掉（截图时踩到）
    }
    if (const char* newAppearance = std::getenv("BKLV_DESIGNER_NEW_APPEARANCE"))
    {
        std::string createError;
        if (!window.createAppearanceFile(newAppearance, "button", createError))
            BKLV_LOG_WARNING("新建外观失败：%s", createError.c_str());
        else
            filePicked = true;
    }
    if (const char* wantedSelect = std::getenv("BKLV_DESIGNER_SELECT"))
        window.selectBySpec(wantedSelect);

    // 自查：BKLV_DESIGNER_FORCE_SAVE=1 直接触发一次保存（用来验证只读保护真的拦得住）
    // 没有显式指定文件时，恢复上次在看的那个（状态里存着）
    if (!filePicked)
        window.restoreLastFileIfAny();

    if (std::getenv("BKLV_DESIGNER_LOG") != nullptr || std::getenv("BKLV_DESIGNER_FOCUS") != nullptr)
        window.setDebugPanelsForTest(std::getenv("BKLV_DESIGNER_LOG") != nullptr,
            std::getenv("BKLV_DESIGNER_FOCUS") != nullptr);

    // 自查：BKLV_DESIGNER_VARIANT=<新名> 把当前外观另存为变体
    if (const char* variant = std::getenv("BKLV_DESIGNER_VARIANT"))
    {
        std::string variantError;
        if (!window.saveAsNewAppearance(variant, variantError))
            BKLV_LOG_WARNING("另存为变体失败：%s", variantError.c_str());
    }

    // 自查：BKLV_DESIGNER_EDIT="height=64" 先改一个 box 数值
    if (const char* edit = std::getenv("BKLV_DESIGNER_EDIT"))
    {
        const std::string spec(edit);
        const size_t equals = spec.find('=');
        if (equals != std::string::npos)
            window.applyTestEdit(spec.substr(0, equals), std::atof(spec.c_str() + equals + 1));
    }

    // 自查：BKLV_DESIGNER_EDIT_TEXT=<JSON> 改页面视图的文本缓冲（不点「应用」）
    if (const char* editText = std::getenv("BKLV_DESIGNER_EDIT_TEXT"))
        window.applyTestTextEdit(editText);

    // 自查：BKLV_DESIGNER_OPEN_DIALOG=<目录> 直接打开"打开项目"浏览器
    if (const char* browse = std::getenv("BKLV_DESIGNER_OPEN_DIALOG"))
        window.openProjectDialogForTest(browse);

    // 钩子的顺序是有讲究的（踩过）：先把编辑做完，再撤销/重做，最后才存盘/截图。
    // 之前 EDIT_DOC 排在 UNDO 后面，结果"撤销"时那次编辑还没发生，
    // 验证出来的撤销说明少了一条 —— 日志顺序一眼能看出来。
    // 自查：BKLV_DESIGNER_EDIT_DOC=a.b.c=<JSON 值>
    if (const char* docEdit = std::getenv("BKLV_DESIGNER_EDIT_DOC"))
    {
        const std::string spec(docEdit);
        const size_t equals = spec.find('=');
        if (equals != std::string::npos)
        {
            const bool ok = window.applyTestDocEdit(spec.substr(0, equals), spec.substr(equals + 1));
            BKLV_LOG_INFO("[自查] EDIT_DOC %s -> %s", docEdit, ok ? "成功" : "失败");
        }
    }

    // 自查：BKLV_DESIGNER_UNDO=1 撤销一步
    if (std::getenv("BKLV_DESIGNER_UNDO") != nullptr)
    {
        window.undoForTest();
        BKLV_LOG_INFO("[自查] 撤销后状态：%s", window.statusForTest().c_str());
    }

    // 自查：BKLV_DESIGNER_REDO=1 重做一步（放在 UNDO 之后验证重做链没被清掉）
    if (std::getenv("BKLV_DESIGNER_REDO") != nullptr)
    {
        window.redoForTest();
        BKLV_LOG_INFO("[自查] 重做后状态：%s", window.statusForTest().c_str());
    }

    // 自查：BKLV_DESIGNER_SAVE_ALL=1 走一次"全部保存"
    // 自查：BKLV_DESIGNER_SYNC_FIELD=<字段名>（把选中层的该字段套到所有层）
    if (const char* field = std::getenv("BKLV_DESIGNER_SYNC_FIELD"))
        BKLV_LOG_INFO("[自查] 套字段 %s -> 改了 %d 个层", field,
            window.syncFieldForTest(field, window.selectionIndexForTest()));

    // 自查：BKLV_DESIGNER_COPY_LAYER=<下标> + BKLV_DESIGNER_PASTE_LAYER=1（跨外观粘层）
    if (const char* copyIndex = std::getenv("BKLV_DESIGNER_COPY_LAYER"))
        window.copyLayerForTest(std::atoi(copyIndex));
    if (std::getenv("BKLV_DESIGNER_PASTE_LAYER") != nullptr)
        BKLV_LOG_INFO("[自查] 粘贴层 -> 下标 %d", window.pasteLayerForTest());

    // 自查：BKLV_DESIGNER_NEXT_CHANGE=<次数> 连点几次"下一处改动"
    if (const char* count = std::getenv("BKLV_DESIGNER_NEXT_CHANGE"))
    {
        for (int i = 0; i < std::atoi(count); i++)
        {
            window.nextChangeForTest(1);
            BKLV_LOG_INFO("[自查] 跳改动 %d：%s", i + 1, window.statusForTest().c_str());
        }
    }

    // 自查：BKLV_DESIGNER_SHOW_DIFF=1 直接打开保存前的 diff 弹窗
    if (std::getenv("BKLV_DESIGNER_SHOW_DIFF") != nullptr)
        window.requestSaveForTest();

    if (std::getenv("BKLV_DESIGNER_SAVE_ALL") != nullptr)
        window.saveAllForTest();

    if (std::getenv("BKLV_DESIGNER_FORCE_SAVE") != nullptr)
        window.forceSaveForTest();

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
    // 窗口尺寸：默认 1280×820；BKLV_DESIGNER_WINDOW=宽x高 可以指定
    // （小屏 / 分屏下需要，也是自查"不同窗口尺寸下布局是否还成立"的手段）
    int windowWidth  = 1280;
    int windowHeight = 820;
    if (const char* size = std::getenv("BKLV_DESIGNER_WINDOW"))
    {
        const std::string spec(size);
        const size_t cross = spec.find('x');
        if (cross != std::string::npos)
        {
            windowWidth  = std::max(720, std::atoi(spec.substr(0, cross).c_str()));
            windowHeight = std::max(480, std::atoi(spec.substr(cross + 1).c_str()));
        }
    }

    const int exitCode = app.run("bklv 设计器", windowWidth, windowHeight);

    // 截图基线对比：BKLV_DESIGNER_BASELINE=<png> 时把刚截的图和基线比一比，
    // 差异大了就返回非 0（可以当界面回归用）。差异图写在 <shot>.diff.png。
    if (const char* baseline = std::getenv("BKLV_DESIGNER_BASELINE"))
    {
        const char* shot = std::getenv("BKLV_DESIGNER_SHOT");
        if (shot != nullptr)
        {
            std::string error;
            const designer::ImageDiff diff
                = designer::compareImages(baseline, shot, error);
            if (!error.empty())
                BKLV_LOG_ERROR("基线对比失败：%s", error.c_str());
            else if (!diff.sameSize)
            {
                BKLV_LOG_ERROR("基线对比：尺寸不同（基线 %dx%d，当前 %dx%d）", diff.width,
                    diff.height, diff.otherWidth, diff.otherHeight);
                return 2;
            }
            else
            {
                std::string writeError;
                designer::writeDiffImage(std::string(shot) + ".diff.png", baseline, shot, writeError);
                BKLV_LOG_INFO("基线对比：平均差 %.2f/255，最大 %d，明显不同的像素 %.2f%%",
                    diff.meanDiff, diff.maxDiff, diff.changedRatio * 100.0f);
                return diff.changedRatio > 0.02f ? 1 : 0; // 超过 2% 像素算"界面变了"
            }
        }
    }
    return exitCode;
}

#pragma once

// 设计器外壳（P2）：打开主题目录 → 列文件 → 看 JSON → 状态栏。
// 界面本体用 ImGui（信息密集），状态栏/标题用库的视图（App 根视图），两套共用一份状态。

#include <string>
#include <vector>

#include "bklv/core/app.hpp"
#include "designer/core/appearance_doc.hpp"
#include "designer/core/layer_fields.hpp"
#include "designer/core/undo_stack.hpp"

namespace bklv { class Header; }

namespace designer
{

struct FileEntry
{
    std::string relPath; // 相对主题根，如 "widget/button.text"
    std::string kind;    // base / widget / view / token
};

class DesignerApp
{
  public:
    // argv 解析：--theme <dir> / --screenshot <png> / --frames <n>
    struct Options
    {
        std::string themeDir = "resources/theme/default";
        std::string themeName = "default";
        std::string screenshotPath;
        int         frames = 3;
        std::string initialFile; // --file：启动就直接打开这个文件（自动化验证用）
    };

    bool init(const Options& options, std::string& error);

    bklv::App& getApp() { return this->app; }
    // 每帧：刷新清单/状态（ImGui 面板在 onDrawWindows 里画）
    void update();

  private:
    void scanTheme();
    void drawPanels();
    void drawMenuBar();
    void drawFileList();
    void drawJsonView();
    void drawInspector();
    void saveCurrent();
    void applyFieldEdit(const std::string& key, const designer::Json& value);
    void drawStatusBar();
    bool openFile(const std::string& relPath);

    bklv::App        app;
    Options          options;
    std::vector<FileEntry> files;
    std::string      currentFile;
    std::string      currentText;
    std::string      lastError;
    std::string      themeTitle;
    int              selectedIndex = -1;
    float            fileFilterWidth = 320.0f;
    char             filter[128] = {0};
    bklv::Header*    header = nullptr; // 根视图（库的控件）：标题 + 状态副标题
    AppearanceDoc    appearance;      // 当前打开的外观文件（P3 只编辑 base/widget）
    bool             appearanceLoaded = false;
    int              selectedLayer = -1;
    UndoStack        undo;
    std::string      saveStatus;
    long long        fileMtimeAtLoad = 0;
    bool             lastErrorEnabled = false;
};

} // namespace designer

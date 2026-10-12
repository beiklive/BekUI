#pragma once

// 设计器界面：本体用 ImGui 控件画，预览用 bklv 的真实视图渲染。
//
// 四块（照样板房）：
//   左上 文件列表（本项目主题的外观 + 页面视图 + 主题表）
//   左   节点树（文件 → box / 每一层 / states → 每状态 → 每通道）
//   左下 节点数 / 当前路径
//   中   预览区（选中什么渲染什么：控件外观 / box / 单层 / 状态 / 整页视图）
//   右   属性表（按节点类型给表单，改完立刻重建预览）

#include <string>
#include <vector>

#include "bklv/bklv.hpp"
#include "designer/core/appearance_doc.hpp"
#include "designer/core/project.hpp"

namespace designer
{

class DesignerWindow
{
  public:
    // 把窗口挂到 App 的 onDrawWindows 上（视图树画完、ImGui::Render() 之前）
    void attach(bklv::App* app);

    // 打开项目：按项目目录装载主题并刷新文件清单
    bool setProject(const Project& project, bool readOnly, std::string& error);
    const Project& getProject() const { return this->project; }

    // 打开某个外观文件（按短名，如 "button.primary"）
    bool openAppearance(const std::string& widgetName, std::string& error);
    // 打开 resources/views 下的某个页面视图
    bool openViewFile(const std::string& fileName);
    // 按规格选中节点："file" / "box" / "layer0" / "state:focused"（自查与脚本用）
    bool selectBySpec(const std::string& spec);

    void draw();
    bool hasUnsavedChanges() const { return this->dirty; }

  private:
    enum class SelKind
    {
        FILE,
        BOX,
        LAYER,       // 选中一层
        LAYER_FIELD, // 选中层里的某个字段（index = 层号，key = 字段名）
        STATE,       // 选中一个状态
        CHANNEL,     // 选中状态里的一个通道（state + index）
        VIEW,        // 页面视图文件
        TABLE,       // 主题表（只读）
    };

    struct Selection
    {
        SelKind kind = SelKind::FILE;
        int index    = -1;
        std::string key;   // LAYER_FIELD 的字段名 / 文件名
        std::string state; // STATE / CHANNEL 的状态名
    };

    // ---- 各块 ----
    void drawTopBar();
    void drawFileList();
    void drawTree();
    void drawStatus();
    void drawPreview();
    void drawProps();
    void drawModals();

    // ---- 数据 ----
    void refreshFileList();
    bool loadCurrentFile(std::string& error);
    void rebuildPreview();
    void markEdited();
    void applyThemeFromProject(std::string& error);

    // ---- 预览 ----
    bklv::AppearanceView* createPreviewControl() const; // 按外观名挑真实控件，兜底裸 AppearanceView
    std::string previewJson() const;                    // 按选中节点生成"给预览用的外观 JSON"
    void drawBoxGuides(ImVec2 origin, ImVec2 size) const;
    void ensurePreviewBuilt();

    // ---- 保存 ----
    void requestSave();
    void doSave();
    void revert();

    bklv::App* app = nullptr;

    Project project;
    bool hasProject = false;
    bool readOnly    = false;

    std::vector<std::string> widgetFiles; // "widgets/button.json" 这种相对路径
    std::vector<std::string> viewFiles;   // "views/page_json.json"
    std::string currentWidget;            // 短名，空 = 没开外观文件
    std::string currentViewFile;
    std::string currentFilePath;          // 绝对路径
    std::string savedText;                // 上次保存/打开时的原文（diff 与"还原"用）

    AppearanceDoc doc;
    Selection selection;

    bool dirty          = false;
    bool previewDirty   = true;
    std::string statusMessage;
    std::string errorMessage;

    // 预览
    bklv::View* previewView = nullptr;
    std::string previewError;
    int canvasWidth  = 420;
    int canvasHeight = 160;
    int canvasZoom   = 1;

    // 弹层
    bool showNewProject = false;
    bool showDiff       = false;
    bool showSaveAs     = false;
    std::vector<std::string> diffLines;

    // 新建项目表单
    int newProjectMode = 0; // 0 = 模板新建，1 = 直接编辑
    char newTemplate[64] = "default";
    char newName[64]     = "my-skin";
    char themeRoot[512]  = "";
};

} // namespace designer

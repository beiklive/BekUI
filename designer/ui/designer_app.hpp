#pragma once

// 设计器外壳（P2）：打开主题目录 → 列文件 → 看 JSON → 状态栏。
// 界面本体用 ImGui（信息密集），状态栏/标题用库的视图（App 根视图），两套共用一份状态。

#include <string>
#include <vector>

#include "bklv/core/app.hpp"
#include "bklv/core/resource.hpp"
#include "designer/core/appearance_doc.hpp"
#include "designer/core/layer_fields.hpp"
#include "designer/core/tokens_doc.hpp"
#include "designer/core/view_doc.hpp"
#include "designer/core/autosave.hpp"
#include "designer/core/undo_stack.hpp"

namespace bklv { class Header; class View; class AppearanceView; }

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
        bool        startOnSource = false; // --source：启动就切到源码页签
        float       clickX = -1.0f;        // --click x,y：启动后模拟一次点击（自动化验证点选）
        float       clickY = -1.0f;
        bool        openPalette = false; // --palette：启动就打开快速打开（自动化验证）
        float       zoom = 1.0f;         // --zoom：画布缩放（自动化验证）
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
    void applyToCanvas();
    void handleCanvasClick();
    void drawSourceTab();
    void drawTokenTable();
    void rebuildViewPreview();
    void drawViewPanel();
    // 画布点击 → 选中节点（预览树与文档按同一 DFS 序对齐；数量对不上就不选，宁可不动）
    void selectNodeAt(float screenX, float screenY);
    void maybeAutosave();
    void drawRecoveryPopup();
    void drawQuickOpen();
    void drawOpenTheme();
    void openThemeDir(const std::string& dir, const std::string& name);
    std::vector<std::string> listThemes() const;
    void drawDiagnostics();
    void drawLayerOps();
    // 操作本体与 UI 分开：按钮与快捷键走同一份实现（快捷键只在 update() 里调这些，
    // 不能调 drawLayerOps —— 那是界面代码，不能在没有窗口上下文时执行）
    void addLayerOp(const std::string& op);
    void duplicateSelected();
    void deleteSelected();
    void moveSelected(int delta);
    void renameSelected();
    void confirmFileSwitch(const std::string& relPath);
    void drawPendingSwitchPopup();
    void applySource();
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
    float            fileFilterWidth = 300.0f;
    char             filter[128] = {0};
    AppearanceDoc    appearance;      // 当前打开的外观文件（P3 只编辑 base/widget）
    bool             appearanceLoaded = false;
    int              selectedLayer = -1;
    UndoStack        undo;
    std::string      saveStatus;
    long long        fileMtimeAtLoad = 0;
    bklv::View*      canvasHost = nullptr; // 中间那块画布（库的视图树负责画）
    bklv::AppearanceView* canvasView = nullptr;
    float            inspectorWidth = 380.0f;
    std::vector<char> sourceBuffer; // 源码页签的编辑缓冲（InputText 需要稳定内存）
    std::string       sourceError;  // 上次"应用"失败的原因（带行列）
    std::string       sourceStatus; // 上次"应用"成功提示
    int               inspectorTab = 0; // 0 = 属性，1 = 源码
    TokensDoc         tokens;
    bool              tokensLoaded = false;
    char              tokenFilter[64] = {0};
    std::vector<TokenRef> tokenRefs; // 当前选中 token 的引用（点"查引用"才扫）
    std::string       pendingFile;   // 有未保存改动时等待确认切换的目标文件
    char              renameBuffer[128] = {0};
    bool              wantRename = false;
    ViewDoc           viewDoc;
    bool              viewLoaded = false;
    bklv::View*       viewPreview = nullptr; // 视图文件的画布预览（与外观预览互斥）
    int               selectedNode = 0;
    std::vector<bklv::View*> previewOrder; // 预览树的 DFS 序（与 viewDoc.listNodes 对齐）
    bool              pendingClick = false;
    int               clickDelayFrames = 0;
    bool              wantPalette = false;
    bool              paletteVisible = false;
    char              paletteFilter[64] = {0};
    int               paletteIndex = 0;
    std::string       lastListGroup;   // 左栏分组节头（只在分组变化时画一次）
    bool              groupOpen[4] = { true, true, true, true }; // base / widget / view / tokens
    std::vector<std::string> diagnostics; // 最近的错误（去重后倒序显示）
    bool              wantOpenTheme = false;
    bool              openThemeVisible = false;
    char              themePathBuffer[512] = {0};
    float             canvasZoom = 1.0f;
    float             appliedZoom = -1.0f; // 上次套用的缩放（避免每帧重设尺寸）
    void applyCanvasZoom();
    std::string       lastSeenError;
    long long         lastAutosaveMs = 0;
    AutosaveInfo      pendingDraft;   // 启动时发现的未保存草稿
    bool              hasPendingDraft = false;
};

} // namespace designer

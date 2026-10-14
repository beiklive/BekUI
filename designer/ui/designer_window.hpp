#pragma once

// 设计器界面：本体用 ImGui 控件画，预览用 bklv 的真实视图渲染。
//
// 四块（照样板房）：
//   左上 文件列表（本项目主题的外观 + 页面视图 + 主题表）
//   左   节点树（文件 → box / 每一层 / states → 每状态 → 每通道）
//   左下 节点数 / 当前路径
//   中   预览区（选中什么渲染什么：控件外观 / box / 单层 / 状态 / 整页视图）
//   右   属性表（按节点类型给表单，改完立刻重建预览）

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bklv/bklv.hpp"
#include "designer/core/appearance_doc.hpp"
#include "designer/core/file_browser.hpp"
#include "designer/core/json_cache.hpp"
#include "designer/core/undo_stack.hpp"
#include "designer/core/project.hpp"

namespace designer
{

// 一份"打开的文件的编辑状态"。
// 多文件同时改：切文件时把当前这份存进 drafts，切回来接着改（不再丢改动）。
struct Draft
{
    AppearanceDoc doc;
    std::string savedText;              // 上次保存/打开时的原文（diff 与"还原"用）
    std::string openHash;               // 打开时的文件指纹（检测被外部改动）
    std::string viewText;               // 页面视图的文本编辑缓冲
    UndoStack history;                  // 撤销栈（整份 JSON 快照，core 里可测）
    bool dirty = false;
};

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
    // 打开主题表（colors.json / metrics.json）——可编辑
    bool openTableFile(const std::string& fileName);
    // 新建 / 删除外观文件（会同步 theme.json 的 widgets 清单）
    bool createAppearanceFile(const std::string& name, const std::string& templateName,
        std::string& error);
    // 把当前外观另存为一个新外观（不变体继承，就是复制一份 + 登记），用来做变体
    bool saveAsNewAppearance(const std::string& name, std::string& error);
    bool deleteCurrentAppearance(std::string& error);
    // 按规格选中节点："file" / "box" / "layer0" / "state:focused"（自查与脚本用）
    bool selectBySpec(const std::string& spec);

    void draw();
    ~DesignerWindow() { this->saveUiState(); }
    // 自查用：直接走一遍保存（验证只读拦截 / 冲突检测）
    // 跳过 diff 弹层直接写（自查用；readOnly / 冲突 / 编译校验 / 增量写盘都照走）
    // 自查用：按点分路径改当前文档里的一个值（比如 common.red = "#FF0000"）
    bool applyTestDocEdit(const std::string& dottedPath, const std::string& jsonValue)
    {
        AppearanceDoc::Json* node = &this->doc.root();
        size_t pos                = 0;
        while (pos < dottedPath.size())
        {
            const size_t dot      = dottedPath.find('.', pos);
            std::string key       = dottedPath.substr(pos,
                dot == std::string::npos ? std::string::npos : dot - pos);
            // 支持 `layers[0]` 这种带下标的段（写成 `[` 就当场拒绝，别把文件写坏）
            int index = -1;
            if (const size_t bracket = key.find('['); bracket != std::string::npos)
            {
                if (key.back() != ']')
                    return false;
                index = std::atoi(key.substr(bracket + 1, key.size() - bracket - 2).c_str());
                key   = key.substr(0, bracket);
            }
            if (key.empty())
                return false;

            if (dot == std::string::npos)
            {
                AppearanceDoc::Json parsed
                    = AppearanceDoc::Json::parse(jsonValue, nullptr, false);
                AppearanceDoc::Json& target = index < 0 ? (*node)[key] : (*node)[key][index];
                target = parsed.is_discarded() ? AppearanceDoc::Json(jsonValue) : parsed;
                this->pushUndoSnapshot();
                this->markEdited();
                this->previewDirty = true;
                return true;
            }
            AppearanceDoc::Json& next = (*node)[key];
            if (index < 0)
            {
                if (!next.is_object())
                    next = AppearanceDoc::Json::object();
                node = &next;
            }
            else
            {
                if (!next.is_array() || index >= (int)next.size())
                    return false;
                node = &next[index];
            }
            pos = dot + 1;
        }
        return false;
    }

    void saveAllForTest() { this->saveAllDirty(); }
    void requestSaveForTest() { this->requestSave(); }
    void nextChangeForTest(int step) { this->selectNextChange(step); }
    void copyLayerForTest(int index)
    {
        if (const AppearanceDoc::Json* layer = this->doc.layer(index))
        {
            this->layerClipboard    = *layer;
            this->hasLayerClipboard = true;
        }
    }
    int syncFieldForTest(const std::string& field, int sourceIndex)
    {
        std::vector<int> targets;
        for (int i = 0; i < this->doc.layerCount(); i++)
            targets.push_back(i);
        const int changed = this->doc.applyFieldToLayers(sourceIndex, field, targets);
        if (changed > 0)
        {
            this->pushUndoSnapshot();
            this->markEdited();
        }
        return changed;
    }
    int pasteLayerForTest()
    {
        const int index = this->doc.appendLayer(this->layerClipboard);
        if (index >= 0)
        {
            this->pushUndoSnapshot();
            this->markEdited();
        }
        return index;
    }
    void openProjectDialogForTest(const std::string& startPath)
    {
        this->showOpenProject = true;
        this->openBrowseAt(startPath);
    }

    void undoForTest() { this->undo(); }
    const std::string& statusForTest() const { return this->statusMessage; }
    // 注意：Selection 是类里**稍后**才声明的嵌套类型，成员函数声明里用不了它
    // （C++ 的类内名字查找要求先声明）—— 所以这里只暴露 int 层面的访问器
    int selectionIndexForTest() const;
    // 拖分隔条用的宽度（点一下分隔条可以复位到默认）
    static constexpr int kLeftWidthDefault  = 320;
    static constexpr int kRightWidthDefault = 420;
    static constexpr int kFileListDefault   = 250;
    void redoForTest() { this->redo(); }
    void forceSaveForTest()
    {
        this->doSave();
        BKLV_LOG_INFO("[自查] 保存结果：%s%s", this->statusMessage.c_str(),
            this->errorMessage.empty() ? "" : (" / " + this->errorMessage).c_str());
    }
    // 自查用：改页面视图的**文本缓冲**但不点「应用」（验证"保存时自动应用"）
    bool applyTestTextEdit(const std::string& text)
    {
        if (this->currentFilePath.empty())
            return false;
        std::snprintf(this->viewText, sizeof(this->viewText), "%s", text.c_str());
        this->markEdited();
        return true;
    }

    // 自查用：改一个 box 数值（验证增量写盘 / 撤销）
    bool applyTestEdit(const std::string& key, double value)
    {
        if (this->currentFilePath.empty())
            return false;
        this->doc.setBoxNumber(key, value);
        // 钩子在**任何一帧之前**跑，帧级快照还没产生 —— 自己补一步（顺序跟帧边界一致：
        // 先改、再把改后的状态压栈；压改前会被去重逻辑跳过，撤销就没东西可撤）
        this->pushUndoSnapshot();
        this->markEdited();
        this->previewDirty = true;
        return true;
    }
    // 启动时恢复"上次在看的文件"（没有环境钩子时用）
    bool restoreLastFileIfAny();
    void loadUiState();
    void saveUiState();

    // 设计器自己的日志面板（右上角，可拖可缩放；读 bklv::Logger 的环形缓冲）
    void drawLogPanel();

    void setDebugPanelsForTest(bool log, bool focusOrder)
    {
        if (log)
            this->showLogPanel = !this->showLogPanel;
        this->highlightFocusOrder = focusOrder;
    }
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
    // 页面视图的节点树（type + id + 属性 + children 逐层展开）
    void drawViewTree(const AppearanceDoc::Json& node, const std::string& path);
    void refreshThemes();
    void switchTheme(const std::string& name);
    // 选中节点在预览里框出来 / 点预览反选节点
    void drawViewHighlight(ImVec2 origin, float zoom);
    int applyBindingSamples();   // 把示例数据套到绑定的视图上，返回套用条数
    void handlePreviewClick(ImVec2 origin, float zoom);
    void drawStatus();
    void drawPreview();
    void drawPreviewCanvas(ImVec2 origin, ImVec2 size, bklv::View* view, bool checker);
    bklv::View* buildPreviewView(const std::string& jsonOverride);
    void drawProps();
    void drawTableProps();
    bool drawEnumCombo(const char* id, const std::string& key, std::string& value);          // 主题表：键列表 + 颜色/数值编辑
    void drawAddFieldCombo(const char* id, const std::vector<std::string>& candidates,
        const std::function<void(const std::string&)>& onPick);
    bool colorPickerPopup(const char* id, std::string& value); // 取色器（写回字面量）
    void drawModals();
    void drawMenuBar();   // 全局菜单：文件 / 编辑 / 视图 / 工具 / 帮助
    void refreshBrowse();                    // 重新列当前浏览目录
    void openBrowseAt(const std::string& path);
    void drawOpenProjectModal();

    // ---- 数据 ----
    void refreshFileList();

    // 主题表文件（清单里的 tokens/*.json；老主题退回根下的 colors.json / metrics.json）
    std::vector<std::string> themeTableFiles() const;
    bool loadCurrentFile(std::string& error);
    void rebuildPreview();
    void markEdited();
    void applyThemeFromProject(std::string& error);

    // ---- 预览 ----
    bklv::AppearanceView* createPreviewControl() const; // 按外观名挑真实控件，兜底裸 AppearanceView
    bklv::AppearanceView* createPreviewControlFor(const std::string& name) const;
    std::string lastAppearanceName;   // 最近打开的外观（编辑主题表时拿它当预览对象）
    std::string lastViewFile;         // 最近打开的页面视图（状态持久化 + 启动恢复）
    // 启动时"上次在看的那份"，单独存：setProject 会打开第一个控件并覆盖
    // lastAppearanceName，用同一个字段恢复就永远恢复不到（踩过）
    std::string startupAppearance;
    std::string startupViewFile;
    float uiScale = 1.0f;             // 设计器界面整体缩放（0.7~1.6）
    int fileListHeightUi = 250;       // 左栏里"文件列表"的高度（可拖）
    bool showGrid = false;            // 预览画布的对齐网格 + 中线
    bool onlyLayers = true;
    bool showLayerStrip = true;   // 绘制顺序条           // 树里只看层（字段归检查器）
    char propFilter[64] = "";         // 属性表字段过滤
    std::vector<DirEntry> recentFiles;// 最近修改（文件列表顶部的快捷入口）
    // 相对"磁盘上那份"变了哪些字段（box.height / layers[0].fill …）
    std::set<std::string> changedFieldPaths;
    std::string changedPathsSource;  // 上次算的时候文档长什么样（省得每帧重算）
    std::string historySummary;      // 最近一次撤销/重做动了哪些字段
    void refreshChangedPaths();
    bool isFieldChanged(const std::string& path) const
    {
        return this->changedFieldPaths.count(path) == 1;
    }
    bool isLayerChanged(int index) const;
    // diff 列表点一下 → 选中对应的地方（层 / 层字段 / box）
    void selectByChangePath(const std::string& path);
    // 在"改动过的字段"里往下/往上跳一处（step = +1 / -1）
    void selectNextChange(int step);
    // 状态栏里的"选中路径"面包屑（可点，点了回到上一层）
    void drawSelectionBreadcrumb();
    // 竖直分隔条：拖它改栏宽，悬停变左右箭头，双击复位。sign=+1 左栏 / -1 右栏
    float drawVerticalSplitter(const char* id, float current, float minWidth, float maxWidth,
        float sign, float defaultWidth);
    // 水平分隔条：拖它改上方面板的高度，双击复位
    float drawHorizontalSplitter(const char* id, float current, float minHeight, float maxHeight,
        float defaultHeight);
    // 打开"把该字段套到其它层"的弹层（默认勾上所有"有或没有这个字段"的层）
    void openFieldSync(const std::string& field)
    {
        this->syncField = field;
        this->syncTargets.assign((size_t)this->doc.layerCount(), 1);
        this->showFieldSync = true;
    }
    std::string previewJson() const;                    // 按选中节点生成"给预览用的外观 JSON"
    void drawBoxGuides(ImVec2 origin, ImVec2 size) const;
    // 色板 + 文本 + 常用主题 key 下拉；返回 true 表示改过
    bool colorField(const char* label, std::string& value);
    // 页面视图的文本改动：校验 + 试实例化，通过才写进 doc
    bool applyViewText(std::string& error);
    void ensurePreviewBuilt();

    // ---- 保存 ----
    bool applyPendingViewText(std::string& error); // 页面视图：文本没点「应用」就自动应用一次
    void requestSave();
    void doSave();
    void revert();

    // ---- 草稿（多文件同时改）----
    Draft& currentDraft();
    void stashCurrent();
    void restoreDraftFor(const std::string& path);
    void switchTo(const std::string& path, const std::string& widgetName,
        const std::string& viewFile); // 带"未保存改动"拦截的切换
    void discardCurrentDraft();

    // ---- 撤销/重做 ----
    void pushUndoSnapshot();
    void undo();
    void redo();

    // ---- 外部改动检测 ----
    std::string hashText(const std::string& text) const;
    bool fileChangedExternally() const;

    // ---- 批量保存 ----
    void saveAllDirty();

    // ---- 其它面板 ----
    void drawDirtyOverview();
    void drawWarningsPanel();

    bklv::App* app = nullptr;

    Project project;
    bool hasProject = false;
    bool readOnly    = false;

    std::vector<std::string> widgetFiles; // 外观：相对主题根的路径（"base/button.json" / "widget/…"）
    std::vector<std::string> tableFiles;  // 主题表：清单里的 "tokens/colors.json" 这类路径
    std::vector<std::string> viewFiles;   // "views/page_json.json"
    std::string currentWidget;            // 短名，空 = 没开外观文件
    std::string currentViewFile;
    std::string currentTable;     // colors.json / metrics.json
    std::vector<std::string> themeNames;   // themes.json 的 available
    std::string defaultThemeName;          // themes.json 的 default
    std::map<std::string, std::vector<std::string>> themeUsage; // key → 哪些外观在用它
    void refreshThemeUsage();
    std::string usagePopupKey;             // "谁在用"弹层里要展示的那个 key
    std::vector<bklv::Binding> pageBindings; // 页面里绑了数据的字段
    char bindingSamples[2048] = "";          // 绑定示例数据编辑缓冲（JSON：字段 → 值）
    std::map<std::string, std::string> bindingSamplesByFile; // 按文件各自保存
    bool applySamplesPending = false;        // 按钮只置这个：真套用在"建完预览之后"
    std::string pendingTheme;                // 切主题时的未保存拦截
    JsonCache includeCache;                  // include 展开用（否则每帧读盘解析）
    int includeDepthGuard = 0;             // include 展开的递归保护
    char tableFilter[64] = "";
    char newAppearanceName[64] = "";
    int newAppearanceTemplate = 0;
    bool showNewAppearance = false;
    bool showNewView       = false;
    bool showSaveAsVariant = false;
    bool showAddFieldForLayer = false;   // 从树右键「增加字段…」打开
    char variantName[64]   = "my_variant";
    char newViewName[64]   = "my_page";
    int newViewTemplate    = 1; // 0=空页面 1=整页（页头+内容+底栏）2=卡片
    bool showDeleteAppearance = false;
    std::string currentFilePath;          // 绝对路径
    std::string savedText;                // 上次保存/打开时的原文（diff 与"还原"用）

    AppearanceDoc doc;
    Selection selection;

    // 多文件草稿：key = 文件绝对路径（当前这份也在里面，切文件时换出换入）
    std::map<std::string, Draft> drafts;
    std::string lastFrameText;   // 帧末快照：下一帧发现不同就压进撤销栈

    // 冲突/未保存拦截
    bool showUnsavedGuard = false;
    bool showConflict     = false;
    bool allowOverwrite   = false;  // 冲突弹层里点过"覆盖写"之后放行一次
    std::string pendingPath;
    std::string pendingWidget;
    std::string pendingViewFile;

    // 焦点顺序（整页预览时列出可聚焦视图的先后）
    std::vector<std::string> focusOrder;
    bool showFocusOrder = false;
    bool highlightFocusOrder = false;

    // 设计器自己的界面状态（栏宽 / 画布 / 开关），跨次运行记住


    // 对比度检查：文字层颜色 vs 它下面的底色
    std::vector<std::string> contrastWarnings;

    // 日志面板：设计器自己是 ImGui 程序，这个面板就读库里的日志环形缓冲。
    // （库侧不再提供 ImGui 版日志窗口了：app 里的日志界面是视图树里的
    //  bklv::DebugLogView，挂在外壳的调试层上。）
    bool showLogPanel    = false;
    bool logAutoScroll   = true;
    bool logShowLocation = true;
    bool logMask[4]      = { true, true, true, true }; // DEBUG / INFO / WARNING / ERROR
    uint64_t logLastVersion = 0;

    // 面板开关
    bool showHelp      = false;
    bool showOverview  = false;
    bool showWarnings  = false;
    bool previewMaximized = false;
    std::vector<std::string> warnings;

    // 搜索
    char fileFilter[64] = "";
    char treeFilter[64] = "";

    // 预览交互
    bool previewContext   = false; // 放进样例容器里看间距/父容器效果
    bool previewSubtree   = false; // 页面视图：只渲染选中子树（否则整页）
    bool previewCompare   = false; // 并排对比：左=编辑中，右=磁盘上的原文件
    int previewStateIndex = 0;     // 0=默认 1=focused 2=pressed 3=disabled 4=selected
    float canvasPanX = 0.0f;
    float canvasPanY = 0.0f;
    int leftWidthUi  = 320;
    int rightWidthUi = 420;
    bool leftCollapsed = false;

    int viewNodeCount   = 0;
    int viewFieldCount  = 0;
    bool dirty          = false;
    bool previewDirty   = true;
    std::string statusMessage;
    std::string errorMessage;

    // 预览
    bklv::View* previewView = nullptr;
    bklv::View* compareView = nullptr;   // 并排对比：磁盘上那份
    std::vector<bklv::InstantiatedView> viewMap; // JSON Pointer ↔ 预览视图（高亮/反选用）
    std::string pendingExpandPath;                // 反选后要展开到的树路径
    std::string previewError;
    int canvasPreset   = 0; // 0=自适应 1=1280x720 2=720x480 3=640x360 4=自定义
    bool canvasChecker = true;
    int canvasWidth    = 420;
    int canvasHeight   = 160;
    int canvasZoom     = 2; // 0=50% 1=75% 2=100% 3=150%（默认 100%）

    // 弹层
    bool showNewProject = false;
    bool showDiff       = false;
    bool showSaveAs     = false;
    std::vector<std::string> diffLines;
    std::vector<std::string> diffPaths;   // 与 diffLines 一一对应，点条目用它定位
    bool diffExpandAll = true;            // diff 弹窗里分组默认展开
    int treeOpenRequest = 0;              // 0 不动 / 1 全展开 / 2 全收起（下一帧生效）
    std::string changeCursorPath;         // "跳到下一处改动"走到哪儿了
    AppearanceDoc::Json layerClipboard;   // 层剪贴板（可跨外观用）
    bool hasLayerClipboard = false;
    // "把该字段套到其它层"的小弹层：字段名 + 各目标层的勾选
    bool showFieldSync    = false;
    std::string syncField;
    std::vector<char> syncTargets;
    char saveAsPath[512] = "";

    // 页面视图：原始 JSON 文本编辑（应用前先校验 + 试实例化）
    char viewText[16384] = "";
    std::string viewTextApplied;

    // 打开项目：文件浏览器（不再手输路径）
    bool showOpenProject   = false;
    int newProjectMode     = 0; // 0 = 模板新建，1 = 直接编辑
    std::string browsePath;     // 当前浏览目录
    std::string browseSelection;// 选中的目录
    std::vector<DirEntry> browseEntries;
    char browsePathInput[512] = "";
    char newThemeName[64]     = "my-skin";
    std::string openProjectError;
};

} // namespace designer

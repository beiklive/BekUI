#include "designer/ui/designer_app.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/core/box.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/core/logger.hpp"
#include "designer/core/save_transaction.hpp"
#include "bklv/core/ui_scale.hpp"
#include "bklv/views/label.hpp"
#include "bklv/widgets/header.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "bklv/widgets/rectangle.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/core/attributes.hpp"
#include "bklv/core/icon_font.hpp"
#include "designer/core/view_fields.hpp"
#include "designer/core/name_table.hpp"
#include "designer/core/autosave.hpp"

namespace designer
{

namespace
{

namespace fs = std::filesystem;

std::string readWhole(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace

namespace
{

// Figma 风格的下划线页签：文字靠左、选中项底部一条强调色线、悬停变亮。
// 返回被点中的下标（没点中返回 -1）。
int underlineTabs(const char* const* labels, int count, int* active)
{
    int       clicked = -1;
    const float u     = bklv::getUiScale();
    const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
    const ImU32 text   = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 muted  = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int i = 0; i < count; i++)
    {
        if (i > 0)
            ImGui::SameLine(0.0f, 18.0f * u);

        ImGui::PushID(i);
        const ImVec2 labelSize = ImGui::CalcTextSize(labels[i]);
        const ImVec2 pos       = ImGui::GetCursorScreenPos();
        const ImVec2 size(labelSize.x + 6.0f * u, labelSize.y + 12.0f * u);
        const bool   hovered   = ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        const bool   isActive  = i == *active;

        if (ImGui::InvisibleButton("##tab", size))
            clicked = i;

        drawList->AddText(ImVec2(pos.x + 3.0f * u, pos.y + 6.0f * u),
            isActive ? text : (hovered ? text : muted), labels[i]);
        if (isActive)
        {
            drawList->AddRectFilled(ImVec2(pos.x + 3.0f * u, pos.y + size.y - 2.0f * u),
                ImVec2(pos.x + size.x - 3.0f * u, pos.y + size.y), accent, 1.0f);
        }
        ImGui::PopID();
    }
    if (clicked >= 0)
        *active = clicked;
    return clicked;
}

} // namespace

namespace
{

// 文件分组的中文说法：`base/` 这种名字只有熟悉主题结构的人才懂
struct GroupText
{
    const char* folder;   // base
    const char* title;    // 基础形状
    const char* hint;     // 单个元素的外观（按钮底、分隔线…）
};
const GroupText kGroups[] = {
    { "base", "基础形状", "单个元素的外观：底、边框、文字、图片" },
    { "widget", "控件样式", "把基础形状拼成的按钮、页头、单元格…" },
    { "view", "页面布局", "整页的结构：有哪些区域、怎么排" },
    { "tokens", "颜色与尺寸", "全局调色板与数值，改了处处都变" },
};
const GroupText* groupText(const std::string& folder)
{
    for (const GroupText& entry : kGroups)
        if (folder == entry.folder)
            return &entry;
    return nullptr;
}

// 视图节点属性的中文名：不翻译的键原样显示（未知键不隐藏）
const char* prettyAttribute(const std::string& key)
{
    static const std::pair<const char*, const char*> kNames[] = {
        { "type", "类型" }, { "axis", "排列方向" }, { "direction", "方向" },
        { "alignItems", "交叉轴对齐" }, { "justifyContent", "主轴对齐" },
        { "flexWrap", "换行" }, { "alignContent", "多行对齐" }, { "gap", "间距" },
        { "width", "宽度" }, { "height", "高度" }, { "text", "文字" },
        { "src", "图片" }, { "icon", "图标" }, { "fontSize", "字号" },
        { "textColor", "文字颜色" }, { "backgroundColor", "背景色" },
        { "cornerRadius", "圆角" }, { "borderThickness", "边框粗细" },
        { "borderColor", "边框颜色" }, { "opacity", "不透明度" },
        { "padding", "内边距" }, { "margin", "外边距" }, { "grow", "可伸展" },
        { "shrink", "可压缩" }, { "scale", "显示比例" }, { "offsetX", "水平偏移" },
        { "offsetY", "垂直偏移" }, { "children", "子节点" }, { "id", "标识" },
        { "include", "引用文件" }, { "scroll", "可滚动" },
    };
    for (const auto& entry : kNames)
        if (key == entry.first)
            return entry.second;
    return nullptr;
}

// 属性行：左边固定宽度的标签列、右边值区占满剩余宽度。
// 以前是"标签一行 + 控件一行"（因为 SetNextItemWidth(-1) 会挤掉标签），信息密度太低。
constexpr float kFieldLabelWidth = 96.0f;

// 图标按钮：Material 字形 + 悬停提示 + 方形点击区（Figma 工具栏的基本单元）
bool iconButton(const char* iconName, const char* tooltip, bool accent = false)
{
    const char* glyph = bklv::icon::glyphByName(iconName);
    if (glyph == nullptr)
        return false;

    const float u    = bklv::getUiScale();
    const float side = 34.0f * u;
    ImFont*     iconFont = bklv::icon::font();
    if (iconFont != nullptr)
        ImGui::PushFont(iconFont);
    if (accent)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_CheckMark));
    const ImVec2 textSize = ImGui::CalcTextSize(glyph);
    const bool   pressed  = ImGui::Button(glyph, ImVec2(side, side));
    if (accent)
        ImGui::PopStyleColor();
    if (iconFont != nullptr)
        ImGui::PopFont();
    (void)textSize;

    if (ImGui::IsItemHovered() && tooltip != nullptr)
        ImGui::SetTooltip("%s", tooltip);
    return pressed;
}

void fieldRowLabel(const char* label)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(kFieldLabelWidth * bklv::getUiScale());
}

} // namespace

bool DesignerApp::init(const Options& options, std::string& error)
{
    this->options = options;

    if (!bklv::loadThemeIntoGlobalFromDirectory(this->options.themeDir, this->options.themeName))
    {
        error = "装载主题失败：" + this->options.themeDir;
        return false;
    }
    this->themeTitle = bklv::getTheme().getName();

    // 根视图就是库的 Header（设计器自己的"外壳"也由框架画）：
    // 标题固定，副标题每帧更新成"主题 · 文件数 · 当前文件 · 错误"。
    // 顶部标题条由设计器自己的 ImGui 顶栏负责（bklv Header 会和它叠字）；
    // 库在这里只负责**画布内容的真实渲染**，这是"设计器用框架"的关键复用。
    auto root = std::make_unique<bklv::Box>(bklv::Axis::COLUMN);

    // 顶部让位：ImGui 顶栏画在 bklv 视图**之上**，不给画布让出这段高度的话，
    // 预览会被顶栏压住（实测：控件顶部 112px 看不见）。
    auto topSpacer = std::make_unique<bklv::Box>(bklv::Axis::COLUMN);
    topSpacer->setHeight(56.0f * bklv::getUiScale());
    root->addView(topSpacer.release());

    // 中间一行：左让位（文件面板）+ 画布 + 右让位（检视器）。
    // 三栏宽度与 ImGui 面板一致（都乘 uiScale），所以画布刚好夹在中间、不会重叠。
    auto row = std::make_unique<bklv::Box>(bklv::Axis::ROW);
    auto leftSpacer = std::make_unique<bklv::Box>(bklv::Axis::COLUMN);
    leftSpacer->setWidth(this->fileFilterWidth * bklv::getUiScale());
    row->addView(leftSpacer.release());

    auto canvasHost = std::make_unique<bklv::Box>(bklv::Axis::COLUMN);
    canvasHost->setGrow(1.0f);
    canvasHost->setBackgroundColor(IM_COL32(0xF2, 0xF2, 0xF7, 0xFF)); // 苹果式浅灰画布底
    // 名字不能给空：空名字会让库打「主题里没有控件外观 ""」的警告，而且画不出一笔
    auto canvas = new bklv::AppearanceView("base/box", false);
    canvas->setGrow(1.0f);                              // 填满画布区的高度
    canvas->setAlignSelf(bklv::AlignSelf::STRETCH);    // 填满宽度
    canvas->setText("示例文字"); // 外观预览给一段样例文字，否则按钮类外观只看得到一块底色
    // 说明：`setDesignGuides` 这条画板边框先不用 —— 截图里它按 draw() 收到的矩形画，
    // 与画布上看到的卡片范围对不上（卡片另有自己的背景层）。选中框不受影响，先用它。

    this->canvasView = canvas;
    // 画板标题：必须由 bklv 画（ImGui 画在它下面，会被画布盖住）
    auto title = new bklv::Label();
    title->setText("");
    title->setFontSize(12.0f);                        // 设计单位，随 uiScale 缩放
    title->setTextColor(IM_COL32(0x9A, 0xA0, 0xAF, 0xFF));
    title->setAlignSelf(bklv::AlignSelf::CENTER);
    canvasHost->addView(title);
    this->canvasTitle = title;

    canvasHost->addView(canvas);
    this->canvasHost = canvasHost.get();
    row->addView(canvasHost.release());

    auto rightSpacer = std::make_unique<bklv::Box>(bklv::Axis::COLUMN);
    rightSpacer->setWidth(this->inspectorWidth * bklv::getUiScale());
    row->addView(rightSpacer.release());
    row->setGrow(1.0f);
    root->addView(row.release());

    this->app.setClearColor(0.95f, 0.95f, 0.97f); // 苹果式浅色底
    this->app.setRootView(root.release());

    if (this->options.zoom > 0.0f)
        this->canvasZoom = this->options.zoom;

    if (this->options.openPalette)
        this->wantPalette = true;

    if (!this->options.initialFile.empty())
        this->welcomeVisible = false; // 命令行直接指定了文件：跳过欢迎页

    if (this->options.clickX >= 0.0f)
        this->pendingClick = true; // 第一帧后再点（此时布局已算完）

    this->app.onDrawWindows = [this]() { this->drawPanels(); };
    this->app.onUpdate       = [this]() {
        // Cmd/Ctrl + S 保存（ImGui 输入在 update 里读没问题：无窗口测试不会走这条路）
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)
            || ImGui::IsKeyChordPressed(ImGuiMod_Super | ImGuiKey_S))
            this->saveCurrent();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_P)
            || ImGui::IsKeyChordPressed(ImGuiMod_Super | ImGuiKey_P))
            this->wantPalette = true;
        this->update();
    };
    this->scanTheme();

    // 上次异常退出留下的草稿？
    AutosaveInfo draft;
    std::string  draftError;
    if (readAutosave(this->options.themeDir, draft, draftError))
    {
        this->pendingDraft    = draft;
        this->hasPendingDraft = true;
    }
    else if (!draftError.empty())
    {
        this->lastError = draftError;
    }
    if (!this->options.initialFile.empty())
    {
        this->openFile(this->options.initialFile);
        if (this->options.startOnSource)
            this->inspectorTab = 1;
    }
    return true;
}

void DesignerApp::scanTheme()
{
    this->files.clear();
    // **必须走资源解析**：`resources/...` 是相对**可执行文件目录**的（resourcePath 负责），
    // 直接用相对 CWD 的路径扫目录，从 Finder / 别的目录启动就是 0 个文件（实测踩到）。
    const fs::path root(bklv::resourcePath(this->options.themeDir));
    std::error_code ec;

    auto addDir = [&](const char* dir, const char* kind) {
        const fs::path path = root / dir;
        if (!fs::exists(path, ec))
            return;
        for (const auto& entry : fs::directory_iterator(path, ec))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;
            std::string rel = dir;
            rel += "/";
            rel += entry.path().stem().string();
            this->files.push_back({ rel, kind });
        }
    };
    addDir("base", "base");
    addDir("widget", "widget");
    addDir("view", "view");
    addDir("tokens", "token");

    std::sort(this->files.begin(), this->files.end(),
        [](const FileEntry& a, const FileEntry& b) { return a.relPath < b.relPath; });
}

bool DesignerApp::openFile(const std::string& relPath)
{
    // 同 scanTheme：`resources/...` 要按**可执行文件目录**解析，不能用相对 CWD 的路径
    const std::string path = bklv::resourcePath(this->options.themeDir + "/" + relPath + ".json");
    std::string       text = readWhole(path);
    if (text.empty())
    {
        this->lastError = "读不到文件：" + path;
        return false;
    }
    this->currentFile = relPath;
    this->currentText = std::move(text);
    this->lastError.clear();
    this->saveStatus.clear();
    this->appearanceLoaded = false;
    this->selectedLayer    = -1;
    this->undo.clear();

    this->tokensLoaded = false;
    this->tokenRefs.clear();
    if (relPath.rfind("tokens/", 0) == 0)
    {
        std::string tokenError;
        if (this->tokens.fromText(this->currentText, tokenError))
            this->tokensLoaded = true;
        else
            this->lastError = tokenError;
    }

    this->viewLoaded = false;
    if (relPath.rfind("view/", 0) == 0)
    {
        std::string viewError;
        if (this->viewDoc.fromText(this->currentText, viewError))
        {
            this->viewLoaded     = true;
            this->updateCanvasTitle();
            this->selectedNode   = 0;
            this->rebuildViewPreview();
        }
        else
            this->lastError = viewError;
    }

    const bool isAppearance = relPath.rfind("base/", 0) == 0 || relPath.rfind("widget/", 0) == 0;
    if (isAppearance)
    {
        std::string parseError;
        if (this->appearance.fromText(this->currentText, parseError))
        {
            this->appearanceLoaded = true;
            this->selectedLayer    = 0; // 直接落到第一层，省一次点击
            this->fileMtimeAtLoad  = fileMtime(path);
            this->currentText      = this->appearance.serialize();
            this->sourceBuffer.assign(this->currentText.begin(), this->currentText.end());
            this->sourceBuffer.push_back('\0');
            this->sourceError.clear();
            this->sourceStatus.clear();
            this->applyToCanvas();
        }
        else
        {
            this->lastError = parseError; // 解析失败仍可只读看源码
        }
    }
    return true;
}

void DesignerApp::saveCurrent()
{
    if (!this->appearanceLoaded || this->currentFile.empty())
        return;

    const std::string text = this->appearance.serialize();
    const std::string path = bklv::resourcePath(this->options.themeDir + "/" + this->currentFile + ".json");

    // 校验先行：外观能编译才允许落盘
    std::string compileError;
    const auto compiled = bklv::Appearance::compile(text, bklv::getTheme(), compileError);
    if (compiled == nullptr)
    {
        this->lastError = "校验失败（未保存）：" + compileError;
        return;
    }

    SaveOptions options;
    options.expectedMtime = this->fileMtimeAtLoad;
    options.validate = [](const std::string& candidate, std::string& why) {
        return bklv::Appearance::compile(candidate, bklv::getTheme(), why) != nullptr;
    };
    std::string saveError;
    const SaveResult result = saveText(path, text, options, saveError);
    if (result == SaveResult::OK)
    {
        this->appearance.markSaved();
        this->fileMtimeAtLoad = fileMtime(path);
        this->undo.clear();
        this->lastError.clear();
        clearAutosave(this->options.themeDir);
        this->saveStatus = "已保存 " + this->currentFile;
    }
    else
    {
        this->lastError = saveError;
    }
}

void DesignerApp::update()
{
    // （顶部状态现在都在 ImGui 顶栏里；bklv Header 已移除）

    // 层快捷键：只在没有输入控件抢键盘时生效；调**操作本体**而不是界面函数
    if (this->appearanceLoaded && !ImGui::GetIO().WantCaptureKeyboard)
    {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D)
            || ImGui::IsKeyChordPressed(ImGuiMod_Super | ImGuiKey_D))
            this->duplicateSelected();
        if (ImGui::IsKeyPressed(ImGuiKey_Delete))
            this->deleteSelected();
        if (ImGui::IsKeyPressed(ImGuiKey_F2))
            this->renameSelected();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z))
        {
            std::string restored;
            if (this->undo.redo(this->appearance.serialize(), restored))
            {
                std::string parseError;
                if (this->appearance.fromText(restored, parseError))
                {
                    this->currentText = this->appearance.serialize();
                    this->applyToCanvas();
                }
            }
        }
        else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
        {
            std::string restored;
            if (this->undo.undo(this->appearance.serialize(), restored))
            {
                std::string parseError;
                if (this->appearance.fromText(restored, parseError))
                {
                    this->currentText = this->appearance.serialize();
                    this->applyToCanvas();
                }
            }
        }
    }

    // 画布缩放：首次布局完成后套一次；改缩放后套一次（不在每帧重设，避免无谓重排）
    if (this->canvasZoom != this->appliedZoom)
    {
        this->applyCanvasZoom();
        this->updateCanvasTitle();
    }

    this->maybeAutosave();
    this->syncCanvasSelection(); // 选中层变化 → 画布选中框（setter 很便宜，每帧设一次即可）

    // 诊断收集：错误换了就记一条（不重复记同一个字符串）
    if (!this->lastError.empty() && this->lastError != this->lastSeenError)
    {
        this->lastSeenError = this->lastError;
        this->diagnostics.insert(this->diagnostics.begin(), this->lastError);
        if (this->diagnostics.size() > 20)
            this->diagnostics.pop_back();
    }
    else if (this->lastError.empty())
    {
        this->lastSeenError.clear();
    }

    // 视图预览：鼠标点击（或 --click 模拟）→ 选中节点
    if (this->viewLoaded && this->viewPreview != nullptr)
    {
        if (this->pendingClick)
        {
            if (this->clickDelayFrames < 5)
                this->clickDelayFrames++;
            else
            {
                this->pendingClick = false;
                this->selectNodeAt(this->options.clickX, this->options.clickY);
            }
        }
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().WantCaptureMouse)
        {
            this->selectNodeAt(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
        }
    }

    if (!this->pendingFile.empty())
        this->drawPendingSwitchPopup();

}

void DesignerApp::drawPanels()
{
    // 样式只能在**第一帧**设：init() 里 ImGui 还没有上下文，GetStyle() 会段错误（实测踩到）
    static bool styleApplied = false;
    if (!styleApplied)
    {
        // 设计器按**桌面程序**的规格排版（不是 720p × uiScale 那套）：
        // 正文约 14pt，边距/行高按 1x 桌面密度；画布仍按 720p 设计基准预览内容。
        styleApplied = true;
        this->applyIosStyle();
    }

    if (this->canvasHost != nullptr)
    {
        static bool welcomeApplied = false;
        if (welcomeApplied != this->welcomeVisible)
        {
            welcomeApplied = this->welcomeVisible;
            this->canvasHost->setVisibility(this->welcomeVisible ? bklv::Visibility::GONE
                                                                 : bklv::Visibility::VISIBLE);
        }
    }
    this->drawWelcome();
    this->drawQuickOpen(); // 面板之前画浮层（它落在右栏空白区，不会被画布盖住）

    const float u     = bklv::getUiScale();
    const float barH  = 56.0f * u;
    const float top   = barH + 8.0f * u;
    const float scale = bklv::getUiScale();
    const float leftW  = this->fileFilterWidth * scale;
    const float rightW = this->inspectorWidth * scale;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float panelH = viewport->WorkSize.y - top - 12.0f;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(leftW, panelH), ImGuiCond_Always);
    ImGui::Begin("##filesPanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::TextDisabled("文件");
    ImGui::Separator();
    this->drawFileList();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - rightW,
                              viewport->WorkPos.y + top),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(rightW, panelH), ImGuiCond_Always);
    ImGui::Begin("##inspectorPanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::TextDisabled("检视器");
    ImGui::Separator();
    // 画布缩放（两种文件都可用）
    ImGui::TextDisabled("画布 %d%%", (int)(this->canvasZoom * 100.0f + 0.5f));
    ImGui::SameLine();
    if (iconButton("zoom_out", "缩小（-25%）"))
    {
        this->canvasZoom = std::max(0.25f, this->canvasZoom - 0.25f);
        this->applyCanvasZoom();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("100%"))
    {
        this->canvasZoom = 1.0f;
        this->applyCanvasZoom();
    }
    ImGui::SameLine();
    if (iconButton("zoom_in", "放大（+25%）"))
    {
        this->canvasZoom = std::min(2.0f, this->canvasZoom + 0.25f);
        this->applyCanvasZoom();
    }
    ImGui::Separator();

    if (this->viewLoaded)
    {
        this->drawViewPanel();
        ImGui::End();
        this->handleCanvasClick();
        return;
    }

    if (this->tokensLoaded)
    {
        this->drawTokenTable();
        ImGui::End();
        this->handleCanvasClick();
        return;
    }

    if (this->appearanceLoaded)
    {
        ImGui::TextDisabled("%s.json%s", this->currentFile.c_str(),
            this->appearance.isDirty() ? "  ●" : "");
        if (!this->lastError.empty())
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", this->lastError.c_str());
        if (!this->saveStatus.empty())
            ImGui::TextDisabled("%s", this->saveStatus.c_str());
        ImGui::Separator();
        static const char* kTabs[] = { "属性", "源码" };
        underlineTabs(kTabs, 2, &this->inspectorTab);
        ImGui::Separator();

        if (this->inspectorTab == 1)
        {
            this->drawSourceTab();
            ImGui::End();
            this->handleCanvasClick();
            return;
        }

        this->drawLayerOps();
        ImGui::BeginChild("##layers", ImVec2(0, 110), true);
        const std::vector<LayerInfo> layers = this->appearance.getLayers();
        for (int i = 0; i < (int)layers.size(); i++)
        {
            char label[160];
            const std::string partName = designer::segmentName(layers[(size_t)i].id);
            std::snprintf(label, sizeof(label), "%s  %s",
                partName.empty() ? layers[(size_t)i].id.c_str() : partName.c_str(),
                designer::opTitle(layers[(size_t)i].op).c_str());
            if (ImGui::Selectable(label, this->selectedLayer == i))
                this->selectedLayer = i;
        }
        ImGui::EndChild();
        ImGui::BeginChild("##fields", ImVec2(0, 0), false);
        this->drawInspector();
        ImGui::EndChild();
    }
    else
    {
        if (this->currentFile.empty())
        {
            ImGui::TextDisabled("还没有选中文件");
            ImGui::TextWrapped("在左边挑一个：先看「基础形状」，改右边的属性，中间会立刻变。");
        }
        else
            ImGui::TextDisabled("%s", this->currentFile.c_str());
        if (!this->lastError.empty())
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", this->lastError.c_str());
        ImGui::Separator();
        ImGui::TextDisabled("非外观文件（P5 起可编辑）");
        ImGui::BeginChild("##raw", ImVec2(0, 0), true);
        ImGui::TextUnformatted(this->currentText.c_str());
        ImGui::EndChild();
    }
    ImGui::End();

    // ---- 顶栏：标题 + 主题 + 动作 ----
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, barH), ImGuiCond_Always);
    ImGui::Begin("##topbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f * u, 8.0f * u));
    ImGui::TextUnformatted("外观设计器");
    ImGui::SameLine();
    ImGui::TextDisabled("%s · %zu 个文件", this->themeTitle.c_str(), this->files.size());
    // 右对齐：按"剩余宽度 - 三个按钮宽度"回推（用 viewport 绝对值会被面板裁掉）
    const float buttonsW = 300.0f * u;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - buttonsW);
    if (iconButton("folder_open", "打开主题…（切换主题目录）"))
        this->wantOpenTheme = true;
    ImGui::SameLine();
    if (iconButton("search", "快速打开（Cmd/Ctrl+P）"))
        this->wantPalette = true;
    ImGui::SameLine();
    const bool dirty = this->appearanceLoaded && this->appearance.isDirty();
    if (iconButton("save", dirty ? "保存（有未保存改动）" : "保存（Cmd/Ctrl+S）", dirty))
        this->saveCurrent();
    ImGui::PopStyleVar();
    ImGui::End();


    this->handleCanvasClick();
}

void DesignerApp::drawMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;
    if (ImGui::BeginMenu("文件"))
    {
        if (ImGui::MenuItem("重新扫描"))
            this->scanTheme();

    // 上次异常退出留下的草稿？
    AutosaveInfo draft;
    std::string  draftError;
    if (readAutosave(this->options.themeDir, draft, draftError))
    {
        this->pendingDraft    = draft;
        this->hasPendingDraft = true;
    }
    else if (!draftError.empty())
    {
        this->lastError = draftError;
    }
        ImGui::Separator();
        ImGui::TextDisabled("主题：%s", this->options.themeDir.c_str());
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("视图"))
    {
        ImGui::TextDisabled("P2：文件浏览与 JSON 只读");
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void DesignerApp::drawFileList()
{
    if (ImGui::Button("重新扫描"))
        this->scanTheme();

    // 上次异常退出留下的草稿？
    AutosaveInfo draft;
    std::string  draftError;
    if (readAutosave(this->options.themeDir, draft, draftError))
    {
        this->pendingDraft    = draft;
        this->hasPendingDraft = true;
    }
    else if (!draftError.empty())
    {
        this->lastError = draftError;
    }
    ImGui::TextDisabled("点名字打开");
    this->drawOpenTheme();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "过滤文件名…", this->filter, sizeof(this->filter));

    ImGui::BeginChild("##list", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.68f), false);
    for (int i = 0; i < (int)this->files.size(); i++)
    {
        const FileEntry& entry = this->files[(size_t)i];
        if (this->filter[0] != '\0'
            && entry.relPath.find(this->filter) == std::string::npos)
            continue;

        const bool selected = (entry.relPath == this->currentFile);
        const size_t      slash = entry.relPath.find('/');
        const std::string group = slash == std::string::npos ? entry.relPath
                                                            : entry.relPath.substr(0, slash);
        if (slash != std::string::npos && group != this->lastListGroup)
        {
            this->lastListGroup = group;
            // Figma 式分组：默认展开，可折叠（折叠后跳过该组的行）
            const int groupIndex
                = group == "base" ? 0 : group == "widget" ? 1 : group == "view" ? 2 : 3;
            const GroupText* text = groupText(group);
            ImGui::Spacing();
            char header[128];
            std::snprintf(header, sizeof(header), "%s (%s)##grp",
                text != nullptr ? text->title : group.c_str(), group.c_str());
            const bool open = ImGui::CollapsingHeader(header,
                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
            this->groupOpen[groupIndex] = open;
            if (open && text != nullptr)
                ImGui::TextDisabled("%s", text->hint);
            if (!open)
                continue;
        }
        else if (slash != std::string::npos)
        {
            const int groupIndex
                = group == "base" ? 0 : group == "widget" ? 1 : group == "view" ? 2 : 3;
            if (!this->groupOpen[groupIndex])
                continue; // 该分组被折叠
        }
        // 中文优先：条目显示中文名，技术名与说明放在悬停提示里
        const std::string title = fileTitle(entry.relPath);
        const std::string hint  = fileHint(entry.relPath);
        const std::string labelText = "  " + (title.empty() ? entry.relPath : title);
        if (ImGui::IsItemHovered())
        {
            if (hint.empty())
                ImGui::SetTooltip("%s.json", entry.relPath.c_str());
            else
                ImGui::SetTooltip("%s\n%s.json", hint.c_str(), entry.relPath.c_str());
        }
        if (ImGui::Selectable(labelText.c_str(), selected))
        {
            this->selectedIndex = i;
            this->confirmFileSwitch(entry.relPath);
            if (!this->lastError.empty())
                ImGui::OpenPopup("打开失败");
        }
    }
    ImGui::EndChild();

    this->drawDiagnostics();

    if (ImGui::BeginPopup("打开失败"))
    {
        ImGui::TextUnformatted(this->lastError.c_str());
        if (ImGui::Button("好"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void DesignerApp::drawTokenTable()
{
    ImGui::TextDisabled("%s%s", this->currentFile.c_str(), this->tokens.isDirty() ? "  ●" : "");
    if (!this->lastError.empty())
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", this->lastError.c_str());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##tokenFilter", "过滤键…", this->tokenFilter, sizeof(this->tokenFilter));
    ImGui::Separator();

    const std::string kind = this->currentFile.find("metrics") != std::string::npos
        ? "metric"
        : "color";
    ImGui::BeginChild("##tokens", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.62f), true);
    for (const TokenEntry& entry : this->tokens.list())
    {
        if (this->tokenFilter[0] != '\0'
            && entry.key.find(this->tokenFilter) == std::string::npos)
            continue;
        ImGui::PushID(entry.key.c_str());
        ImGui::TextUnformatted(entry.key.c_str());
        // 用窗口坐标定位值列（SameLine 的相对量与"剩余宽度"不是一回事，第一版就撞在一起了）
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x * 0.62f);
        ImGui::TextDisabled("%s", entry.value.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("改"))
        {
            std::string buffer = entry.value;
            char        editable[256];
            std::snprintf(editable, sizeof(editable), "%s", buffer.c_str());
            ImGui::OpenPopup("改令牌");
            ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
            if (ImGui::BeginPopup("改令牌"))
            {
                ImGui::TextUnformatted(entry.key.c_str());
                ImGui::InputText("##value", editable, sizeof(editable));
                if (ImGui::Button("写入并重载"))
                {
                    std::string writeError;
                    if (!this->tokens.write(entry.key, editable, writeError))
                        this->lastError = writeError;
                    else
                    {
                        const std::string path
                            = this->options.themeDir + "/" + this->currentFile + ".json";
                        SaveOptions options; // 令牌改动靠"重新装载主题"验证，不再单独编译
                        std::string saveError;
                        if (saveText(path, this->tokens.serialize(), options, saveError)
                            == SaveResult::OK)
                        {
                            this->tokens.markSaved();
                            // 重新装载主题 → 画布/控件立刻拿到新色值
                            std::string loadError;
                            bklv::loadThemeIntoGlobalFromDirectory(
                                this->options.themeDir, this->options.themeName);
                            this->applyToCanvas();
                            this->saveStatus = "已写入 " + entry.key + " 并重载主题";
                        }
                        else
                            this->lastError = saveError;
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("取消"))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("引用"))
        {
            this->tokenRefs = scanThemeReferences(
                this->options.themeDir, kind, entry.key);
            this->saveStatus = entry.key + "：" + std::to_string(this->tokenRefs.size()) + " 处引用";
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled("引用列表（%zu）", this->tokenRefs.size());
    ImGui::BeginChild("##refs", ImVec2(0, 0), true);
    for (const TokenRef& ref : this->tokenRefs)
        ImGui::Text("%s  %s", ref.file.c_str(), ref.pointer.c_str());
    ImGui::EndChild();
}

void DesignerApp::drawDiagnostics()
{
    ImGui::Separator();
    ImGui::BeginChild("##diagnostics", ImVec2(0, 0), true);
    // 标题用 TextDisabled 直接打印（`###` 只在控件 label 里起隐藏 ID 的作用，打印出来会是字面量）
    ImGui::TextDisabled("诊断（%zu）", this->diagnostics.size());
    if (this->diagnostics.empty())
    {
        ImGui::TextDisabled("没有错误。");
    }
    else
    {
        for (const std::string& line : this->diagnostics)
            ImGui::TextWrapped("%s", line.c_str());
        if (ImGui::Button("清空"))
            this->diagnostics.clear();
    }
    ImGui::EndChild();
}

std::vector<std::string> DesignerApp::listThemes() const
{
    std::vector<std::string> out;
    std::error_code          ec;
    const fs::path           root(bklv::resourcePath("resources/theme"));
    if (!fs::exists(root, ec))
        return out;
    for (const auto& entry : fs::directory_iterator(root, ec))
    {
        if (!entry.is_directory())
            continue;
        if (!fs::exists(entry.path() / "theme.json", ec))
            continue;
        out.push_back("resources/theme/" + entry.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

void DesignerApp::openThemeDir(const std::string& dir, const std::string& name)
{
    if (!bklv::loadThemeIntoGlobalFromDirectory(dir, name))
    {
        this->lastError = "装载主题失败：" + dir;
        return;
    }
    this->options.themeDir  = dir;
    this->options.themeName = name;
    this->themeTitle        = bklv::getTheme().getName();
    this->currentFile.clear();
    this->currentText.clear();
    this->appearanceLoaded = false;
    this->viewLoaded       = false;
    this->tokensLoaded     = false;
    this->previewOrder.clear();
    if (this->viewPreview != nullptr)
    {
        this->canvasHost->asBox()->removeView(this->viewPreview, true);
        this->viewPreview = nullptr;
    }
    if (this->canvasView != nullptr)
        this->canvasView->setVisibility(bklv::Visibility::GONE);
    this->scanTheme();
    this->lastError.clear();
}

void DesignerApp::drawOpenTheme()
{
    if (this->wantOpenTheme)
    {
        this->wantOpenTheme    = false;
        this->openThemeVisible = true;
        std::snprintf(this->themePathBuffer, sizeof(this->themePathBuffer), "%s",
            this->options.themeDir.c_str());
    }
    if (!this->openThemeVisible)
        return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float          scale    = bklv::getUiScale();
    const float          rightW   = this->inspectorWidth * scale;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - rightW - 8.0f,
                               viewport->WorkPos.y + 96.0f * scale + 8.0f),
        ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(rightW - 16.0f, 380.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin("打开主题", &this->openThemeVisible,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("内置主题");
    for (const std::string& theme : this->listThemes())
    {
        if (ImGui::Selectable(theme.c_str()))
        {
            this->openThemeDir(theme, fs::path(theme).filename().string());
            this->openThemeVisible = false;
        }
    }
    ImGui::Separator();
    ImGui::TextDisabled("或手输主题目录（相对 resources/ 或绝对路径）");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##themePath", this->themePathBuffer, sizeof(this->themePathBuffer));
    if (ImGui::Button("打开"))
    {
        const std::string dir(this->themePathBuffer);
        if (!dir.empty())
        {
            this->openThemeDir(dir, fs::path(dir).filename().string());
            this->openThemeVisible = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("取消"))
        this->openThemeVisible = false;
    ImGui::End();
}

void DesignerApp::drawQuickOpen()
{
    if (this->wantPalette)
    {
        this->wantPalette     = false;
        this->paletteVisible  = true;
        this->paletteIndex    = 0;
        this->paletteFilter[0] = '\0';
    }
    if (!this->paletteVisible)
        return;

    // **必须放在不画视图的区域**：视图树画在 ImGui 的 foreground draw list 上，
    // 会把屏幕中央的浮层盖住（见 DESIGN.md §10）。右栏是空 Box，安全。
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float          scale    = bklv::getUiScale();
    const float          rightW   = this->inspectorWidth * scale;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - rightW - 8.0f,
                               viewport->WorkPos.y + 96.0f * scale + 8.0f),
        ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(rightW - 16.0f, 340.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin("快速打开（Cmd+P）", &this->paletteVisible,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##paletteFilter", "文件名片段，回车打开", this->paletteFilter,
        sizeof(this->paletteFilter));

    std::vector<int> matches;
    for (int i = 0; i < (int)this->files.size(); i++)
    {
        if (this->paletteFilter[0] != '\0'
            && this->files[(size_t)i].relPath.find(this->paletteFilter) == std::string::npos)
            continue;
        matches.push_back(i);
    }
    if (matches.empty())
        this->paletteIndex = 0;
    else if (this->paletteIndex >= (int)matches.size())
        this->paletteIndex = (int)matches.size() - 1;

    const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
    if (!matches.empty() && ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        this->paletteIndex = (this->paletteIndex + 1) % (int)matches.size();
    if (!matches.empty() && ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        this->paletteIndex = (this->paletteIndex + (int)matches.size() - 1) % (int)matches.size();

    ImGui::BeginChild("##paletteList", ImVec2(0, -32.0f), true);
    for (int i = 0; i < (int)matches.size(); i++)
    {
        const FileEntry& entry = this->files[(size_t)matches[(size_t)i]];
        if (ImGui::Selectable(entry.relPath.c_str(), i == this->paletteIndex))
            this->paletteIndex = i;
    }
    ImGui::EndChild();

    const bool activate = (enter || ImGui::Button("打开")) && !matches.empty();
    if (activate)
    {
        const std::string target = this->files[(size_t)matches[(size_t)this->paletteIndex]].relPath;
        this->paletteVisible = false;
        this->confirmFileSwitch(target);
    }
    ImGui::SameLine();
    if (ImGui::Button("取消"))
        this->paletteVisible = false;
    ImGui::End();
}

void DesignerApp::maybeAutosave()
{
    if (!this->appearanceLoaded || !this->appearance.isDirty())
        return;
    const long long now = (long long)(ImGui::GetTime() * 1000.0);
    if (now - this->lastAutosaveMs < 2000)
        return;
    this->lastAutosaveMs = now;

    std::string error;
    if (!writeAutosave(this->options.themeDir, this->currentFile,
            this->appearance.serialize(), error))
        this->lastError = error;
}

void DesignerApp::drawRecoveryPopup()
{
    if (!this->hasPendingDraft)
        return;

    ImGui::OpenPopup("发现未保存的草稿");
    if (ImGui::BeginPopupModal("发现未保存的草稿", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("上次退出时「%s」还有未保存的改动。", this->pendingDraft.file.c_str());
        if (ImGui::Button("恢复"))
        {
            this->hasPendingDraft = false;
            this->openFile(this->pendingDraft.file);
            if (this->appearanceLoaded && this->appearance.fromText(this->pendingDraft.text,
                                                    this->lastError))
            {
                this->currentText = this->appearance.serialize();
                this->applyToCanvas();
            }
            clearAutosave(this->options.themeDir);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("丢弃"))
        {
            this->hasPendingDraft = false;
            clearAutosave(this->options.themeDir);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DesignerApp::drawSourceTab()
{
    if (!this->sourceError.empty())
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", this->sourceError.c_str());
    else if (!this->sourceStatus.empty())
        ImGui::TextDisabled("%s", this->sourceStatus.c_str());

    if (ImGui::Button("应用（Cmd+Enter）"))
        this->applySource();
    ImGui::SameLine();
    if (ImGui::Button("还原"))
    {
        this->sourceBuffer.assign(this->currentText.begin(), this->currentText.end());
        this->sourceBuffer.push_back('\0');
        this->sourceError.clear();
        this->sourceStatus = "已还原到上次成功的版本";
    }

    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Enter)
        || ImGui::IsKeyChordPressed(ImGuiMod_Super | ImGuiKey_Enter))
        this->applySource();

    ImGui::BeginChild("##source", ImVec2(0, 0), true);
    ImGui::PushFont(nullptr); // 用默认字体；等宽在 P7 统一
    ImGui::InputTextMultiline("##text", this->sourceBuffer.data(), this->sourceBuffer.size(),
        ImVec2(-1, -1));
    ImGui::PopFont();
    ImGui::EndChild();
}

void DesignerApp::applySource()
{
    const std::string text(this->sourceBuffer.data());
    this->lastError.clear();

    if (this->viewLoaded)
    {
        std::string parseError;
        if (!this->viewDoc.fromText(text, parseError))
        {
            this->sourceError = "应用失败（文档未改动）：" + parseError;
            return;
        }
        this->currentText = this->viewDoc.serialize();
        this->sourceError.clear();
        this->sourceStatus = "已应用（未保存）";
        this->rebuildViewPreview();
        return;
    }

    if (!this->appearanceLoaded)
    {
        // 非外观文件：只验证 JSON 语法，不接文档模型
        designer::Json scratch;
        try
        {
            scratch = designer::Json::parse(text, nullptr, true, true);
        }
        catch (const std::exception& e)
        {
            this->sourceError = std::string("语法错误：") + e.what();
            return;
        }
        this->sourceError.clear();
        this->sourceStatus = "语法有效（这个文件类型暂不可编辑，P6 接令牌表）";
        return;
    }

    // 外观：解析失败时**保持旧文档不动**（不丢用户已有编辑），并把行列报出来
    std::string parseError;
    if (!this->appearance.fromText(text, parseError))
    {
        this->sourceError = "应用失败（文档未改动）：" + parseError;
        return;
    }

    this->currentText = this->appearance.serialize();
    this->selectedLayer = this->appearance.getLayers().empty() ? -1 : 0;
    this->sourceError.clear();
    this->sourceStatus = "已应用（未保存）";
    this->applyToCanvas();
}

void DesignerApp::rebuildViewPreview()
{
    if (this->canvasHost == nullptr)
        return;

    // 外观预览与视图预览互斥：同一块画布一次只显示一个
    if (this->canvasView != nullptr)
        this->canvasView->setVisibility(
            this->appearanceLoaded ? bklv::Visibility::VISIBLE : bklv::Visibility::GONE);

    if (this->viewPreview != nullptr)
    {
        this->canvasHost->asBox()->removeView(this->viewPreview, true);
        this->viewPreview = nullptr;
    }
    if (!this->viewLoaded)
        return;

    const auto tmpl = bklv::ViewTemplate::fromString(this->viewDoc.serialize(), this->currentFile);
    if (tmpl == nullptr)
    {
        this->lastError = "视图模板装载失败（语法或结构错误）";
        return;
    }
    bklv::View* preview = tmpl->instantiate();
    if (preview == nullptr)
    {
        this->lastError = "视图实例化失败（结构错误，细节见日志）";
        return;
    }
    preview->setGrow(1.0f);
    preview->setAlignSelf(bklv::AlignSelf::STRETCH);
    this->viewPreview = preview;
    this->canvasHost->asBox()->addView(preview);
    this->lastError.clear();

    // DFS 序（先父后子、子按书写顺序）—— 与 ViewDoc::listNodes 的顺序一致
    this->previewOrder.clear();
    std::function<void(bklv::View*)> walk = [&](bklv::View* node) {
        this->previewOrder.push_back(node);
        if (bklv::Box* box = node != nullptr ? node->asBox() : nullptr)
            for (bklv::View* child : box->getChildren())
                walk(child);
    };
    walk(preview);
}

void DesignerApp::applyCanvasZoom()
{
    // 方案①：按画布区的**像素尺寸**×缩放给预览定尺寸（不用百分比 —— 根 Box 高度 auto 时
    // 百分比拿不到确定值，zoom=1 反而不铺满，这条是上一轮踩过的）
    if (this->canvasHost == nullptr)
        return;
    const float areaW = this->canvasHost->getWidth();
    const float areaH = this->canvasHost->getHeight();
    if (areaW <= 1.0f || areaH <= 1.0f)
        return; // 还没布局

    const float scale = bklv::getUiScale();

    // 外观预览：按"一个控件样例"的尺寸居中显示（铺满整个画布看起来像一块灰板，很丑）
    if (this->canvasView != nullptr)
    {
        this->canvasView->setGrow(0.0f);
        this->canvasView->setAlignSelf(bklv::AlignSelf::CENTER);
        this->canvasView->setWidth(320.0f * scale * this->canvasZoom);
        this->canvasView->setHeight(88.0f * scale * this->canvasZoom);
    }

    // 视图预览：页面类内容按画布区大小 × 缩放
    if (this->viewPreview != nullptr)
    {
        this->viewPreview->setGrow(0.0f);
        this->viewPreview->setAlignSelf(bklv::AlignSelf::CENTER);
        this->viewPreview->setWidth(areaW * this->canvasZoom);
        this->viewPreview->setHeight(areaH * this->canvasZoom);
    }
    this->appliedZoom = this->canvasZoom;
}

void DesignerApp::selectNodeAt(float screenX, float screenY)
{
    if (!this->viewLoaded || this->viewPreview == nullptr || this->previewOrder.empty())
        return;

    const std::vector<ViewNodeInfo> nodes = this->viewDoc.listNodes();
    std::vector<int>                typed;
    for (int i = 0; i < (int)nodes.size(); i++)
        if (!nodes[(size_t)i].type.empty())
            typed.push_back(i);

    if (typed.size() != this->previewOrder.size())
    {
        BKLV_LOG_WARNING("画布点选跳过：节点数对不上（include 展开？）");
        return;
    }

    int bestNode = -1;
    int bestDepth = -1;
    for (size_t i = 0; i < this->previewOrder.size(); i++)
    {
        bklv::View* view = this->previewOrder[i];
        const float x = view->getX();
        const float y = view->getY();
        const float w = view->getWidth();
        const float h = view->getHeight();
        if (w <= 0.0f || h <= 0.0f)
            continue;
        if (screenX < x || screenX > x + w || screenY < y || screenY > y + h)
            continue;
        const int depth = nodes[(size_t)typed[i]].depth;
        if (depth >= bestDepth)
        {
            bestDepth = depth;
            bestNode  = typed[i];
        }
    }
    if (bestNode >= 0)
        this->selectedNode = bestNode;
    else
        BKLV_LOG_INFO("点选：没有命中任何视图");
}

void DesignerApp::drawViewPanel()
{
    ImGui::TextDisabled("%s.json%s", this->currentFile.c_str(),
        this->viewDoc.isDirty() ? "  ●" : "");
    if (!this->lastError.empty())
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", this->lastError.c_str());
    ImGui::Separator();

    static const char* kTabs[] = { "属性", "源码" };
    underlineTabs(kTabs, 2, &this->inspectorTab);
    ImGui::Separator();

    if (this->inspectorTab == 1)
    {
        this->drawSourceTab();
        return;
    }

    ImGui::TextDisabled("节点（点选同步画布预览）");
    ImGui::BeginChild("##nodes", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.45f), true);
    const std::vector<ViewNodeInfo> nodes = this->viewDoc.listNodes();
    for (int i = 0; i < (int)nodes.size(); i++)
    {
        ImGui::PushID(i);
        ImGui::Indent((float)nodes[(size_t)i].depth * 12.0f);
        char label[192];
        std::snprintf(label, sizeof(label), "%s  %s",
            nodes[(size_t)i].type.empty() ? "(无 type)" : nodes[(size_t)i].type.c_str(),
            nodes[(size_t)i].pointer.empty() ? "/" : nodes[(size_t)i].pointer.c_str());
        if (ImGui::Selectable(label, this->selectedNode == i))
            this->selectedNode = i;
        ImGui::Unindent((float)nodes[(size_t)i].depth * 12.0f);
        ImGui::PopID();
    }
    ImGui::EndChild();

    // ---- 选中节点的属性（字段来自控件自己的属性表）----
    if (this->selectedNode < 0 || this->selectedNode >= (int)nodes.size())
        return;
    const ViewNodeInfo& node = nodes[(size_t)this->selectedNode];
    if (node.type.empty())
    {
        ImGui::TextDisabled("这个节点没写 type");
        return;
    }

    ImGui::Separator();
    ImGui::Text("%s   %s", node.type.c_str(), node.pointer.empty() ? "/" : node.pointer.c_str());
    ImGui::BeginChild("##nodeFields", ImVec2(0, 0), false);
    // 资源图片清单（只在有 Image 的 src 字段时才列出）
    static std::vector<std::string> resourceImages;
    if (resourceImages.empty())
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator("resources", ec))
        {
            if (!entry.is_regular_file())
                continue;
            const std::string ext = entry.path().extension().string();
            if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".webp" && ext != ".svg")
                continue;
            std::string rel = entry.path().lexically_relative("resources").string();
            resourceImages.push_back("@res/" + rel);
        }
        std::sort(resourceImages.begin(), resourceImages.end());
    }

    const std::vector<ViewFieldDesc> fields = viewFields(node.type);
    for (const ViewFieldDesc& field : fields)
    {
        const std::string current = this->viewDoc.attributeText(node.pointer, field.key);
        ImGui::PushID(field.key.c_str());

        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%s", current.c_str());
        // 固定标签列 + 右侧值区（原来标签一行、控件一行，太松散）
        fieldRowLabel(prettyAttribute(field.key) != nullptr ? prettyAttribute(field.key)
                                                           : field.key.c_str());
        ImGui::SetNextItemWidth(-1.0f);
        bool committed = false;

        using bklv::AttributeKind;
        switch (field.kind)
        {
            case AttributeKind::BOOL:
            {
                bool value = current == "true";
                if (ImGui::Checkbox("##b", &value))
                {
                    this->viewDoc.setAttribute(node.pointer, field.key, designer::Json(value),
                        this->lastError);
                    committed = true;
                }
                break;
            }
            case AttributeKind::ENUM:
            {
                // allowed 是 "a|b|c"
                std::vector<std::string> options;
                std::string              token;
                for (char c : field.allowed)
                {
                    if (c == '|')
                    {
                        options.push_back(token);
                        token.clear();
                    }
                    else
                        token.push_back(c);
                }
                if (!token.empty())
                    options.push_back(token);
                if (ImGui::BeginCombo("##enum", current.empty() ? "（未设）" : current.c_str()))
                {
                    for (const std::string& option : options)
                    {
                        if (ImGui::Selectable(option.c_str(), option == current))
                        {
                            this->viewDoc.setAttribute(
                                node.pointer, field.key, designer::Json(option), this->lastError);
                            committed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                break;
            }
            default:
                if (ImGui::InputText("##value", buffer, sizeof(buffer),
                        ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    // 数字/布尔按字面量写，其余按字符串（"auto"/"50%"/"@res/…"都要原样保留）
                    designer::Json value;
                    try
                    {
                        value = designer::Json::parse(buffer, nullptr, true, true);
                    }
                    catch (...)
                    {
                        value = designer::Json(std::string(buffer));
                    }
                    this->viewDoc.setAttribute(node.pointer, field.key, std::move(value),
                        this->lastError);
                    committed = true;
                }
                break;
        }

        // `src` 键给一个资源挑选器（列出 resources/ 下的图片）
        if (field.key == "src")
        {
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##res", "从 resources/ 选一张"))
            {
                for (const std::string& candidate : resourceImages)
                {
                    if (ImGui::Selectable(candidate.c_str(), candidate == current))
                    {
                        this->viewDoc.setAttribute(node.pointer, field.key,
                            designer::Json(candidate), this->lastError);
                        committed = true;
                    }
                }
                ImGui::EndCombo();
            }
        }

        // `icon` 键给一排常用的内置图标（@icon/名字）—— 比手打字形可靠
        if (field.key == "icon")
        {
            static const char* kIcons[] = { "star", "settings", "save", "search",
                "add", "check", "close", "menu" };
            for (const char* icon : kIcons)
            {
                if (ImGui::SmallButton(icon))
                {
                    this->viewDoc.setAttribute(node.pointer, field.key,
                        designer::Json(std::string("@icon/") + icon), this->lastError);
                    committed = true;
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }

        if (committed)
        {
            this->currentText = this->viewDoc.serialize();
            this->rebuildViewPreview();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void DesignerApp::applyIosStyle()
{
    const float u = bklv::getUiScale();
    ImGuiIO&    io = ImGui::GetIO();
    io.FontGlobalScale = 0.72f; // 字体图集 20px 栅格 → 约 14pt

    ImGuiStyle& style       = ImGui::GetStyle();
    style.WindowRounding    = 14.0f;
    style.ChildRounding     = 12.0f;
    style.FrameRounding     = 9.0f;
    style.PopupRounding     = 12.0f;
    style.GrabRounding      = 8.0f;
    style.ScrollbarRounding = 10.0f;
    style.ScrollbarSize     = 8.0f * u;
    style.WindowPadding     = ImVec2(18.0f * u, 16.0f * u);
    style.FramePadding      = ImVec2(12.0f * u, 7.0f * u);
    style.ItemSpacing       = ImVec2(12.0f * u, 10.0f * u);
    style.ItemInnerSpacing  = ImVec2(8.0f * u, 6.0f * u);
    style.WindowBorderSize  = 0.0f;
    style.FrameBorderSize   = 0.0f;
    style.CellPadding       = ImVec2(10.0f * u, 6.0f * u);
    style.WindowTitleAlign  = ImVec2(0.0f, 0.5f);

    // 苹果系统色（light）：systemGroupedBackground / secondarySystemGroupedBackground
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                  = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    c[ImGuiCol_TextDisabled]          = ImVec4(0.56f, 0.56f, 0.58f, 1.00f); // secondaryLabel
    c[ImGuiCol_WindowBg]              = ImVec4(0.95f, 0.95f, 0.97f, 1.00f); // groupedBackground
    c[ImGuiCol_ChildBg]               = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // 白卡片
    c[ImGuiCol_PopupBg]               = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_Border]                = ImVec4(0.78f, 0.78f, 0.80f, 1.00f); // separator
    c[ImGuiCol_FrameBg]               = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
    c[ImGuiCol_FrameBgHovered]        = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    c[ImGuiCol_FrameBgActive]         = ImVec4(0.86f, 0.86f, 0.88f, 1.00f);
    c[ImGuiCol_TitleBg]               = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
    c[ImGuiCol_TitleBgActive]         = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
    c[ImGuiCol_Button]                = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
    c[ImGuiCol_ButtonHovered]         = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    c[ImGuiCol_ButtonActive]          = ImVec4(0.84f, 0.84f, 0.86f, 1.00f);
    c[ImGuiCol_Header]                = ImVec4(0.00f, 0.48f, 1.00f, 0.14f); // systemBlue 14%
    c[ImGuiCol_HeaderHovered]         = ImVec4(0.00f, 0.48f, 1.00f, 0.10f);
    c[ImGuiCol_HeaderActive]          = ImVec4(0.00f, 0.48f, 1.00f, 0.20f);
    c[ImGuiCol_Separator]             = ImVec4(0.78f, 0.78f, 0.80f, 1.00f);
    c[ImGuiCol_CheckMark]             = ImVec4(0.00f, 0.48f, 1.00f, 1.00f); // systemBlue
    c[ImGuiCol_SliderGrab]            = ImVec4(0.00f, 0.48f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrabActive]      = ImVec4(0.00f, 0.40f, 0.85f, 1.00f);
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.60f, 0.60f, 0.62f, 0.60f);
    c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.50f, 0.50f, 0.52f, 0.75f);
}

bool DesignerApp::createProjectFromTemplate(const std::string& name, const std::string& templateDir)
{
    if (name.empty())
    {
        this->lastError = "项目名不能为空";
        return false;
    }
    const char* home = std::getenv("HOME");
    const char* appdata = std::getenv("APPDATA");
    std::string base = appdata != nullptr ? std::string(appdata) + "/bklv-designer/projects"
                                          : std::string(home != nullptr ? home
                                                                        : "/tmp")
            + "/.bklv-designer/projects";
    const std::string target = base + "/" + name;

    std::error_code ec;
    if (std::filesystem::exists(target, ec))
    {
        this->lastError = "同名项目已存在：" + target;
        return false;
    }
    std::filesystem::create_directories(base, ec);
    std::filesystem::copy(bklv::resourcePath(templateDir), target,
        std::filesystem::copy_options::recursive, ec);
    if (ec)
    {
        this->lastError = "复制模板失败：" + ec.message();
        return false;
    }

    this->welcomeVisible = false;
    this->openThemeDir(target, name);
    this->lastError.clear();
    return true;
}

void DesignerApp::drawWelcome()
{
    if (!this->welcomeVisible)
        return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float          u        = bklv::getUiScale();
    const ImVec2         size(520.0f * u, 420.0f * u);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + (viewport->WorkSize.x - size.x) * 0.5f,
                               viewport->WorkPos.y + (viewport->WorkSize.y - size.y) * 0.5f),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin("##welcome", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoScrollbar);

    ImGui::Dummy(ImVec2(0.0f, 6.0f * u));
    ImGui::TextDisabled("外观设计器");
    ImGui::Dummy(ImVec2(0.0f, 2.0f * u));
    ImGui::SetWindowFontScale(1.45f);
    ImGui::TextUnformatted("开始");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Dummy(ImVec2(0.0f, 8.0f * u));
    ImGui::TextWrapped("做界面外观不用写代码：挑一个模板、改颜色和尺寸，保存后立刻生效。");

    ImGui::Dummy(ImVec2(0.0f, 14.0f * u));

    // 两张卡片：从模板创建 / 编辑已有
    const ImVec2 card(ImGui::GetContentRegionAvail().x, 124.0f * u); // 标题+说明+整行按钮
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::BeginChild("##newProject", card, true);
    ImGui::SetWindowFontScale(1.12f);
    ImGui::TextUnformatted("从模板创建项目");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextDisabled("复制一份内置模板，改的是你自己的副本");
    if (ImGui::Button("创建…##new", ImVec2(-1.0f, 0.0f)))
        this->createPanelVisible = true;
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0.0f, 8.0f * u));

    ImGui::BeginChild("##openProject", card, true);
    ImGui::SetWindowFontScale(1.12f);
    ImGui::TextUnformatted("编辑已有项目");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextDisabled("打开内置主题，或你之前创建的项目");
    if (ImGui::Button("打开…##open", ImVec2(-1.0f, 0.0f)))
    {
        this->welcomeVisible = false;
        this->wantOpenTheme  = true;
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    if (this->createPanelVisible)
    {
        ImGui::Dummy(ImVec2(0.0f, 8.0f * u));
        ImGui::TextDisabled("项目名");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##projectName", "例如 我的主题", this->projectName,
            sizeof(this->projectName));
        for (const std::string& theme : this->listThemes())
        {
            const std::string label = "用 " + theme + " 作模板";
            if (ImGui::Button(label.c_str()))
                this->createProjectFromTemplate(this->projectName, theme);
        }
    }

    if (!this->lastError.empty())
        ImGui::TextColored(ImVec4(0.85f, 0.20f, 0.18f, 1.0f), "%s", this->lastError.c_str());
    ImGui::End();
}

void DesignerApp::updateCanvasTitle()
{
    if (this->canvasTitle == nullptr)
        return;
    if (this->currentFile.empty())
    {
        this->canvasTitle->setText("");
        return;
    }

    char buffer[256];
    if (this->appearanceLoaded)
        std::snprintf(buffer, sizeof(buffer), "%s  ·  320 × 88  ·  %d%%",
            this->currentFile.c_str(), (int)(this->canvasZoom * 100.0f + 0.5f));
    else if (this->viewLoaded)
        std::snprintf(buffer, sizeof(buffer), "%s  ·  %d%%", this->currentFile.c_str(),
            (int)(this->canvasZoom * 100.0f + 0.5f));
    else
        std::snprintf(buffer, sizeof(buffer), "%s", this->currentFile.c_str());
    this->canvasTitle->setText(buffer);
}

void DesignerApp::syncCanvasSelection()
{
    if (this->canvasView == nullptr)
        return;
    this->canvasView->setDesignSelection(this->selectedLayer);
}

void DesignerApp::applyToCanvas()
{
    if (this->canvasView == nullptr)
        return;
    if (!this->appearanceLoaded)
    {
        this->canvasView->setVisibility(bklv::Visibility::GONE);
        return;
    }
    std::string error;

    // 预览垫一张白底：很多外观（如 widget/button.text）本身**没有底色**、只有文字，
    // 直接画在画布底上会看起来"什么都没渲染"。注入成第一个层，最稳（不依赖布局顺序）。
    std::string json = this->appearance.serialize();
    try
    {
        designer::Json document = designer::Json::parse(json);
        if (document.contains("layers") && document["layers"].is_array())
        {
            designer::Json artboard;
            artboard["op"]   = "rect";
            artboard["id"]   = "__artboard";
            // 画板色取主题的页面底色：深色主题上写死白色会让白字看不见（实测）
            const ImU32 page = bklv::getTheme().color("color.common.background",
                IM_COL32(0x1F, 0x22, 0x27, 0xFF));
            char rgb[16];
            std::snprintf(rgb, sizeof(rgb), "#%02X%02X%02X", (page >> 16) & 0xFF,
                (page >> 8) & 0xFF, page & 0xFF);
            artboard["fill"] = rgb;
            document["layers"].insert(document["layers"].begin(), artboard);
            json = document.dump(2);
        }
    }
    catch (const std::exception&)
    {
        // 解析失败就按原样交给库，让库报出真正的错误
    }

    // **不走库的外观缓存**：直接编译这份 JSON 换进画布那个实例，所以未保存的改动立刻可见
    if (!this->canvasView->applyAppearanceJson(json, error))
    {
        this->lastError = "画布预览失败：" + error;
        BKLV_LOG_ERROR("画布预览失败：%s", error.c_str());
    }
    this->canvasView->setVisibility(bklv::Visibility::VISIBLE);


}

void DesignerApp::handleCanvasClick()
{
    if (!this->appearanceLoaded || this->canvasView == nullptr)
        return;
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::GetIO().WantCaptureMouse)
        return;

    const float x  = this->canvasView->getX();
    const float y  = this->canvasView->getY();
    const float w  = this->canvasView->getWidth();
    const float h  = this->canvasView->getHeight();
    if (w <= 0.0f || h <= 0.0f)
        return;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (mouse.x < x || mouse.x > x + w || mouse.y < y || mouse.y > y + h)
        return;

    // 命中最上层的层（绘制顺序的最后一层压在最上面）
    const std::vector<LayerInfo> layers = this->appearance.getLayers();
    int hit = -1;
    for (int i = 0; i < (int)layers.size(); i++)
    {
        float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (!this->canvasView->getLayerRect(i, x, y, w, h, rect))
            continue;
        const bool inside = mouse.x >= rect[0] && mouse.x <= rect[0] + rect[2]
            && mouse.y >= rect[1] && mouse.y <= rect[1] + rect[3];
        if (inside)
            hit = i;
    }
    if (hit >= 0)
        this->selectedLayer = hit;
}

void DesignerApp::applyFieldEdit(const std::string& key, const designer::Json& value)
{
    if (this->selectedLayer < 0)
        return;
    const std::vector<LayerInfo> layers = this->appearance.getLayers();
    if (this->selectedLayer >= (int)layers.size())
        return;

    const std::string layerId = layers[(size_t)this->selectedLayer].id;
    const std::string before  = this->appearance.serialize();
    std::string       error;
    // 写"路径补丁"：只在值真的变了的时候记一步撤销
    if (this->appearance.setLayerField(layerId, key, value, error))
    {
        this->undo.record(before, "改 " + layerId + "." + key, layerId + "." + key,
            (int64_t)(ImGui::GetTime() * 1000.0));
        this->currentText = this->appearance.serialize();
        this->applyToCanvas();
        this->lastError.clear();
    }
    else
    {
        this->lastError = error;
    }
}

void DesignerApp::confirmFileSwitch(const std::string& relPath)
{
    if (this->appearanceLoaded && this->appearance.isDirty())
    {
        this->pendingFile = relPath;
        return;
    }
    this->openFile(relPath);
}

void DesignerApp::drawPendingSwitchPopup()
{
    if (this->pendingFile.empty())
        return;

    ImGui::OpenPopup("未保存的改动");
    if (ImGui::BeginPopupModal("未保存的改动", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("「%s」有未保存的改动，切走会丢掉它们。", this->currentFile.c_str());
        if (ImGui::Button("保存并切换"))
        {
            this->saveCurrent();
            if (!this->appearance.isDirty())
            {
                const std::string target = this->pendingFile;
                this->pendingFile.clear();
                this->openFile(target);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("丢弃并切换"))
        {
            const std::string target = this->pendingFile;
            this->pendingFile.clear();
            this->openFile(target);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消"))
        {
            this->pendingFile.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DesignerApp::addLayerOp(const std::string& op)
{
    if (!this->appearanceLoaded)
        return;
    const std::string before = this->appearance.serialize();
    std::string       newId, error;
    if (this->appearance.addLayer(op, newId, error))
    {
        this->undo.record(before, "添加组成部分 " + newId);
        this->currentText   = this->appearance.serialize();
        this->selectedLayer = (int)this->appearance.getLayers().size() - 1;
        this->applyToCanvas();
    }
    else
        this->lastError = error;
}

void DesignerApp::duplicateSelected()
{
    const std::vector<LayerInfo> layers = this->appearance.getLayers();
    if (!this->appearanceLoaded || this->selectedLayer < 0
        || this->selectedLayer >= (int)layers.size())
        return;
    const std::string before = this->appearance.serialize();
    const std::string id     = layers[(size_t)this->selectedLayer].id;
    std::string       copyId, error;
    if (this->appearance.duplicateLayer(id, copyId, error))
    {
        this->undo.record(before, "复制 " + id);
        this->currentText   = this->appearance.serialize();
        this->selectedLayer = this->appearance.findLayer(copyId);
        this->applyToCanvas();
    }
    else
        this->lastError = error;
}

void DesignerApp::deleteSelected()
{
    const std::vector<LayerInfo> layers = this->appearance.getLayers();
    if (!this->appearanceLoaded || this->selectedLayer < 0
        || this->selectedLayer >= (int)layers.size())
        return;
    const std::string before = this->appearance.serialize();
    const std::string id     = layers[(size_t)this->selectedLayer].id;
    std::string       error;
    if (this->appearance.removeLayer(id, error))
    {
        this->undo.record(before, "删层 " + id);
        this->currentText   = this->appearance.serialize();
        this->selectedLayer = std::min(this->selectedLayer,
            (int)this->appearance.getLayers().size() - 1);
        this->applyToCanvas();
    }
    else
        this->lastError = error;
}

void DesignerApp::moveSelected(int delta)
{
    const std::vector<LayerInfo> layers = this->appearance.getLayers();
    if (!this->appearanceLoaded || this->selectedLayer < 0
        || this->selectedLayer >= (int)layers.size())
        return;
    const int target = this->selectedLayer + delta;
    if (target < 0 || target >= (int)layers.size())
        return;

    const std::string before = this->appearance.serialize();
    const std::string id     = layers[(size_t)this->selectedLayer].id;
    std::string       error;
    if (this->appearance.moveLayer(id, (size_t)target, error))
    {
        this->undo.record(before, "移动 " + id);
        this->currentText   = this->appearance.serialize();
        this->selectedLayer = target;
        this->applyToCanvas();
    }
    else
        this->lastError = error;
}

void DesignerApp::renameSelected()
{
    const std::vector<LayerInfo> layers = this->appearance.getLayers();
    if (!this->appearanceLoaded || this->selectedLayer < 0
        || this->selectedLayer >= (int)layers.size())
        return;
    std::snprintf(this->renameBuffer, sizeof(this->renameBuffer), "%s",
        layers[(size_t)this->selectedLayer].id.c_str());
    this->wantRename = true;
}

void DesignerApp::drawLayerOps()
{
    if (!this->appearanceLoaded)
        return;

    static const char* kOps[] = { "rect", "text", "image", "shadow", "glass", "custom" };
    ImGui::SetNextItemWidth(90.0f * bklv::getUiScale());
    if (ImGui::BeginCombo("##newLayerOp", "＋ 添加组成部分", ImGuiComboFlags_HeightLarge))
    {
        for (const char* op : kOps)
            if (ImGui::Selectable(op))
                this->addLayerOp(op);
        ImGui::EndCombo();
    }

    const std::vector<LayerInfo> layers = this->appearance.getLayers();
    const bool hasSelection
        = this->selectedLayer >= 0 && this->selectedLayer < (int)layers.size();

    ImGui::SameLine();
    ImGui::BeginDisabled(!hasSelection);
    if (iconButton("content_copy", "复制所选层（Cmd/Ctrl+D）"))
        this->duplicateSelected();
    ImGui::SameLine();
    if (iconButton("delete", "删除所选层（Del）"))
        this->deleteSelected();
    ImGui::SameLine();
    if (ImGui::ArrowButton("##up", ImGuiDir_Up))
        this->moveSelected(-1);
    ImGui::SameLine();
    if (ImGui::ArrowButton("##down", ImGuiDir_Down))
        this->moveSelected(1);
    ImGui::EndDisabled();
}

void DesignerApp::drawInspector()
{
    this->drawRecoveryPopup();

    if (this->wantRename)
    {
        ImGui::OpenPopup("重命名 id");
        this->wantRename = false;
    }
    if (ImGui::BeginPopupModal("重命名 id", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("标识（同一外观内不能重名，如 bg / text）");
        ImGui::InputText("##id", this->renameBuffer, sizeof(this->renameBuffer));
        if (ImGui::Button("确定"))
        {
            const std::vector<LayerInfo> layers = this->appearance.getLayers();
            if (this->selectedLayer >= 0 && this->selectedLayer < (int)layers.size())
            {
                const std::string oldId = layers[(size_t)this->selectedLayer].id;
                const std::string newId(this->renameBuffer);
                std::string       error;
                const bool conflict
                    = newId != oldId && this->appearance.findLayer(newId) >= 0;
                if (conflict)
                {
                    this->lastError = "id 已被占用：" + newId;
                }
                else
                {
                    const std::string before = this->appearance.serialize();
                    if (this->appearance.setLayerField(
                            oldId, "id", designer::Json(newId), error))
                    {
                        this->undo.record(before, "重命名 " + oldId + " -> " + newId);
                        this->currentText = this->appearance.serialize();
                        this->applyToCanvas();
                    }
                    else
                        this->lastError = error;
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    const std::vector<LayerInfo> layers = this->appearance.getLayers();
    if (this->selectedLayer < 0 || this->selectedLayer >= (int)layers.size())
    {
        ImGui::TextDisabled("选一层开始编辑");
        return;
    }

    const std::string& layerId = layers[(size_t)this->selectedLayer].id;
    const std::string& op      = layers[(size_t)this->selectedLayer].op;
    const std::string  base    = "/layers/" + std::to_string(this->selectedLayer);

    // 统一叫"组成部分"；悬停给出概念解释
    ImGui::Text("%s", designer::segmentName(layerId).empty() ? layerId.c_str()
                                                            : designer::segmentName(layerId).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", designer::opTitle(op).c_str());
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", designer::layerConceptHint());
    ImGui::Separator();

    const std::vector<FieldDesc> fields = layerFields(op);
    if (fields.empty())
    {
        ImGui::TextDisabled("未知 op：%s（第三方层需要先在设计器进程里注册）", op.c_str());
        return;
    }

    // Figma 式分组：外观（颜色/透明度）、几何（尺寸/圆角/对齐/内缩/位移）、参数（其余）
    const auto groupOf = [](const FieldDesc& field) {
        if (field.key == "fill" || field.key == "stroke" || field.key == "opacity"
            || field.key == "fontSize" || field.key == "text")
            return 0;
        if (field.key == "inset" || field.key == "offset" || field.key == "width"
            || field.key == "height" || field.key == "radius" || field.key == "strokeWidth"
            || field.key == "align" || field.key == "valign")
            return 1;
        return 2;
    };
    static const char* kGroupNames[] = { "外观##g", "几何##g", "参数##g" };

    for (int group = 0; group < 3; group++)
    {
        bool any = false;
        for (const FieldDesc& field : fields)
            any = any || groupOf(field) == group;
        if (!any)
            continue;
        if (!ImGui::CollapsingHeader(kGroupNames[group],
                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
            continue;

    for (const FieldDesc& field : fields)
    {
        if (groupOf(field) != group)
            continue;
        const std::string pointer = base + "/" + field.key;
        const designer::Json* current = this->appearance.get(pointer);

        ImGui::PushID(field.key.c_str());
        fieldRowLabel(field.label.empty() ? field.key.c_str() : field.label.c_str());
        if (!field.help.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", field.help.c_str());

        switch (field.kind)
        {
            case FieldKind::NUMBER:
            case FieldKind::PIXELS:
            {
                float value = current != nullptr && current->is_number() ? current->get<float>()
                                                                        : field.numberDefault;
                if (ImGui::DragFloat("##v", &value, 0.5f, 0.0f, 0.0f, "%.1f"))
                    this->applyFieldEdit(field.key, designer::Json(value));
                break;
            }
            case FieldKind::COLOR:
            {
                // 只处理字面量颜色；@theme/ 引用在 P6 做选择器，这里显示成文本
                if (current != nullptr && current->is_string())
                {
                    ImGui::TextDisabled("%s", current->get<std::string>().c_str());
                    break;
                }
                unsigned int packed = current != nullptr && current->is_number() ? current->get<unsigned int>()
                                                                                : field.colorDefault;
                float color[4] = { ((packed >> 16) & 0xFF) / 255.0f, ((packed >> 8) & 0xFF) / 255.0f,
                    (packed & 0xFF) / 255.0f, ((packed >> 24) & 0xFF) / 255.0f };
                if (ImGui::ColorEdit4("##v", color, ImGuiColorEditFlags_AlphaBar))
                {
                    const unsigned int r = (unsigned int)(color[0] * 255.0f + 0.5f);
                    const unsigned int g = (unsigned int)(color[1] * 255.0f + 0.5f);
                    const unsigned int b = (unsigned int)(color[2] * 255.0f + 0.5f);
                    const unsigned int a = (unsigned int)(color[3] * 255.0f + 0.5f);
                    this->applyFieldEdit(
                        field.key, designer::Json((a << 24) | (b << 16) | (g << 8) | r));
                }
                break;
            }
            case FieldKind::BOOL:
            {
                bool value = current != nullptr && current->is_boolean() ? current->get<bool>()
                                                                         : field.boolDefault;
                if (ImGui::Checkbox("##v", &value))
                    this->applyFieldEdit(field.key, designer::Json(value));
                break;
            }
            case FieldKind::ENUM:
            {
                std::string value = current != nullptr && current->is_string()
                    ? current->get<std::string>()
                    : std::string();
                int index = 0;
                for (int i = 0; i < (int)field.enumValues.size(); i++)
                    if (field.enumValues[(size_t)i] == value)
                        index = i;
                if (ImGui::BeginCombo("##v", index < (int)field.enumValues.size()
                        ? field.enumValues[(size_t)index].c_str()
                        : "（未设）"))
                {
                    for (int i = 0; i < (int)field.enumValues.size(); i++)
                    {
                        const bool selected = i == index;
                        if (ImGui::Selectable(field.enumValues[(size_t)i].c_str(), selected))
                            this->applyFieldEdit(field.key, designer::Json(field.enumValues[(size_t)i]));
                    }
                    ImGui::EndCombo();
                }
                break;
            }
            case FieldKind::TEXT:
            {
                char buffer[512] = {0};
                if (current != nullptr && current->is_string())
                    std::snprintf(buffer, sizeof(buffer), "%s", current->get<std::string>().c_str());
                if (ImGui::InputText("##v", buffer, sizeof(buffer),
                        ImGuiInputTextFlags_EnterReturnsTrue))
                    this->applyFieldEdit(field.key, designer::Json(std::string(buffer)));
                break;
            }
            case FieldKind::INSET:
            case FieldKind::OFFSET:
            {
                const int count = field.kind == FieldKind::INSET ? 4 : 2;
                float values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                if (current != nullptr && current->is_array())
                    for (int i = 0; i < count && i < (int)current->size(); i++)
                        if ((*current)[(size_t)i].is_number())
                            values[i] = (*current)[(size_t)i].get<float>();
                if (ImGui::DragFloat2("##v", values, 0.5f))
                {
                    designer::Json array = designer::Json::array();
                    for (int i = 0; i < count; i++)
                        array.push_back(values[i]);
                    this->applyFieldEdit(field.key, array);
                }
                break;
            }
        }
        ImGui::PopID();
    }
    } // for (group)
}

void DesignerApp::drawJsonView()
{
    return; // 面板已合并进 drawPanels（检视器右栏）

    if (this->currentFile.empty())
    {
        ImGui::TextDisabled("左侧选一个文件");
        return;
    }
    ImGui::TextDisabled("%s.json%s", this->currentFile.c_str(),
        this->appearanceLoaded && this->appearance.isDirty() ? "  ●" : "");
    if (!this->lastError.empty())
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", this->lastError.c_str());
    if (!this->saveStatus.empty())
        ImGui::TextDisabled("%s", this->saveStatus.c_str());
    ImGui::Separator();

    if (!this->appearanceLoaded)
    {
        // 非外观文件（或解析失败）：P3 只读看源码，P5 换成可编辑
        ImGui::InputTextMultiline("##json", const_cast<char*>(this->currentText.c_str()),
            this->currentText.size() + 1, ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
        return;
    }

    const float half = ImGui::GetContentRegionAvail().x * 0.42f;
    ImGui::BeginChild("##layers", ImVec2(half, 0), true);
    ImGui::TextDisabled("组成部分");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", designer::layerConceptHint());
    const std::vector<LayerInfo> layers = this->appearance.getLayers();
    for (int i = 0; i < (int)layers.size(); i++)
    {
        char label[160];
        const std::string partName = designer::segmentName(layers[(size_t)i].id);
        std::snprintf(label, sizeof(label), "%s  %s",
            partName.empty() ? layers[(size_t)i].id.c_str() : partName.c_str(),
            designer::opTitle(layers[(size_t)i].op).c_str());
        if (ImGui::Selectable(label, this->selectedLayer == i))
            this->selectedLayer = i;
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##inspector", ImVec2(0, 0), true);
    this->drawInspector();
    ImGui::EndChild();
}

void DesignerApp::drawStatusBar()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - 26.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, 26.0f), ImGuiCond_Always);
    ImGui::Begin("##status", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextDisabled("%s · 文件 %zu · 当前 %s", this->themeTitle.c_str(), this->files.size(),
        this->currentFile.empty() ? "（无）" : this->currentFile.c_str());
    ImGui::End();
}

} // namespace designer

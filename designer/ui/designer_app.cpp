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
#include "designer/core/view_fields.hpp"
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
    auto root  = std::make_unique<bklv::Box>(bklv::Axis::COLUMN);
    auto header = new bklv::Header();
    header->setTitle("外观设计器");
    header->setSubtitle(this->options.themeDir);
    this->header = header;
    root->addView(header);

    // 中间一行：左让位（文件面板）+ 画布 + 右让位（检视器）。
    // 三栏宽度与 ImGui 面板一致（都乘 uiScale），所以画布刚好夹在中间、不会重叠。
    auto row = std::make_unique<bklv::Box>(bklv::Axis::ROW);
    auto leftSpacer = std::make_unique<bklv::Box>(bklv::Axis::COLUMN);
    leftSpacer->setWidth(this->fileFilterWidth * bklv::getUiScale());
    row->addView(leftSpacer.release());

    auto canvasHost = std::make_unique<bklv::Box>(bklv::Axis::COLUMN);
    canvasHost->setGrow(1.0f);
    canvasHost->setBackgroundColor(IM_COL32(0x16, 0x1a, 0x22, 0xFF));
    auto canvas = new bklv::AppearanceView(std::string(), false);
    canvas->setGrow(1.0f);                              // 填满画布区的高度
    canvas->setAlignSelf(bklv::AlignSelf::STRETCH);    // 填满宽度
    this->canvasView = canvas;
    canvasHost->addView(canvas);
    this->canvasHost = canvasHost.get();
    row->addView(canvasHost.release());

    auto rightSpacer = std::make_unique<bklv::Box>(bklv::Axis::COLUMN);
    rightSpacer->setWidth(this->inspectorWidth * bklv::getUiScale());
    row->addView(rightSpacer.release());
    row->setGrow(1.0f);
    root->addView(row.release());

    this->app.setRootView(root.release());

    if (this->options.clickX >= 0.0f)
        this->pendingClick = true; // 第一帧后再点（此时布局已算完）

    this->app.onDrawWindows = [this]() { this->drawPanels(); };
    this->app.onUpdate       = [this]() {
        // Cmd/Ctrl + S 保存（ImGui 输入在 update 里读没问题：无窗口测试不会走这条路）
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)
            || ImGui::IsKeyChordPressed(ImGuiMod_Super | ImGuiKey_S))
            this->saveCurrent();
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
    const fs::path root(this->options.themeDir);
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
    const std::string path = this->options.themeDir + "/" + relPath + ".json";
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
    const std::string path = this->options.themeDir + "/" + this->currentFile + ".json";

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

    this->maybeAutosave();

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

    if (this->header == nullptr)
        return;
    std::string subtitle = this->themeTitle + " · " + std::to_string(this->files.size()) + " 个文件";
    if (!this->currentFile.empty())
        subtitle += " · " + this->currentFile;
    if (!this->lastError.empty())
        subtitle += " · ⚠ " + this->lastError;
    this->header->setSubtitle(subtitle);
}

void DesignerApp::drawPanels()
{
    const float top   = 96.0f * bklv::getUiScale();
    const float scale = bklv::getUiScale();
    const float leftW = this->fileFilterWidth * scale;
    const float rightW = this->inspectorWidth * scale;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float panelH = viewport->WorkSize.y - top - 12.0f;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(leftW, panelH), ImGuiCond_Always);
    ImGui::Begin("文件", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoBringToFrontOnFocus);
    this->drawFileList();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - rightW,
                              viewport->WorkPos.y + top),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(rightW, panelH), ImGuiCond_Always);
    ImGui::Begin("检视器", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoBringToFrontOnFocus);
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
        if (ImGui::Button("属性"))
            this->inspectorTab = 0;
        ImGui::SameLine();
        if (ImGui::Button("源码"))
            this->inspectorTab = 1;
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
            std::snprintf(label, sizeof(label), "%s  %s", layers[(size_t)i].op.c_str(),
                layers[(size_t)i].id.c_str());
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
        ImGui::TextDisabled("%s", this->currentFile.empty() ? "左侧选一个文件" : this->currentFile.c_str());
        if (!this->lastError.empty())
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", this->lastError.c_str());
        ImGui::Separator();
        ImGui::TextDisabled("非外观文件（P5 起可编辑）");
        ImGui::BeginChild("##raw", ImVec2(0, 0), true);
        ImGui::TextUnformatted(this->currentText.c_str());
        ImGui::EndChild();
    }
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
    ImGui::SameLine();
    ImGui::TextDisabled("%zu 个文件", this->files.size());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "过滤文件名…", this->filter, sizeof(this->filter));

    ImGui::BeginChild("##list", ImVec2(0, 0), false);
    for (int i = 0; i < (int)this->files.size(); i++)
    {
        const FileEntry& entry = this->files[(size_t)i];
        if (this->filter[0] != '\0'
            && entry.relPath.find(this->filter) == std::string::npos)
            continue;

        const bool selected = (entry.relPath == this->currentFile);
        if (ImGui::Selectable(entry.relPath.c_str(), selected))
        {
            this->selectedIndex = i;
            this->confirmFileSwitch(entry.relPath);
            if (!this->lastError.empty())
                ImGui::OpenPopup("打开失败");
        }
    }
    ImGui::EndChild();

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

    if (ImGui::Button("属性"))
        this->inspectorTab = 0;
    ImGui::SameLine();
    if (ImGui::Button("源码"))
        this->inspectorTab = 1;
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
        // 标签单独一行：ImGui 的 SetNextItemWidth(-1) 会把"控件前的标签"挤出可视区
        ImGui::TextUnformatted(field.key.c_str());
        ImGui::SetNextItemWidth(-1);
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
    // **不走库的外观缓存**：直接编译这份 JSON 换进画布那个实例，所以未保存的改动立刻可见
    if (!this->canvasView->applyAppearanceJson(this->appearance.serialize(), error))
        this->lastError = "画布预览失败：" + error;
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
        this->undo.record(before, "加层 " + newId);
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
    if (ImGui::BeginCombo("##newLayerOp", "＋ 加层"))
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
    if (ImGui::Button("复制"))
        this->duplicateSelected();
    ImGui::SameLine();
    if (ImGui::Button("删"))
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
        ImGui::TextUnformatted("层 id（同层不能重名）");
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

    ImGui::Text("层 %s（%s）", layerId.c_str(), op.c_str());
    ImGui::Separator();

    const std::vector<FieldDesc> fields = layerFields(op);
    if (fields.empty())
    {
        ImGui::TextDisabled("未知 op：%s（第三方层需要先在设计器进程里注册）", op.c_str());
        return;
    }

    for (const FieldDesc& field : fields)
    {
        const std::string pointer = base + "/" + field.key;
        const designer::Json* current = this->appearance.get(pointer);

        ImGui::PushID(field.key.c_str());
        ImGui::TextUnformatted(field.label.empty() ? field.key.c_str() : field.label.c_str());
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
    ImGui::TextDisabled("层（绘制顺序见 DESIGN §4.2）");
    const std::vector<LayerInfo> layers = this->appearance.getLayers();
    for (int i = 0; i < (int)layers.size(); i++)
    {
        char label[160];
        std::snprintf(label, sizeof(label), "%s  %s", layers[(size_t)i].op.c_str(),
            layers[(size_t)i].id.c_str());
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

#include "designer/ui/designer_app.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "bklv/3rd/imgui/imgui.h"
#include "bklv/core/box.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/ui_scale.hpp"
#include "bklv/views/label.hpp"
#include "bklv/widgets/header.hpp"

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
    this->app.setRootView(root.release());

    this->app.onDrawWindows = [this]() { this->drawPanels(); };
    this->app.onUpdate       = [this]() { this->update(); };
    this->scanTheme();
    if (!this->options.initialFile.empty())
        this->openFile(this->options.initialFile);
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
    return true;
}

void DesignerApp::update()
{
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
    const float top = 96.0f * bklv::getUiScale();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(this->fileFilterWidth, viewport->WorkSize.y - top - 12.0f), ImGuiCond_Always);
    ImGui::Begin("文件", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoBringToFrontOnFocus);
    this->drawFileList();
    ImGui::End();

    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + this->fileFilterWidth, viewport->WorkPos.y + top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(viewport->WorkSize.x - this->fileFilterWidth, viewport->WorkSize.y - top - 12.0f),
        ImGuiCond_Always);
    ImGui::Begin("内容", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoBringToFrontOnFocus);
    this->drawJsonView();
    ImGui::End();

}

void DesignerApp::drawMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;
    if (ImGui::BeginMenu("文件"))
    {
        if (ImGui::MenuItem("重新扫描"))
            this->scanTheme();
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
            if (!this->openFile(entry.relPath))
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

void DesignerApp::drawJsonView()
{
    if (this->currentFile.empty())
    {
        ImGui::TextDisabled("左侧选一个文件");
        return;
    }
    ImGui::TextDisabled("%s.json", this->currentFile.c_str());
    ImGui::Separator();
    if (this->lastErrorEnabled)
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", this->lastError.c_str());

    // 只读展示：P5 会换成可编辑 + 校验 + 行列定位
    ImGui::InputTextMultiline("##json", const_cast<char*>(this->currentText.c_str()),
        this->currentText.size() + 1, ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
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

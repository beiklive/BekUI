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
#include "designer/core/save_transaction.hpp"
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
    this->app.onUpdate       = [this]() {
        // Cmd/Ctrl + S 保存（ImGui 输入在 update 里读没问题：无窗口测试不会走这条路）
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)
            || ImGui::IsKeyChordPressed(ImGuiMod_Super | ImGuiKey_S))
            this->saveCurrent();
        this->update();
    };
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
    this->saveStatus.clear();
    this->appearanceLoaded = false;
    this->selectedLayer    = -1;
    this->undo.clear();

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
        this->saveStatus = "已保存 " + this->currentFile;
    }
    else
    {
        this->lastError = saveError;
    }
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
        this->lastError.clear();
    }
    else
    {
        this->lastError = error;
    }
}

void DesignerApp::drawInspector()
{
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

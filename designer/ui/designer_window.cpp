#include "designer/ui/designer_window.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>

#include "bklv/3rd/imgui/imgui.h"
#include "designer/core/diff.hpp"
#include "designer/core/theme_files.hpp"

namespace fs = std::filesystem;

using namespace bklv;

namespace designer
{
namespace
{

const ImVec4 kAccent(0.0f, 1.0f, 0.8f, 1.0f);
const ImVec4 kMuted(0.55f, 0.57f, 0.61f, 1.0f);

// ImGui 的 InputText 要 char 缓冲，这里包一层（长度上限保护）
bool inputString(const char* label, std::string& value, size_t maxLength = 512)
{
    std::vector<char> buffer(maxLength);
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
    if (ImGui::InputText(label, buffer.data(), buffer.size()))
    {
        value = buffer.data();
        return true;
    }
    return false;
}

bool inputNumberFloat(const char* label, double& value)
{
    float temp = (float)value;
    if (ImGui::InputFloat(label, &temp, 1.0f, 10.0f, "%.2f"))
    {
        value = temp;
        return true;
    }
    return false;
}

// 一层里某个值是数字、字符串还是数组
std::string valueKind(const AppearanceDoc::Json& value)
{
    if (value.is_number())
        return "number";
    if (value.is_boolean())
        return "bool";
    if (value.is_string())
        return "string";
    if (value.is_array())
        return "array";
    return "other";
}

} // namespace

// ---------------------------------------------------------------- 生命周期

void DesignerWindow::attach(App* application)
{
    this->app = application;

    // 设计器是鼠标工具：关掉 ImGui 自己的键盘/手柄导航。
    // 不关的话方向键会被 ImGui 拿去移动它内部的"焦点"，树会被滚到不知哪里去（踩过）。
    if (ImGui::GetCurrentContext() != nullptr)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    }

    if (this->app != nullptr)
        this->app->onDrawWindows = [this] { this->draw(); };
}

bool DesignerWindow::setProject(const Project& value, bool isReadOnly, std::string& error)
{
    // 主题按**目录**装（项目可能在别的仓库），装完清外观缓存再重建预览
    if (!loadThemeIntoGlobalFromDirectory(value.themeDir(), value.name))
    {
        error = "主题装载失败：" + value.themeDir();
        return false;
    }

    this->project     = value;
    this->hasProject  = true;
    this->readOnly    = isReadOnly;
    this->dirty       = false;
    this->currentWidget.clear();
    this->currentViewFile.clear();
    this->currentFilePath.clear();
    this->savedText.clear();
    this->previewDirty = true;

    this->refreshFileList();

    // 默认打开第一个外观文件，界面一进来就有的看
    if (!this->widgetFiles.empty())
        this->openAppearance(widgetNameFromPath(this->widgetFiles.front()), error);
    return true;
}

void DesignerWindow::refreshFileList()
{
    this->widgetFiles.clear();
    this->viewFiles.clear();

    std::string error;
    readWidgetFileList(this->project.themeDir(), this->widgetFiles, error);

    // 页面视图：扫 resources/views（桌面工具，这里扫目录没问题）
    const std::string viewsDir = this->project.resourcesDir() + "/views";
    std::error_code code;
    if (fs::is_directory(viewsDir, code))
    {
        for (const auto& entry : fs::directory_iterator(viewsDir, code))
        {
            if (!entry.is_regular_file())
                continue;
            if (entry.path().extension() != ".json")
                continue;
            this->viewFiles.push_back(entry.path().filename().string());
        }
        std::sort(this->viewFiles.begin(), this->viewFiles.end());
    }
}

bool DesignerWindow::openAppearance(const std::string& widgetName, std::string& error)
{
    const std::string path = this->project.themeDir() + "/" + widgetPathFromName(widgetName);

    std::string text;
    if (!readTextFile(path, text, error))
        return false;

    if (!this->doc.parse(text, error))
        return false;

    this->currentWidget    = widgetName;
    this->currentViewFile.clear();
    this->currentFilePath  = path;
    this->savedText        = text;
    this->dirty            = false;
    this->selection        = Selection{};
    this->previewDirty     = true;
    this->errorMessage.clear();
    this->statusMessage    = "打开 " + widgetName;
    return true;
}

bool DesignerWindow::openViewFile(const std::string& fileName)
{
    this->currentViewFile = fileName;
    this->currentWidget.clear();
    this->currentFilePath = this->project.resourcesDir() + "/views/" + fileName;

    std::string text;
    std::string error;
    if (!readTextFile(this->currentFilePath, text, error))
    {
        this->errorMessage = error;
        return false;
    }
    this->savedText = text;
    const AppearanceDoc::Json parsed = AppearanceDoc::Json::parse(text, nullptr, false);
    this->doc.root() = parsed.is_discarded() ? AppearanceDoc::Json::object() : parsed;
    this->selection    = Selection{};
    this->selection.kind = SelKind::VIEW;
    this->previewDirty = true;
    this->dirty        = false;
    this->statusMessage = "打开 " + fileName;
    return true;
}

bool DesignerWindow::selectBySpec(const std::string& spec)
{
    if (spec == "file")
    {
        this->selection = Selection{};
        this->previewDirty = true;
        return true;
    }
    if (spec == "box")
    {
        this->selection = Selection{};
        this->selection.kind = SelKind::BOX;
        this->previewDirty = true;
        return true;
    }
    if (spec.rfind("layer", 0) == 0)
    {
        const int index = std::atoi(spec.c_str() + 5);
        if (index < 0 || index >= this->doc.layerCount())
            return false;
        this->selection = Selection{};
        this->selection.kind  = SelKind::LAYER;
        this->selection.index = index;
        this->previewDirty = true;
        return true;
    }
    if (spec.rfind("state:", 0) == 0)
    {
        const std::string state = spec.substr(6);
        for (const std::string& name : this->doc.stateNames())
        {
            if (name != state)
                continue;
            this->selection = Selection{};
            this->selection.kind  = SelKind::STATE;
            this->selection.state = name;
            this->previewDirty = true;
            return true;
        }
        return false;
    }
    return false;
}

// ---------------------------------------------------------------- 顶栏

void DesignerWindow::drawTopBar()
{
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::TextUnformatted("bklv 设计器");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    if (this->hasProject)
    {
        std::string root = this->project.themeRoot;
        if (root.size() > 44)
            root = "…" + root.substr(root.size() - 43);
        ImGui::TextColored(kMuted, "%s（%s%s）· %s", this->project.name.c_str(),
            this->project.mode == ProjectMode::FORK ? "模板新建，来自 " : "直接编辑",
            this->project.templateName.c_str(), root.c_str());
        if (this->readOnly)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "只读");
        }
    }
    else
    {
        ImGui::TextColored(kMuted, "还没有打开项目");
    }

    ImGui::SameLine();
    const float buttonsWidth = 420.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - buttonsWidth);
    (void)buttonsWidth;

    if (ImGui::Button("新建项目"))
        this->showNewProject = true;
    ImGui::SameLine();
    if (ImGui::Button("重载"))
    {
        std::string error;
        this->applyThemeFromProject(error);
        if (!error.empty())
            this->errorMessage = error;
        if (!this->currentWidget.empty())
            this->openAppearance(this->currentWidget, error);
        this->previewDirty = true;
        this->statusMessage = "已重新装载主题";
    }
    ImGui::SameLine();
    if (ImGui::Button("还原") && !this->currentFilePath.empty())
        this->revert();
    ImGui::SameLine();
    ImGui::BeginDisabled(this->readOnly || this->currentFilePath.empty());
    if (ImGui::Button("保存…"))
        this->requestSave();
    ImGui::EndDisabled();
    if (this->dirty)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "●未保存");
    }
    ImGui::Separator();
}

// ---------------------------------------------------------------- 文件列表

void DesignerWindow::drawFileList()
{
    ImGui::TextColored(kMuted, "主题外观（%s）", this->project.name.c_str());
    for (const std::string& relative : this->widgetFiles)
    {
        const std::string name = widgetNameFromPath(relative);
        const bool selected    = (name == this->currentWidget);
        if (ImGui::Selectable(name.c_str(), selected))
        {
            std::string error;
            if (!this->openAppearance(name, error))
                this->errorMessage = error;
        }
    }

    ImGui::Separator();
    ImGui::TextColored(kMuted, "页面视图（views/）");
    if (this->viewFiles.empty())
        ImGui::TextColored(kMuted, "（没有）");
    for (const std::string& file : this->viewFiles)
    {
        if (ImGui::Selectable(file.c_str(), file == this->currentViewFile))
        {
            this->currentViewFile = file;
            this->currentWidget.clear();
            this->currentFilePath = this->project.resourcesDir() + "/views/" + file;
            std::string text, error;
            if (readTextFile(this->currentFilePath, text, error))
            {
                this->savedText = text;
                this->doc.root() = AppearanceDoc::Json::parse(text, nullptr, false);
                if (this->doc.root().is_discarded())
                    this->doc.root() = AppearanceDoc::Json::object();
            }
            this->selection = Selection{};
            this->selection.kind = SelKind::VIEW;
            this->previewDirty = true;
            this->dirty        = false;
        }
    }

    ImGui::Separator();
    ImGui::TextColored(kMuted, "主题表");
    ImGui::TextColored(kMuted, "colors.json（%d 色） · metrics.json（%d 数值）",
        getTheme().getColorCount(), (int)getTheme().getNumberKeys().size());
}

// ---------------------------------------------------------------- 节点树

void DesignerWindow::drawTree()
{
    if (this->currentWidget.empty())
    {
        ImGui::TextColored(kMuted, "先从上面挑一个外观文件");
        return;
    }

    const ImGuiTreeNodeFlags leafFlags = ImGuiTreeNodeFlags_Leaf
        | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;

    // 文件
    {
        const bool selected = this->selection.kind == SelKind::FILE;
        if (ImGui::TreeNodeEx("##file", ImGuiTreeNodeFlags_DefaultOpen
                | ImGuiTreeNodeFlags_SpanAvailWidth
                | (selected ? ImGuiTreeNodeFlags_Selected : 0), "file  %s", this->currentWidget.c_str()))
        {
            if (ImGui::IsItemClicked())
            {
                this->selection = Selection{};
                this->previewDirty = true;
            }

            // box
            const bool boxSelected = this->selection.kind == SelKind::BOX;
            if (ImGui::TreeNodeEx("##box", ImGuiTreeNodeFlags_DefaultOpen
                    | ImGuiTreeNodeFlags_SpanAvailWidth
                    | (boxSelected ? ImGuiTreeNodeFlags_Selected : 0), "box"))
            {
                if (ImGui::IsItemClicked())
                {
                    this->selection = Selection{};
                    this->selection.kind = SelKind::BOX;
                    this->previewDirty = true;
                }
                for (const std::string& key : this->doc.boxKeys())
                {
                    AppearanceDoc::Json value;
                    std::string shown;
                    if (this->doc.getBoxString(key, shown))
                    {
                        // 字符串值（含 @theme 引用）
                    }
                    else
                    {
                        double number = 0.0;
                        this->doc.getBoxNumber(key, number);
                        char text[64];
                        std::snprintf(text, sizeof(text), "%g", number);
                        shown = text;
                    }
                    ImGui::TreeNodeEx((void*)(uintptr_t)(key.c_str()), leafFlags, "field  %s = %s",
                        key.c_str(), shown.c_str());
                }
                ImGui::TreePop();
            }

            // 层
            for (int i = 0; i < this->doc.layerCount(); i++)
            {
                const bool selected = (this->selection.kind == SelKind::LAYER
                                          || this->selection.kind == SelKind::LAYER_FIELD)
                    && this->selection.index == i;
                char label[128];
                std::snprintf(label, sizeof(label), "#%d  %s  %s", i + 1,
                    this->doc.layerOp(i).c_str(), this->doc.layerId(i).c_str());
                const std::string id = "##layer" + std::to_string(i);
                if (ImGui::TreeNodeEx(id.c_str(), ImGuiTreeNodeFlags_DefaultOpen
                        | ImGuiTreeNodeFlags_SpanAvailWidth
                        | (selected ? ImGuiTreeNodeFlags_Selected : 0), "%s", label))
                {
                    if (ImGui::IsItemClicked())
                    {
                        this->selection = Selection{};
                        this->selection.kind  = SelKind::LAYER;
                        this->selection.index = i;
                        this->previewDirty = true;
                    }
                    const AppearanceDoc::Json* layer = this->doc.layer(i);
                    if (layer != nullptr)
                    {
                        for (auto it = layer->begin(); it != layer->end(); ++it)
                        {
                            std::string shown = compactValue(it.value());
                            ImGui::TreeNodeEx((void*)(uintptr_t)(id + it.key()).c_str(), leafFlags,
                                "field  %s = %s", it.key().c_str(), shown.c_str());
                            if (ImGui::IsItemClicked())
                            {
                                this->selection = Selection{};
                                this->selection.kind  = SelKind::LAYER_FIELD;
                                this->selection.index = i;
                                this->selection.key   = it.key();
                                this->previewDirty = true;
                            }
                        }
                    }
                    ImGui::TreePop();
                }
            }

            // states
            if (ImGui::TreeNodeEx("##states", ImGuiTreeNodeFlags_SpanAvailWidth, "states"))
            {
                for (const std::string& state : this->doc.stateNames())
                {
                    const bool selected = (this->selection.kind == SelKind::STATE
                                              || this->selection.kind == SelKind::CHANNEL)
                        && this->selection.state == state;
                    const std::string id = "##state" + state;
                    if (ImGui::TreeNodeEx(id.c_str(), ImGuiTreeNodeFlags_DefaultOpen
                            | ImGuiTreeNodeFlags_SpanAvailWidth
                            | (selected ? ImGuiTreeNodeFlags_Selected : 0), "state  %s", state.c_str()))
                    {
                        if (ImGui::IsItemClicked())
                        {
                            this->selection = Selection{};
                            this->selection.kind  = SelKind::STATE;
                            this->selection.state = state;
                            this->previewDirty = true;
                        }
                        const AppearanceDoc::Json& node = this->doc.root()["states"][state];
                        int index = 0;
                        for (auto it = node.begin(); it != node.end(); ++it, ++index)
                        {
                            const bool chSelected = this->selection.kind == SelKind::CHANNEL
                                && this->selection.state == state && this->selection.index == index;
                            ImGui::TreeNodeEx((void*)(uintptr_t)(id + it.key()).c_str(),
                                leafFlags | (chSelected ? ImGuiTreeNodeFlags_Selected : 0),
                                "channel  %s", it.key().c_str());
                            if (ImGui::IsItemClicked())
                            {
                                this->selection = Selection{};
                                this->selection.kind  = SelKind::CHANNEL;
                                this->selection.state = state;
                                this->selection.index = index;
                                this->previewDirty = true;
                            }
                        }
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }

    // 层序硬约定的提醒
    for (const std::string& warning : this->doc.layerOrderWarnings())
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "⚠ %s", warning.c_str());
}

// ---------------------------------------------------------------- 左下状态

void DesignerWindow::drawStatus()
{
    int nodes = 1 + (int)this->doc.boxKeys().size();
    for (int i = 0; i < this->doc.layerCount(); i++)
        nodes += 1 + (this->doc.layer(i) != nullptr ? (int)this->doc.layer(i)->size() : 0);
    for (const std::string& state : this->doc.stateNames())
        nodes += 1 + this->doc.channelCount(state);

    ImGui::TextColored(kAccent, "节点 %d", nodes);
    ImGui::SameLine();
    ImGui::TextColored(kMuted, "· 层 %d · 状态 %d", this->doc.layerCount(),
        (int)this->doc.stateNames().size());

    std::string path = this->project.name;
    if (!this->currentWidget.empty())
        path += " / " + this->currentWidget;
    if (this->selection.kind == SelKind::BOX)
        path += " / box";
    else if (this->selection.kind == SelKind::LAYER || this->selection.kind == SelKind::LAYER_FIELD)
    {
        path += " / layers[" + std::to_string(this->selection.index) + "] "
            + this->doc.layerId(this->selection.index);
        if (this->selection.kind == SelKind::LAYER_FIELD)
            path += "." + this->selection.key;
    }
    else if (this->selection.kind == SelKind::STATE || this->selection.kind == SelKind::CHANNEL)
    {
        path += " / states." + this->selection.state;
        if (this->selection.kind == SelKind::CHANNEL)
        {
            const AppearanceDoc::Json& node = this->doc.root()["states"][this->selection.state];
            int index = 0;
            for (auto it = node.begin(); it != node.end() && index < this->selection.index;
                 ++it, ++index)
            {
            }
        }
    }
    ImGui::TextColored(kMuted, "%s", path.c_str());

    if (!this->statusMessage.empty())
        ImGui::TextColored(kMuted, "%s", this->statusMessage.c_str());
    if (!this->errorMessage.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f), "%s", this->errorMessage.c_str());
}

// ---------------------------------------------------------------- 预览

AppearanceView* DesignerWindow::createPreviewControl() const
{
    const std::string& name = this->currentWidget;

    if (name.rfind("button", 0) == 0)
    {
        const std::string variant = name.size() > 7 ? name.substr(7) : std::string();
        return new Button("主要按钮", variant);
    }
    if (name == "cell_bool")
        return new BooleanCell("显示页头", true);
    if (name == "cell_radio")
        return new RadioCell("使用自定义主题", true);
    if (name == "cell_slider")
        return new SliderCell("补间时长（秒）");
    if (name == "cell_selector")
    {
        SelectorCell* cell = new SelectorCell("主题");
        cell->setOptions({ "default", "ocean" });
        return cell;
    }
    if (name == "cell_input")
        return new InputCell("网址");
    if (name.rfind("cell", 0) == 0)
        return new DetailCell("页面标题", "右侧说明文字");

    return new AppearanceView(name.empty() ? "rectangle" : name, false);
}

std::string DesignerWindow::previewJson() const
{
    AppearanceDoc::Json copy = this->doc.root();

    switch (this->selection.kind)
    {
        case SelKind::LAYER:
        {
            // 只留这一层
            AppearanceDoc::Json only = AppearanceDoc::Json::object();
            only["layers"] = AppearanceDoc::Json::array();
            const AppearanceDoc::Json* layer = this->doc.layer(this->selection.index);
            if (layer != nullptr)
                only["layers"].push_back(*layer);
            return only.dump();
        }
        case SelKind::LAYER_FIELD:
        case SelKind::FILE:
            return copy.dump();
        case SelKind::STATE:
        case SelKind::CHANNEL:
        {
            // 状态预览：把该状态所有通道的 duration 归零，效果立刻到位
            if (copy.contains("states") && copy["states"].contains(this->selection.state))
            {
                AppearanceDoc::Json one = AppearanceDoc::Json::object();
                one["states"][this->selection.state] = copy["states"][this->selection.state];
                one["layers"] = copy.value("layers", AppearanceDoc::Json::array());
                one["box"]    = copy.value("box", AppearanceDoc::Json::object());
                if (one.contains("states"))
                    for (auto it = one["states"][this->selection.state].begin();
                         it != one["states"][this->selection.state].end(); ++it)
                        if (it.value().is_object())
                            it.value()["duration"] = 0;
                return one.dump();
            }
            return copy.dump();
        }
        default:
            return copy.dump();
    }
}

void DesignerWindow::drawBoxGuides(ImVec2 origin, ImVec2 size) const
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    double paddingTop = 0, paddingSides = 0;
    this->doc.getBoxNumber("paddingTop", paddingTop);
    this->doc.getBoxNumber("paddingSides", paddingSides);

    drawList->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y),
        IM_COL32(255, 183, 77, 255), 0.0f, 0, 1.0f);
    drawList->AddRect(ImVec2(origin.x + (float)paddingSides, origin.y + (float)paddingTop),
        ImVec2(origin.x + size.x - (float)paddingSides, origin.y + size.y - (float)paddingTop),
        IM_COL32(0, 255, 204, 255), 0.0f, 0, 1.0f);
    drawList->AddText(ImVec2(origin.x + 4, origin.y + 2), IM_COL32(255, 183, 77, 255),
        "margin/边界");
    drawList->AddText(ImVec2(origin.x + (float)paddingSides + 4, origin.y + (float)paddingTop + 14),
        IM_COL32(0, 255, 204, 255), "padding 内的内容区");
}

void DesignerWindow::ensurePreviewBuilt()
{
    if (!this->previewDirty)
        return;

    this->previewDirty = false;
    this->previewError.clear();

    if (this->previewView != nullptr)
    {
        delete this->previewView;
        this->previewView = nullptr;
    }

    // 页面视图：直接实例化那棵视图树
    if (!this->currentViewFile.empty())
    {
        std::string text;
        std::string readError;
        if (readTextFile(this->currentFilePath, text, readError))
            this->previewView = createFromJSONString(text);
        if (this->previewView == nullptr)
            this->previewError = readError.empty() ? "视图 JSON 实例化失败（看日志）" : readError;
        return;
    }

    if (this->currentWidget.empty())
        return;

    AppearanceView* control = this->createPreviewControl();
    std::string compileError;
    if (!control->applyAppearanceJson(this->previewJson(), compileError))
    {
        this->previewError = compileError;
        delete control;
        return;
    }

    // 状态预览：把该状态打开（duration 已经归零，立刻到位）
    if (this->selection.kind == SelKind::STATE || this->selection.kind == SelKind::CHANNEL)
        control->setAppearanceState(this->selection.state, true);

    this->previewView = control;
}

void DesignerWindow::drawPreview()
{
    // 画布控制条
    ImGui::TextColored(kAccent, "预览");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::InputInt("宽", &this->canvasWidth);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::InputInt("高", &this->canvasHeight);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("缩放", &this->canvasZoom, "50%\0" "75%\0" "100%\0" "150%\0");
    this->canvasWidth  = std::max(40, std::min(this->canvasWidth, 4000));
    this->canvasHeight = std::max(30, std::min(this->canvasHeight, 4000));
    const float zoom   = this->canvasZoom == 0 ? 0.5f : this->canvasZoom == 1 ? 0.75f
        : this->canvasZoom == 2 ? 1.0f : 1.5f;

    const char* modeText = "渲染整个控件外观";
    switch (this->selection.kind)
    {
        case SelKind::BOX: modeText = "box 段：盒模型/间距（虚线标出边界与 padding）"; break;
        case SelKind::LAYER: modeText = "只渲染这一层"; break;
        case SelKind::STATE:
        case SelKind::CHANNEL: modeText = "该状态生效时的样子（时长已归零，立刻到位）"; break;
        default: break;
    }
    if (!this->currentViewFile.empty())
        modeText = "整页视图：真实实例化这棵视图树";
    ImGui::SameLine();
    ImGui::TextColored(kMuted, "%s", modeText);

    ImGui::BeginChild("##stage", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size((float)this->canvasWidth * zoom, (float)this->canvasHeight * zoom);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 画布底：棋盘格方便看 shadow / 半透明
    const ImU32 dark = IM_COL32(38, 38, 38, 255);
    const ImU32 lite = IM_COL32(48, 48, 48, 255);
    drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), dark);
    for (int y = 0; y < (int)size.y; y += 16)
        for (int x = 0; x < (int)size.x; x += 16)
            if (((x / 16) + (y / 16)) % 2 == 0)
                drawList->AddRectFilled(ImVec2(origin.x + (float)x, origin.y + (float)y),
                    ImVec2(std::min(origin.x + x + 16.0f, origin.x + size.x),
                        std::min(origin.y + y + 16.0f, origin.y + size.y)), lite);

    this->ensurePreviewBuilt();

    if (this->selection.kind == SelKind::BOX)
        this->drawBoxGuides(origin, size);

    if (this->previewView != nullptr)
    {
        // 手动驱动这一棵视图：它不在 App 的视图树里，所以布局/更新/绘制都自己来。
        // frame() 用的是视图自己的（Yoga 算出来的）绝对坐标，所以把它平移到画布原点。
        View* view = this->previewView;
        YGNodeCalculateLayout(view->getYGNode(), size.x, size.y, YGDirectionLTR);
        view->notifyLayout();
        view->update();
        view->setTranslation(origin.x, origin.y);

        FrameContext ctx;
        ctx.drawList   = drawList;
        ctx.pixelRatio = 1.0f;

        drawList->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
        view->frame(&ctx);
        drawList->PopClipRect();
    }

    drawList->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y),
        IM_COL32(90, 95, 103, 255));

    ImGui::Dummy(ImVec2(size.x + 8.0f, size.y + 8.0f));
    ImGui::EndChild();

    if (!this->previewError.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f), "预览编译失败：%s",
            this->previewError.c_str());
}

// ---------------------------------------------------------------- 属性表

void DesignerWindow::drawProps()
{
    if (this->currentWidget.empty() && this->currentViewFile.empty())
    {
        ImGui::TextColored(kMuted, "选一个文件");
        return;
    }

    if (!this->currentViewFile.empty())
    {
        ImGui::TextColored(kAccent, "页面视图 %s", this->currentViewFile.c_str());
        ImGui::TextColored(kMuted, "P2c 才开放编辑；现在可以看整页预览与原始 JSON：");
        ImGui::Separator();
        const std::string text = this->doc.root().is_discarded() ? this->savedText
                                                                 : this->doc.root().dump(2);
        ImGui::TextUnformatted(text.c_str());
        return;
    }

    ImGui::TextColored(kAccent, "外观 %s", this->currentWidget.c_str());
    ImGui::TextColored(kMuted, "%s", this->currentFilePath.c_str());
    ImGui::Separator();

    auto edited = [this] {
        this->markEdited();
        this->previewDirty = true;
    };

    switch (this->selection.kind)
    {
        case SelKind::BOX:
        case SelKind::FILE:
        {
            ImGui::TextColored(kMuted, "盒模型");
            const char* numberKeys[] = { "height", "minWidth", "radius", "paddingTop",
                "paddingSides", "gap" };
            for (const char* key : numberKeys)
            {
                double value = 0.0;
                const bool has = this->doc.getBoxNumber(key, value);
                const std::string label
                    = has ? std::string(key) : (std::string(key) + "（未设置）");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputDouble(label.c_str(), &value, 1.0, 10.0, "%.1f") && has)
                {
                    this->doc.setBoxNumber(key, value);
                    edited();
                }
            }

            ImGui::Separator();
            ImGui::TextColored(kMuted, "布局");
            std::string axis = "column";
            this->doc.getBoxString("axis", axis);
            int axisIndex = (axis == "row") ? 1 : 0;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("axis", &axisIndex, "column\0row\0"))
            {
                this->doc.setBoxString("axis", axisIndex == 1 ? "row" : "column");
                edited();
            }

            std::string justify = "flexStart";
            this->doc.getBoxString("justifyContent", justify);
            const char* justifyItems[]
                = { "flexStart", "center", "flexEnd", "spaceBetween", "spaceAround", "spaceEvenly" };
            int justifyIndex = 0;
            for (int i = 0; i < 6; i++)
                if (justify == justifyItems[i])
                    justifyIndex = i;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("justifyContent", &justifyIndex,
                    "flexStart\0center\0flexEnd\0spaceBetween\0spaceAround\0spaceEvenly\0"))
            {
                this->doc.setBoxString("justifyContent", justifyItems[justifyIndex]);
                edited();
            }

            std::string align = "flexStart";
            this->doc.getBoxString("alignItems", align);
            const char* alignItems[] = { "flexStart", "center", "flexEnd", "stretch", "baseline" };
            int alignIndex = 0;
            for (int i = 0; i < 5; i++)
                if (align == alignItems[i])
                    alignIndex = i;
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("alignItems", &alignIndex,
                    "flexStart\0center\0flexEnd\0stretch\0baseline\0"))
            {
                this->doc.setBoxString("alignItems", alignItems[alignIndex]);
                edited();
            }

            ImGui::Separator();
            ImGui::TextColored(kMuted, "颜色（可直接写 @theme/color.xxx）");
            const char* colorKeys[] = { "backgroundColor", "borderColor" };
            for (const char* key : colorKeys)
            {
                std::string value;
                if (!this->doc.getBoxString(key, value))
                    value.clear();
                ImGui::SetNextItemWidth(-1);
                if (inputString(key, value))
                {
                    this->doc.setBoxString(key, value);
                    edited();
                }
            }
            break;
        }

        case SelKind::LAYER:
        case SelKind::LAYER_FIELD:
        {
            AppearanceDoc::Json* layer = this->doc.layer(this->selection.index);
            if (layer == nullptr)
                break;

            ImGui::TextColored(kMuted, "第 %d 层 · %s", this->selection.index + 1,
                this->doc.layerOp(this->selection.index).c_str());

            for (auto it = layer->begin(); it != layer->end(); ++it)
            {
                const std::string key = it.key();
                ImGui::PushID(key.c_str());
                const std::string kind = valueKind(it.value());
                bool changed = false;

                if (kind == "number")
                {
                    double value = it.value().get<double>();
                    ImGui::SetNextItemWidth(-1);
                    changed = inputNumberFloat(key.c_str(), value);
                    if (changed)
                        it.value() = value;
                }
                else if (kind == "bool")
                {
                    bool value = it.value().get<bool>();
                    changed = ImGui::Checkbox(key.c_str(), &value);
                    if (changed)
                        it.value() = value;
                }
                else if (kind == "array")
                {
                    std::string value;
                    if (it.value().size() > 0)
                        value = compactValue(it.value());
                    ImGui::SetNextItemWidth(-1);
                    if (inputString(key.c_str(), value) && !value.empty())
                    {
                        // 允许 [1,2] / 数字 两种写法，解析失败就原样留着
                        AppearanceDoc::Json parsed
                            = AppearanceDoc::Json::parse(value, nullptr, false);
                        if (!parsed.is_discarded())
                        {
                            it.value() = parsed;
                            changed    = true;
                        }
                    }
                }
                else
                {
                    std::string value = kind == "string" ? it.value().get<std::string>()
                                                        : compactValue(it.value());
                    ImGui::SetNextItemWidth(-1);
                    changed = inputString(key.c_str(), value);
                    if (changed)
                        it.value() = value;
                }

                if (changed)
                    edited();
                ImGui::PopID();
            }

            ImGui::Separator();
            ImGui::TextColored(kMuted, "层操作");
            if (ImGui::Button("上移"))
            {
                if (this->doc.moveLayer(this->selection.index, -1))
                    this->selection.index -= 1;
                edited();
            }
            ImGui::SameLine();
            if (ImGui::Button("下移"))
            {
                if (this->doc.moveLayer(this->selection.index, 1))
                    this->selection.index += 1;
                edited();
            }
            ImGui::SameLine();
            if (ImGui::Button("复制"))
            {
                const int created = this->doc.duplicateLayer(this->selection.index);
                if (created >= 0)
                    this->selection.index = created;
                edited();
            }
            ImGui::SameLine();
            if (ImGui::Button("删除"))
            {
                this->doc.removeLayer(this->selection.index);
                this->selection = Selection{};
                this->selection.kind = SelKind::FILE;
                edited();
            }
            break;
        }

        case SelKind::STATE:
        case SelKind::CHANNEL:
        {
            const std::string state = this->selection.state;
            ImGui::TextColored(kMuted, "状态 %s", state.c_str());

            if (this->doc.root()["states"].contains(state))
            {
                AppearanceDoc::Json& node = this->doc.root()["states"][state];
                int index = 0;
                for (auto it = node.begin(); it != node.end(); ++it, ++index)
                {
                    ImGui::PushID(index);
                    ImGui::TextColored(kAccent, "%s", it.key().c_str());
                    if (it.value().is_object())
                    {
                        AppearanceDoc::Json& transition = it.value();

                        std::string to = compactValue(transition.value("to", AppearanceDoc::Json()));
                        ImGui::SetNextItemWidth(-1);
                        if (inputString("to", to))
                        {
                            AppearanceDoc::Json parsed
                                = AppearanceDoc::Json::parse(to, nullptr, false);
                            transition["to"] = parsed.is_discarded() ? AppearanceDoc::Json(to)
                                                                     : parsed;
                            edited();
                        }

                        double duration = transition.value("duration", 120.0);
                        ImGui::SetNextItemWidth(-1);
                        if (inputNumberFloat("duration(ms)", duration))
                        {
                            transition["duration"] = duration;
                            edited();
                        }

                        std::string easing = transition.value("easing", std::string("linear"));
                        const char* easings[] = { "linear", "quadraticIn", "quadraticOut",
                            "quadraticInOut", "cubicOut", "backOut", "bounceOut", "elasticOut" };
                        int easingIndex = 0;
                        for (int i = 0; i < 8; i++)
                            if (easing == easings[i])
                                easingIndex = i;
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::Combo("easing", &easingIndex,
                                "linear\0quadraticIn\0quadraticOut\0quadraticInOut\0cubicOut\0"
                                "backOut\0bounceOut\0elasticOut\0"))
                        {
                            transition["easing"] = easings[easingIndex];
                            edited();
                        }
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
            break;
        }

        default:
            ImGui::TextColored(kMuted, "选中层 / box / 状态看属性");
            break;
    }

    ImGui::Separator();
    ImGui::TextColored(kMuted, "layers 增删（顺序就是绘制顺序：hlBg 在内容前，hlGlow 最后）");
    const char* ops[] = { "rect", "text", "image", "shadow", "custom" };
    for (int i = 0; i < 5; i++)
    {
        if (i > 0)
            ImGui::SameLine();
        if (ImGui::SmallButton((std::string("+ ") + ops[i]).c_str()))
        {
            const int created = this->doc.addLayer(ops[i]);
            this->selection = Selection{};
            this->selection.kind  = SelKind::LAYER;
            this->selection.index = created;
            edited();
        }
    }
}

// ---------------------------------------------------------------- 保存

void DesignerWindow::markEdited()
{
    this->dirty = true;
    this->statusMessage = "已修改（未保存）";
}

void DesignerWindow::applyThemeFromProject(std::string& error)
{
    if (!loadThemeIntoGlobalFromDirectory(this->project.themeDir(), this->project.name))
        error = "主题装载失败：" + this->project.themeDir();
}

void DesignerWindow::requestSave()
{
    const std::string current = this->doc.serialize();
    AppearanceDoc::Json before = AppearanceDoc::Json::parse(this->savedText, nullptr, false);
    AppearanceDoc::Json after  = AppearanceDoc::Json::parse(current, nullptr, false);
    if (before.is_discarded() || after.is_discarded())
    {
        this->errorMessage = "JSON 解析失败，先修语法";
        return;
    }

    const std::vector<JsonChange> changes = diffJson(before, after);
    this->diffLines.clear();
    for (const JsonChange& change : changes)
        this->diffLines.push_back(changeSummary(change));

    if (this->diffLines.empty())
    {
        this->statusMessage = "没有改动";
        return;
    }
    this->showDiff = true;
}

void DesignerWindow::doSave()
{
    this->showDiff = false;

    const std::string text = this->doc.serialize();

    std::string error;
    if (!validateJsonText(text, error))
    {
        this->errorMessage = error;
        return;
    }

    // 保存前先编译一遍：写坏的外观不落盘
    std::shared_ptr<const Appearance> compiled = Appearance::compile(text, getTheme(), error);
    if (compiled == nullptr)
    {
        this->errorMessage = "编译失败（没写盘）：" + error;
        return;
    }

    if (!writeTextFileAtomic(this->currentFilePath, text, true, error))
    {
        this->errorMessage = error;
        return;
    }

    this->savedText = text;
    this->dirty     = false;

    // 热重载：重新读盘 + 清外观缓存，然后把预览重建
    std::string reloadError;
    this->applyThemeFromProject(reloadError);
    this->previewDirty = true;
    this->statusMessage = reloadError.empty() ? "已写入（含 .bak）并热重载" : reloadError;
}

void DesignerWindow::revert()
{
    if (this->currentFilePath.empty())
        return;
    std::string error;
    if (this->openAppearance(this->currentWidget, error))
        this->statusMessage = "已还原到上次保存";
    else
        this->errorMessage = error;
}

// ---------------------------------------------------------------- 弹层与主循环

void DesignerWindow::drawModals()
{
    if (this->showNewProject)
    {
        ImGui::OpenPopup("新建项目");
        this->showNewProject = false;
    }

    if (ImGui::BeginPopupModal("新建项目", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::RadioButton("用模板新建主题（原主题不动）", &this->newProjectMode, 0);
        ImGui::RadioButton("直接编辑现有主题", &this->newProjectMode, 1);
        ImGui::Separator();

        if (this->newProjectMode == 0)
        {
            ImGui::InputText("模板主题", this->newTemplate, sizeof(this->newTemplate));
            ImGui::InputText("新主题名", this->newName, sizeof(this->newName));
        }
        else
        {
            ImGui::InputText("主题名（default / ocean 不允许）", this->newName, sizeof(this->newName));
        }
        ImGui::InputText("主题根目录（.../resources/theme）", this->themeRoot,
            sizeof(this->themeRoot));

        ImGui::TextColored(kMuted, "新建 = 复制模板并在 themes.json 里登记；直接编辑只对自建主题开放");

        if (ImGui::Button("创建", ImVec2(120, 0)))
        {
            std::string error;
            Project created;
            bool ok = false;
            if (this->newProjectMode == 0)
                ok = createProjectFromTemplate(this->themeRoot, this->newTemplate, this->newName,
                    created, error);
            else
                ok = openProjectInPlace(this->themeRoot, this->newName, created, error);

            if (ok)
            {
                if (this->setProject(created, false, error))
                {
                    this->statusMessage = "项目已创建：" + created.name;
                    ImGui::CloseCurrentPopup();
                }
                else
                    this->errorMessage = error;
            }
            else
                this->errorMessage = error;
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();

        if (!this->errorMessage.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f), "%s", this->errorMessage.c_str());

        ImGui::EndPopup();
    }

    if (this->showDiff)
    {
        ImGui::OpenPopup("保存前确认改动");
        this->showDiff = false;
    }

    if (ImGui::BeginPopupModal("保存前确认改动", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(kMuted, "写入前会先编译校验；不通过就不写盘。会留一份 .bak。");
        ImGui::Separator();
        for (const std::string& line : this->diffLines)
            ImGui::TextUnformatted(line.c_str());
        ImGui::Separator();
        if (ImGui::Button("写入", ImVec2(120, 0)))
            this->doSave();
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void DesignerWindow::draw()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    // 顶层窗口不滚动：里面的四块自己滚（否则整窗一滚，顶栏与画布就跑到视野外了）
    ImGui::Begin("bklv 设计器", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse);

    static int probeFrames = 0;
    if (++probeFrames == 6)
        BKLV_LOG_INFO("[探针] 主窗 pos=(%.0f,%.0f) size=%.0fx%.0f scrollY=%.1f avail=(%.0f,%.0f)",
            ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowSize().x,
            ImGui::GetWindowSize().y, ImGui::GetScrollY(), ImGui::GetContentRegionAvail().x,
            ImGui::GetContentRegionAvail().y);

    this->drawTopBar();

    const float leftWidth = 300.0f;
    const float rightWidth = 360.0f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();

    ImGui::BeginChild("##left", ImVec2(leftWidth, 0), true);
    {
        ImGui::TextColored(kAccent, "文件");
        ImGui::BeginChild("##files", ImVec2(0, 220), false);
        this->drawFileList();
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::TextColored(kAccent, "节点树");
        ImGui::BeginChild("##tree", ImVec2(0, ImGui::GetContentRegionAvail().y - 74), false);
        this->drawTree();
        ImGui::EndChild();
        ImGui::Separator();
        this->drawStatus();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##preview", ImVec2(avail.x - leftWidth - rightWidth - 16.0f, 0), true);
    if (probeFrames == 6)
        BKLV_LOG_INFO("[探针] 预览面板 pos=(%.0f,%.0f) size=%.0fx%.0f scroll=(%.1f,%.1f)",
            ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowSize().x,
            ImGui::GetWindowSize().y, ImGui::GetScrollX(), ImGui::GetScrollY());
    this->drawPreview();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##props", ImVec2(rightWidth, 0), true);
    this->drawProps();
    ImGui::EndChild();

    this->drawModals();
    ImGui::End();
}

} // namespace designer

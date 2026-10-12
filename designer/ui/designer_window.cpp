#include "designer/ui/designer_window.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "bklv/3rd/imgui/imgui.h"
#include "designer/core/diff.hpp"
#include "designer/core/theme_edit.hpp"
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

// 字段速查：属性表标签悬停时显示（讲清"这个字段干什么 / 能写什么"）
const char* helpTextFor(const char* key)
{
    static const std::pair<const char*, const char*> help[] = {
        { "box", "盒模型与布局：只有写了的字段才会套用，空 box 不会冲掉控件默认值" },
        { "height", "数字按 720p 缩放；也可写 \"auto\" / \"50%\" / @theme/metric.a.b" },
        { "radius", "圆角半径（会被 UI 缩放乘）" },
        { "paddingTop", "上下内边距（数字或 @theme/metric...）" },
        { "paddingSides", "左右内边距" },
        { "gap", "子节点间距" },
        { "axis", "主轴方向：row / column（AppearanceView 默认 column，横向控件要写 row）" },
        { "justifyContent", "主轴对齐：flexStart / center / flexEnd / spaceBetween / spaceAround / spaceEvenly" },
        { "alignItems", "交叉轴对齐：flexStart / center / flexEnd / stretch / baseline" },
        { "backgroundColor", "底色，@theme/color.x.y 或 #RRGGBB[AA]" },
        { "layers", "绘制结构，顺序就是绘制顺序：hlBg 在内容层前，shadow 在底色前，hlGlow/hlBorder 最后" },
        { "op", "层类型：rect / text / image / shadow / custom（custom 的语义绘制在 C++ 里）" },
        { "fill", "填充色（shadow 层里是阴影浓度）" },
        { "inset", "[上,右,下,左] 相对控件范围的收缩；负数=向外扩" },
        { "shadow", "柔化投影：fill 浓度 / feather 羽化 / spread 外扩 / offset 位移 / cutout 挖空" },
        { "cutout", "挖空：只画外圈，内圈一个顶点都不落，后面加半透明底也不会透出阴影" },
        { "states", "状态动效：通道写 <层id>.<属性名>，to/duration(ms)/easing" },
    };;
    for (const auto& entry : help)
        if (std::strcmp(entry.first, key) == 0)
            return entry.second;
    return nullptr;
}

// 属性表统一排版：标签画在左列，控件占右边剩余宽度。
// 直接把 label 交给 ImGui 画在控件右侧的话，控件一宽标签就被顶出面板（踩过）。
constexpr float kLabelWidth = 148.0f;

void propLabel(const char* label)
{
    ImGui::TextColored(kMuted, "%s", label);
    if (const char* help = helpTextFor(label))
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", help);
    ImGui::SameLine(kLabelWidth);
    ImGui::SetNextItemWidth(-1.0f);
}

// 不要 +/- 按钮：窄栏里那对按钮会把后面的内容顶出可视区
bool inputNumberFloat(const char* label, double& value)
{
    propLabel(label);
    const bool changed = ImGui::InputScalar((std::string("##") + label).c_str(),
        ImGuiDataType_Double, &value, nullptr, nullptr, "%.2f");
    return changed;
}

// "#RRGGBB" / "#RRGGBBAA" → ImU32（解析不了返回 0）
ImU32 parseHexColor(const std::string& text)
{
    if (text.size() < 7 || text[0] != '#')
        return 0;
    unsigned int value = 0;
    if (std::sscanf(text.c_str() + 1, "%x", &value) != 1)
        return 0;
    if (text.size() >= 9)
        return IM_COL32((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    return IM_COL32((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 255);
}

// ColorEdit4 要 float[4]
std::array<float, 4> colorToArray(const std::string& text)
{
    const ImVec4 color = ImGui::ColorConvertU32ToFloat4(parseHexColor(text));
    return { color.x, color.y, color.z, color.w };
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
    {
        // 记住上次的项目，下次启动直接打开（以前每次都是 default，白点一遍）
        std::string saveError;
        saveProjectFile(value, resourcePath("designer_project.json"), saveError);
    }
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

    // 已经有草稿：直接切回去（不重读磁盘，未保存的改动不能丢——审查时发现原实现会丢）
    const auto existing = this->drafts.find(path);
    if (existing != this->drafts.end())
    {
        this->currentWidget = widgetName;
        this->currentViewFile.clear();
        this->currentTable.clear();
        this->currentFilePath = path;
        this->doc             = existing->second.doc;
        this->savedText       = existing->second.savedText;
        this->dirty           = existing->second.dirty;
        this->viewTextApplied = existing->second.savedText;
        std::snprintf(this->viewText, sizeof(this->viewText), "%s",
            existing->second.viewText.c_str());
        this->selection     = Selection{};
        this->previewDirty  = true;
        this->errorMessage.clear();
        this->lastFrameText = this->doc.serialize();
        this->statusMessage = "切回 " + widgetName + (this->dirty ? "（有未保存改动）" : "");
        return true;
    }

    std::string text;
    if (!readTextFile(path, text, error))
        return false;

    if (!this->doc.parse(text, error))
        return false;

    this->currentWidget    = widgetName;
    this->currentViewFile.clear();
    this->currentTable.clear();
    this->currentFilePath  = path;
    this->savedText        = text;
    this->dirty            = false;
    this->selection        = Selection{};
    this->previewDirty     = true;
    this->errorMessage.clear();
    this->statusMessage    = "打开 " + widgetName;

    // 这份文件的第一份草稿：记指纹 + 把当前内容压进撤销栈的第 0 步
    Draft& draft = this->drafts[path];
    draft.savedText = text;
    draft.openHash  = this->hashText(text);
    draft.dirty     = false;
    draft.undo.clear();
    draft.redo.clear();
    draft.undo.push_back(text);
    this->lastFrameText = text;
    return true;
}

bool DesignerWindow::openViewFile(const std::string& fileName)
{
    const std::string path = this->project.resourcesDir() + "/views/" + fileName;

    const auto existing = this->drafts.find(path);
    if (existing != this->drafts.end())
    {
        this->currentViewFile = fileName;
        this->currentWidget.clear();
        this->currentTable.clear();
        this->currentFilePath = path;
        this->doc             = existing->second.doc;
        this->savedText       = existing->second.savedText;
        this->dirty           = existing->second.dirty;
        std::snprintf(this->viewText, sizeof(this->viewText), "%s",
            existing->second.viewText.c_str());
        this->viewTextApplied = existing->second.viewText;
        this->selection       = Selection{};
        this->previewDirty    = true;
        this->lastFrameText   = this->doc.serialize();
        this->statusMessage   = "切回 " + fileName + (this->dirty ? "（有未保存改动）" : "");
        return true;
    }

    this->currentViewFile = fileName;
    this->currentWidget.clear();
    this->currentTable.clear();
    this->currentFilePath = path;

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
    std::snprintf(this->viewText, sizeof(this->viewText), "%s", text.c_str());
    this->viewTextApplied = text;
    Draft& draft = this->drafts[this->currentFilePath];
    draft.savedText = text;
    draft.openHash  = this->hashText(text);
    draft.dirty     = false;
    draft.undo.clear();
    draft.redo.clear();
    draft.undo.push_back(text);
    draft.viewText = text;
    this->lastFrameText = text;
    this->selection    = Selection{};
    this->selection.kind = SelKind::VIEW;
    this->previewDirty = true;
    this->dirty        = false;
    this->statusMessage = "打开 " + fileName;
    return true;
}

bool DesignerWindow::openTableFile(const std::string& fileName)
{
    const std::string path = this->project.themeDir() + "/" + fileName;

    const auto existing = this->drafts.find(path);
    if (existing != this->drafts.end())
    {
        this->currentTable    = fileName;
        this->currentWidget.clear();
        this->currentViewFile.clear();
        this->currentFilePath = path;
        this->doc             = existing->second.doc;
        this->savedText       = existing->second.savedText;
        this->dirty           = existing->second.dirty;
        this->selection       = Selection{};
        this->selection.kind  = SelKind::TABLE;
        this->previewDirty    = true;
        this->lastFrameText   = this->doc.serialize();
        this->statusMessage   = "切回主题表 " + fileName;
        return true;
    }

    std::string text;
    std::string error;
    if (!readTextFile(path, text, error))
    {
        this->errorMessage = error;
        return false;
    }

    const AppearanceDoc::Json parsed = AppearanceDoc::Json::parse(text, nullptr, false);
    if (parsed.is_discarded())
    {
        this->errorMessage = fileName + " 不是合法 JSON";
        return false;
    }

    if (!this->currentFilePath.empty() && this->currentFilePath != path)
        this->stashCurrent();

    this->currentTable     = fileName;
    this->currentWidget.clear();
    this->currentViewFile.clear();
    this->currentFilePath  = path;
    this->savedText        = text;
    this->doc.root()       = parsed;
    this->dirty            = false;
    this->selection        = Selection{};
    this->selection.kind   = SelKind::TABLE;
    this->previewDirty     = true;
    this->lastFrameText    = text;

    Draft& draft    = this->drafts[path];
    draft.savedText = text;
    draft.openHash  = this->hashText(text);
    draft.dirty     = false;
    draft.undo.clear();
    draft.redo.clear();
    draft.undo.push_back(text);
    this->statusMessage = "打开主题表 " + fileName;
    return true;
}

bool DesignerWindow::createAppearanceFile(const std::string& name, const std::string& templateName,
    std::string& error)
{
    if (name.empty())
    {
        error = "名字不能为空";
        return false;
    }

    AppearanceDoc::Json doc = AppearanceDoc::Json::object();
    doc["box"]    = AppearanceDoc::Json::object();
    doc["layers"] = AppearanceDoc::Json::array();
    doc["states"] = AppearanceDoc::Json::object();

    auto rectLayer = [](const char* id, const char* fill) {
        AppearanceDoc::Json layer = AppearanceDoc::Json::object();
        layer["op"]     = "rect";
        layer["id"]     = id;
        layer["fill"]   = fill;
        layer["radius"] = "@theme/metric.radius.md";
        layer["inset"]  = AppearanceDoc::Json::array({ 0, 0, 0, 0 });
        return layer;
    };

    if (templateName == "button")
    {
        doc["box"]["height"]        = "@theme/metric.control.height";
        doc["box"]["axis"]          = "row";
        doc["box"]["justifyContent"] = "center";
        doc["box"]["alignItems"]    = "center";
        doc["layers"].push_back(rectLayer("bg", "#3A3F47FF"));
        AppearanceDoc::Json text = AppearanceDoc::Json::object();
        text["op"]       = "text";
        text["id"]       = "title";
        text["source"]   = "text";
        text["color"]    = "@theme/color.text.primary";
        text["fontSize"] = "@theme/metric.font.size.md";
        text["align"]    = "center";
        text["valign"]   = "center";
        doc["layers"].push_back(text);
    }
    else if (templateName == "cell")
    {
        doc["box"]["height"]         = 70;
        doc["box"]["axis"]           = "row";
        doc["box"]["alignItems"]     = "center";
        doc["box"]["paddingSides"]   = 16;
        doc["box"]["paddingTop"]     = 12.5;
        doc["layers"].push_back(rectLayer("bg", "#00000000"));
        AppearanceDoc::Json text = AppearanceDoc::Json::object();
        text["op"]      = "text";
        text["id"]      = "title";
        text["source"]  = "title";
        text["color"]   = "@theme/color.text.primary";
        text["fontSize"] = 22;
        text["valign"]  = "center";
        doc["layers"].push_back(text);
    }
    else if (templateName == "card")
    {
        doc["box"]["radius"]        = 12;
        doc["box"]["paddingSides"]  = 16;
        doc["box"]["paddingTop"]    = 14;
        doc["box"]["backgroundColor"] = "@theme/color.surface.card";
        doc["layers"].push_back(rectLayer("bg", "@theme/color.surface.card"));
    }
    // "empty" 就是上面那份空壳

    // 先编译一遍：模板里引用了不存在的主题 key 要当场报错，不能落一个坏文件
    {
        std::string compileError;
        if (Appearance::compile(doc.dump(), getTheme(), compileError) == nullptr)
        {
            error = "模板编译失败（没写盘）：" + compileError;
            return false;
        }
    }

    // core 层做：防覆盖（同名拒绝）+ 登记 theme.json + 失败回滚
    if (!createWidgetAppearance(this->project.themeDir(), name, doc.dump(2) + "\n", error))
        return false;

    this->refreshFileList();
    std::string reloadError;
    this->applyThemeFromProject(reloadError);
    std::string openError;
    this->openAppearance(name, openError);
    this->statusMessage = "新建外观 " + name + "（已登记进 theme.json）";
    return true;
}

bool DesignerWindow::deleteCurrentAppearance(std::string& error)
{
    if (this->currentWidget.empty())
    {
        error = "当前没有打开外观文件";
        return false;
    }

    const std::string relative = widgetPathFromName(this->currentWidget);
    const std::string path     = this->project.themeDir() + "/" + relative;

    // core 层：摘掉登记 + 把文件改名成 .deleted（不是直接删，手抖还能捞回来）
    if (!deleteWidgetAppearance(this->project.themeDir(), this->currentWidget, error))
        return false;

    this->drafts.erase(path);
    this->currentWidget.clear();
    this->currentFilePath.clear();
    this->currentTable.clear();
    this->dirty       = false;
    this->previewDirty = true;
    this->refreshFileList();
    this->applyThemeFromProject(error);
    this->statusMessage = "已删除外观 " + relative + "（文件改名为 .deleted，登记已摘掉）";
    return true;
}

bool DesignerWindow::colorPickerPopup(const char* id, std::string& value)
{
    bool changed = false;
    ImGui::SameLine();
    if (ImGui::SmallButton((std::string("取色##") + id).c_str()))
        ImGui::OpenPopup((std::string("##picker") + id).c_str());

    if (ImGui::BeginPopup((std::string("##picker") + id).c_str()))
    {
        // 当前是 @theme 引用时，取色器改的是"字面量"，等于解引用后写死
        std::array<float, 4> color = { 0.0f, 0.0f, 0.0f, 1.0f };
        {
            const ImVec4 converted = ImGui::ColorConvertU32ToFloat4(parseHexColor(value));
            color = { converted.x, converted.y, converted.z, converted.w };
        }
        if (ImGui::ColorPicker4("##pick", color.data(),
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf))
        {
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X",
                (int)(color[0] * 255.0f + 0.5f), (int)(color[1] * 255.0f + 0.5f),
                (int)(color[2] * 255.0f + 0.5f), (int)(color[3] * 255.0f + 0.5f));
            value   = buffer;
            changed = true;
        }
        ImGui::TextColored(kMuted, "取色会写成 #RRGGBBAA 字面量（不再跟主题走）");
        ImGui::EndPopup();
    }
    return changed;
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

// ---------------------------------------------------------------- 草稿与撤销

Draft& DesignerWindow::currentDraft()
{
    return this->drafts[this->currentFilePath];
}

std::string DesignerWindow::hashText(const std::string& text) const
{
    // FNV-1a：只为"这个文件被外部改过没有"用，不要求抗碰撞
    unsigned long long hash = 1469598103934665603ULL;
    for (const char c : text)
    {
        hash ^= (unsigned char)c;
        hash *= 1099511628211ULL;
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%016llx", hash);
    return buffer;
}

// 把当前编辑状态存进 drafts（切文件 / 保存前都会调）
void DesignerWindow::stashCurrent()
{
    if (this->currentFilePath.empty())
        return;

    Draft& draft      = this->drafts[this->currentFilePath];
    draft.doc         = this->doc;
    draft.savedText   = this->savedText;
    draft.dirty       = this->dirty;
    draft.viewText    = this->viewText;
    if (draft.openHash.empty())
        draft.openHash = this->hashText(this->savedText);
}

void DesignerWindow::restoreDraftFor(const std::string& path)
{
    const auto found = this->drafts.find(path);
    if (found == this->drafts.end())
        return;

    this->doc        = found->second.doc;
    this->savedText  = found->second.savedText;
    this->dirty      = found->second.dirty;
    std::snprintf(this->viewText, sizeof(this->viewText), "%s", found->second.viewText.c_str());
    this->previewDirty = true;
}

// 带"未保存改动"拦截的切换：脏了就弹确认，不脏直接切
void DesignerWindow::switchTo(const std::string& path, const std::string& widgetName,
    const std::string& viewFile)
{
    if (this->dirty && !this->currentFilePath.empty() && path != this->currentFilePath)
    {
        this->pendingPath       = path;
        this->pendingWidget     = widgetName;
        this->pendingViewFile   = viewFile;
        this->showUnsavedGuard  = true;
        return;
    }

    if (!this->currentFilePath.empty() && path != this->currentFilePath)
        this->stashCurrent();

    std::string error;
    if (!viewFile.empty())
        this->openViewFile(viewFile);
    else if (!this->openAppearance(widgetName, error))
        this->errorMessage = error;
}

void DesignerWindow::discardCurrentDraft()
{
    this->drafts.erase(this->currentFilePath);
}

bool DesignerWindow::fileChangedExternally() const
{
    if (this->currentFilePath.empty())
        return false;

    std::string onDisk;
    std::string error;
    if (!readTextFile(this->currentFilePath, onDisk, error))
        return false; // 文件都没了，交给写盘那步报错

    const auto found = this->drafts.find(this->currentFilePath);
    const std::string openHash
        = (found != this->drafts.end() && !found->second.openHash.empty())
        ? found->second.openHash
        : this->hashText(this->savedText);
    return this->hashText(onDisk) != openHash;
}

void DesignerWindow::pushUndoSnapshot()
{
    if (this->currentFilePath.empty())
        return;

    Draft& draft    = this->currentDraft();
    const std::string now = this->doc.serialize();
    if (draft.undo.empty() || draft.undo.back() != now)
    {
        draft.undo.push_back(now);
        if (draft.undo.size() > 64)
            draft.undo.erase(draft.undo.begin()); // 只留最近 64 步
        draft.redo.clear();
    }
}

void DesignerWindow::undo()
{
    if (this->currentFilePath.empty())
        return;
    Draft& draft = this->currentDraft();
    if (draft.undo.size() < 2)
    {
        this->statusMessage = "没有可撤销的步骤";
        return;
    }

    draft.redo.push_back(draft.undo.back());
    draft.undo.pop_back();

    std::string error;
    this->doc.parse(draft.undo.back(), error);
    this->viewTextApplied = this->doc.serialize();
    std::snprintf(this->viewText, sizeof(this->viewText), "%s", this->viewTextApplied.c_str());
    this->dirty        = true;
    this->previewDirty = true;
    this->statusMessage = "已撤销";
}

void DesignerWindow::redo()
{
    if (this->currentFilePath.empty())
        return;
    Draft& draft = this->currentDraft();
    if (draft.redo.empty())
    {
        this->statusMessage = "没有可重做的步骤";
        return;
    }

    const std::string text = draft.redo.back();
    draft.redo.pop_back();
    draft.undo.push_back(text);

    std::string error;
    this->doc.parse(text, error);
    this->viewTextApplied = this->doc.serialize();
    std::snprintf(this->viewText, sizeof(this->viewText), "%s", this->viewTextApplied.c_str());
    this->dirty        = true;
    this->previewDirty = true;
    this->statusMessage = "已重做";
}

// ---------------------------------------------------------------- 顶栏

void DesignerWindow::drawTopBar()
{
    // 第一行：标题 + 项目信息（长路径中间省略，别把按钮挤歪）
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::TextUnformatted("bklv 设计器");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    if (this->hasProject)
    {
        std::string root = this->project.themeRoot;
        if (root.size() > 46)
            root = "…" + root.substr(root.size() - 45);
        ImGui::TextColored(kMuted, "%s（%s%s）· %s", this->project.name.c_str(),
            this->project.mode == ProjectMode::FORK ? "模板新建，来自 " : "直接编辑",
            this->project.templateName.c_str(), root.c_str());
        if (this->readOnly)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "· 基准主题只读，改样式请「新建项目」");
        }
    }
    else
        ImGui::TextColored(kMuted, "还没有打开项目");

    // 第二行：动作
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
        this->previewDirty  = true;
        this->statusMessage = "已重新装载主题";
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(this->currentFilePath.empty());
    if (ImGui::Button("还原"))
        this->revert();
    ImGui::SameLine();
    if (ImGui::Button("另存…"))
    {
        std::snprintf(this->saveAsPath, sizeof(this->saveAsPath), "%s",
            this->currentFilePath.c_str());
        this->showSaveAs = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(this->readOnly || this->currentFilePath.empty());
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 1.0f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.03f, 0.13f, 0.11f, 1.0f));
    const bool savePressed = ImGui::Button("保存（先看 diff）");
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
    if (savePressed)
        this->requestSave();

    ImGui::SameLine();
    ImGui::BeginDisabled(this->currentFilePath.empty());
    if (ImGui::Button("撤销"))
        this->undo();
    ImGui::SameLine();
    if (ImGui::Button("重做"))
        this->redo();
    ImGui::EndDisabled();

    ImGui::SameLine();
    {
        int dirtyCount = 0;
        for (const auto& entry : this->drafts)
            if (entry.second.dirty)
                dirtyCount++;
        if (ImGui::Button("改动总览"))
            this->showOverview = true;
        ImGui::SameLine();
        ImGui::TextColored(dirtyCount > 0 ? ImVec4(1.0f, 0.72f, 0.3f, 1.0f) : kMuted,
            dirtyCount > 0 ? "%d 个文件有未保存改动" : "都已同步", dirtyCount);
    }
    ImGui::SameLine();
    if (ImGui::Button("校验面板"))
        this->showWarnings = true;
    ImGui::SameLine();
    if (ImGui::Button("快捷键/约定"))
        this->showHelp = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ctrl+S 保存 · Ctrl+Z/Y 撤销重做 · Ctrl+D 复制层 · Alt+↑↓ 移层 · F1 校验 · F2 预览最大化");

    ImGui::SameLine();
    if (this->dirty)
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "● 当前文件未保存");
    else if (this->readOnly)
        ImGui::TextColored(kMuted, "（只读浏览）");
    else
        ImGui::TextColored(kMuted, "已同步");

    ImGui::Separator();
}

// ---------------------------------------------------------------- 文件列表

void DesignerWindow::drawFileList()
{
    auto fileRow = [this](const std::string& label, bool selected, bool isCurrent, const char* tag) {
        std::string text = (isCurrent && this->dirty ? "● " : "  ") + label;
        if (tag != nullptr)
            text += std::string("   [") + tag + "]";
        if (ImGui::Selectable(text.c_str(), selected))
            return true;
        return false;
    };

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "过滤外观名…", this->fileFilter, sizeof(this->fileFilter));
    ImGui::TextColored(kMuted, "主题外观 · %d 个（%s）", (int)this->widgetFiles.size(),
        this->project.name.c_str());
    for (const std::string& relative : this->widgetFiles)
    {
        const std::string name = widgetNameFromPath(relative);
        const std::string path = this->project.themeDir() + "/" + relative;
        if (this->fileFilter[0] != 0 && name.find(this->fileFilter) == std::string::npos)
            continue;

        const bool isCurrent = (name == this->currentWidget);
        const auto found     = this->drafts.find(path);
        const bool isDirty   = (found != this->drafts.end() && found->second.dirty) || (isCurrent && this->dirty);
        if (fileRow(name, isCurrent, isDirty, nullptr))
            this->switchTo(path, name, std::string());
    }

    ImGui::Separator();
    ImGui::TextColored(kMuted, "页面视图 · %d 个（views/）", (int)this->viewFiles.size());
    if (this->viewFiles.empty())
        ImGui::TextColored(kMuted, "  （这个项目里没有）");
    for (const std::string& file : this->viewFiles)
    {
        const std::string path = this->project.resourcesDir() + "/views/" + file;
        if (this->fileFilter[0] != 0 && file.find(this->fileFilter) == std::string::npos)
            continue;
        const bool isCurrent = (file == this->currentViewFile);
        const auto found     = this->drafts.find(path);
        const bool isDirty   = (found != this->drafts.end() && found->second.dirty) || (isCurrent && this->dirty);
        if (fileRow(file, isCurrent, isDirty, "页面"))
            this->switchTo(path, std::string(), file);
    }

    ImGui::Separator();
    if (!this->readOnly)
    {
        if (ImGui::Button("新建外观…"))
        {
            std::snprintf(this->newAppearanceName, sizeof(this->newAppearanceName), "my_widget");
            this->showNewAppearance = true;
        }
    }
    if (!this->currentWidget.empty() && !this->readOnly)
    {
        ImGui::SameLine();
        if (ImGui::Button("删除当前外观"))
            this->showDeleteAppearance = true;
    }

    ImGui::Separator();
    ImGui::TextColored(kMuted, "主题表（可编辑）");
    for (const char* table : { "colors.json", "metrics.json" })
    {
        const std::string path = this->project.themeDir() + "/" + table;
        const auto found       = this->drafts.find(path);
        const bool isDirty     = found != this->drafts.end() && found->second.dirty;
        if (fileRow(table, table == this->currentTable, isDirty, nullptr))
            this->openTableFile(table);
    }
}

// ---------------------------------------------------------------- 节点树

void DesignerWindow::drawViewTree(const AppearanceDoc::Json& node, const std::string& path)
{
    if (!node.is_object())
        return;

    const std::string type = node.value("type", std::string("(没有 type)"));
    const std::string id   = node.value("id", std::string());
    std::string label      = type;
    if (!id.empty())
        label += "  " + id;

    ImGui::PushID(path.c_str());
    this->viewNodeCount++;

    const bool selected = this->selection.kind == SelKind::VIEW && this->selection.key == path;
    const bool open     = ImGui::TreeNodeEx("##node", ImGuiTreeNodeFlags_DefaultOpen
            | ImGuiTreeNodeFlags_SpanAvailWidth
            | (selected ? ImGuiTreeNodeFlags_Selected : 0), "%s", label.c_str());
    if (ImGui::IsItemClicked())
    {
        this->selection      = Selection{};
        this->selection.kind = SelKind::VIEW;
        this->selection.key  = path;
    }

    if (open)
    {
        // 属性（除了 children）逐条列出来
        for (auto it = node.begin(); it != node.end(); ++it)
        {
            if (it.key() == "children")
                continue;
            this->viewFieldCount++;
            ImGui::TreeNodeEx((void*)(uintptr_t)(path + it.key()).c_str(),
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen
                    | ImGuiTreeNodeFlags_SpanAvailWidth,
                "field  %s = %s", it.key().c_str(), compactValue(it.value()).c_str());
        }

        if (node.contains("children") && node["children"].is_array())
        {
            int index = 0;
            for (const AppearanceDoc::Json& child : node["children"])
            {
                this->drawViewTree(child, path + "/" + std::to_string(index));
                index++;
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void DesignerWindow::drawTree()
{
    // 页面视图：树里显示这棵视图树（用户要求的"逐层展开"）
    if (!this->currentViewFile.empty())
    {
        this->viewNodeCount  = 0;
        this->viewFieldCount = 0;
        this->drawViewTree(this->doc.root(), "$");
        if (this->viewNodeCount <= 0)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f), "这个 JSON 里没有可展开的视图节点");
        return;
    }

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
            // box 的字段不默认展开：十几行字段会把层与状态挤到下面看不见
            const bool boxSelected = this->selection.kind == SelKind::BOX;
            if (ImGui::TreeNodeEx("##box", ImGuiTreeNodeFlags_SpanAvailWidth
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
                    if (this->treeFilter[0] != 0 && key.find(this->treeFilter) == std::string::npos)
                        continue;
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
                if (this->treeFilter[0] != 0
                    && this->doc.layerId(i).find(this->treeFilter) == std::string::npos
                    && this->doc.layerOp(i).find(this->treeFilter) == std::string::npos)
                    continue;
                const bool selected = (this->selection.kind == SelKind::LAYER
                                          || this->selection.kind == SelKind::LAYER_FIELD)
                    && this->selection.index == i;
                char label[128];
                std::snprintf(label, sizeof(label), "#%d  %s  %s", i + 1,
                    this->doc.layerOp(i).c_str(), this->doc.layerId(i).c_str());
                const std::string id = "##layer" + std::to_string(i);
                const bool open = ImGui::TreeNodeEx(id.c_str(), ImGuiTreeNodeFlags_DefaultOpen
                        | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap
                        | (selected ? ImGuiTreeNodeFlags_Selected : 0), "%s", label);
                // 每层一行直接给操作按钮：不用"先选中、再去属性表点"
                {
                    ImGui::SameLine(ImGui::GetWindowWidth() - 96.0f);
                    ImGui::PushID(i);
                    if (ImGui::SmallButton("↑"))
                    {
                        if (this->doc.moveLayer(i, -1) && this->selection.index == i)
                            this->selection.index -= 1;
                        this->markEdited();
                        this->previewDirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("↓"))
                    {
                        if (this->doc.moveLayer(i, 1) && this->selection.index == i)
                            this->selection.index += 1;
                        this->markEdited();
                        this->previewDirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("⧉"))
                    {
                        const int created = this->doc.duplicateLayer(i);
                        this->selection = Selection{};
                        this->selection.kind  = SelKind::LAYER;
                        this->selection.index = created;
                        this->markEdited();
                        this->previewDirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("✕"))
                    {
                        this->doc.removeLayer(i);
                        this->selection = Selection{};
                        this->selection.kind = SelKind::FILE;
                        this->markEdited();
                        this->previewDirty = true;
                    }
                    ImGui::PopID();
                }
                if (open)
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
                            ImGui::TreeNodeEx((void*)(uintptr_t)(id + it.key()).c_str(),
                                leafFlags | ImGuiTreeNodeFlags_NoAutoOpenOnLog,
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

    if (!this->currentViewFile.empty())
    {
        ImGui::TextColored(kAccent, "视图节点 %d", this->viewNodeCount);
        ImGui::SameLine();
        ImGui::TextColored(kMuted, "· 字段 %d", this->viewFieldCount);
    }
    else
    {
        ImGui::TextColored(kAccent, "节点 %d", nodes);
        ImGui::SameLine();
        ImGui::TextColored(kMuted, "· 层 %d · 状态 %d", this->doc.layerCount(),
            (int)this->doc.stateNames().size());
    }

    std::string path = this->project.name;
    if (!this->currentWidget.empty())
        path += " / " + this->currentWidget;
    else if (!this->currentViewFile.empty())
    {
        path += " / " + this->currentViewFile;
        if (this->selection.kind == SelKind::VIEW && !this->selection.key.empty())
            path += " / " + this->selection.key;
    }
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

void DesignerWindow::drawAddFieldCombo(const char* id, const std::vector<std::string>& candidates,
    const std::function<void(const std::string&)>& onPick)
{
    ImGui::SetNextItemWidth(-1);
    if (!ImGui::BeginCombo(id, "+ 添加字段…"))
        return;

    for (const std::string& key : candidates)
        if (ImGui::Selectable(key.c_str()))
            onPick(key);
    ImGui::EndCombo();
}

bool DesignerWindow::colorField(const char* label, std::string& value)
{
    bool changed = false;

    // 色板预览：@theme 引用也解出来给个准色
    ImU32 preview = 0;
    if (value.rfind("@theme/", 0) == 0)
    {
        std::string key = value.substr(7);
        if (key.rfind("color.", 0) == 0)
            key = key.substr(6);
        preview = getTheme().color(key, 0);
    }
    else
        preview = parseHexColor(value); // "#RRGGBB[AA]"

    propLabel(label);

    ImGui::ColorButton((std::string("##sw") + label).c_str(), ImGui::ColorConvertU32ToFloat4(preview),
        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(22, 18));
    ImGui::SameLine();

    // 文本占中间，右边留 92 给"主题 key"下拉
    ImGui::SetNextItemWidth(-96.0f);
    changed |= inputString((std::string("##") + label).c_str(), value);

    changed |= this->colorPickerPopup(label, value);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(88);
    if (ImGui::BeginCombo((std::string("##key") + label).c_str(), "主题 key…"))
    {
        static const char* common[] = { "accent.base", "common.transparent", "common.red",
            "common.orange", "common.yellow", "common.green", "common.cyan", "common.blue",
            "common.purple", "common.grey", "text.primary", "text.disabled", "text.value",
            "surface.card", "surface.page", "button.primaryBg", "highlight.background",
            "highlight.glow", "highlight.border", "common.shadow", "common.divider" };
        for (const char* key : common)
        {
            if (ImGui::Selectable(key))
            {
                value   = std::string("@theme/color.") + key;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    // @theme 引用：显示解出来的色值 + 一键跳到主题表那个 key
    if (value.rfind("@theme/", 0) == 0)
    {
        ImGui::SameLine();
        if (ImGui::SmallButton((std::string("定位##") + label).c_str()))
        {
            this->openTableFile("colors.json");
            std::string key = value.substr(7);
            if (key.rfind("color.", 0) == 0)
                key = key.substr(6);
            std::snprintf(this->tableFilter, sizeof(this->tableFilter), "%s", key.c_str());
        }
    }
    return changed;
}

bool DesignerWindow::applyViewText(std::string& error)
{
    const std::string text = this->viewText;
    if (!validateJsonText(text, error))
        return false;

    // 试实例化：结构不对（未知 type、children 类型错）就别让它进预览
    View* probe = createFromJSONString(text);
    if (probe == nullptr)
    {
        error = "视图实例化失败（看日志里的具体原因：未知 type / children 结构 / include 路径）";
        return false;
    }
    delete probe;

    const AppearanceDoc::Json parsed = AppearanceDoc::Json::parse(text, nullptr, false);
    if (parsed.is_discarded())
    {
        error = "JSON 解析失败";
        return false;
    }
    this->doc.root() = parsed;
    this->viewTextApplied = text;
    this->previewDirty    = true;
    this->markEdited();
    return true;
}

void DesignerWindow::drawTableProps()
{
    ImGui::TextColored(kAccent, "主题表 %s", this->currentTable.c_str());
    ImGui::TextColored(kMuted, "改完「保存」= 写回这个文件 + 热重载（预览立刻变）");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##tableFilter", "过滤 key…", this->tableFilter, sizeof(this->tableFilter));
    ImGui::TextColored(kMuted, "%s", this->currentFilePath.c_str());
    ImGui::Separator();

    bool edited = false;

    // 递归遍历（键是点分层级；$ 开头的是元信息）
    std::function<void(AppearanceDoc::Json&, const std::string&)> walk
        = [&](AppearanceDoc::Json& node, const std::string& prefix) {
              for (auto it = node.begin(); it != node.end(); ++it)
              {
                  const std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();
                  if (!it.key().empty() && it.key()[0] == '$')
                      continue;

                  if (it.value().is_object())
                  {
                      if (ImGui::TreeNodeEx((void*)(uintptr_t)key.c_str(),
                              ImGuiTreeNodeFlags_SpanAvailWidth
                                  | (prefix.empty() ? ImGuiTreeNodeFlags_DefaultOpen : 0),
                              "%s", it.key().c_str()))
                      {
                          walk(it.value(), key);
                          ImGui::TreePop();
                      }
                      continue;
                  }

                  if (this->tableFilter[0] != 0 && key.find(this->tableFilter) == std::string::npos)
                      continue;

                  ImGui::PushID(key.c_str());
                  if (it.value().is_string())
                  {
                      std::string text = it.value().get<std::string>();
                      ImGui::PushItemWidth(120.0f);
                      std::array<float, 4> scratch      = colorToArray(text);
                      const bool colorPicked = ImGui::ColorEdit4("##c", scratch.data(),
                          ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs
                              | ImGuiColorEditFlags_AlphaPreviewHalf);
                      if (colorPicked)
                      {
                          // 必须写回：只置 edited 的话取色器改了等于没改（审查时发现）
                          char hex[16];
                          std::snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X",
                              (int)(scratch[0] * 255.0f + 0.5f),
                              (int)(scratch[1] * 255.0f + 0.5f),
                              (int)(scratch[2] * 255.0f + 0.5f),
                              (int)(scratch[3] * 255.0f + 0.5f));
                          it.value() = std::string(hex);
                          text       = hex;
                          edited     = true;
                      }
                      ImGui::PopItemWidth();
                      ImGui::SameLine();
                      ImGui::SetNextItemWidth(140);
                      if (inputString("##hex", text))
                      {
                          it.value() = text;
                          edited     = true;
                      }
                      ImGui::SameLine();
                      ImGui::Text("%s", key.c_str());
                  }
                  else if (it.value().is_number())
                  {
                      double value = it.value().get<double>();
                      ImGui::PushItemWidth(120.0f);
                      if (ImGui::DragScalar("##n", ImGuiDataType_Double, &value, 0.5, nullptr,
                              nullptr, "%.2f"))
                      {
                          it.value() = value;
                          edited     = true;
                      }
                      ImGui::PopItemWidth();
                      ImGui::SameLine();
                      ImGui::Text("%s", key.c_str());
                  }
                  else
                      ImGui::TextColored(kMuted, "%s = %s", key.c_str(),
                          compactValue(it.value()).c_str());
                  ImGui::PopID();
              }
          };

    walk(this->doc.root(), "");

    if (edited)
    {
        this->markEdited();
        this->previewDirty = true;
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

View* DesignerWindow::buildPreviewView(const std::string& jsonOverride)
{
    // 页面视图：直接实例化那棵视图树（可以只实例化选中的子树）
    if (!this->currentViewFile.empty())
    {
        std::string text = jsonOverride;
        if (text.empty())
        {
            std::string readError;
            readTextFile(this->currentFilePath, text, readError);
        }

        if (this->previewSubtree && this->selection.kind == SelKind::VIEW
            && !this->selection.key.empty())
        {
            // 按树的路径取出那一棵子树（$.$/1/0 这种），单独实例化
            std::string nodeText;
            const AppearanceDoc::Json* node = &this->doc.root();
            std::string path                = this->selection.key;
            size_t start                    = 0;
            while (start < path.size() && node != nullptr)
            {
                const size_t next = path.find('/', start + 1);
                const std::string step = path.substr(start + 1, next - start - 1);
                if (step == "$")
                {
                    node = &this->doc.root();
                }
                else if (node->contains("children") && (*node)["children"].is_array())
                {
                    const int index = std::atoi(step.c_str());
                    if (index < 0 || index >= (int)(*node)["children"].size())
                        node = nullptr;
                    else
                        node = &(*node)["children"][(size_t)index];
                }
                else
                    node = nullptr;

                if (next == std::string::npos)
                    break;
                start = next;
            }
            if (node != nullptr && node->is_object())
                nodeText = node->dump();
            text = nodeText.empty() ? text : nodeText;
        }

        View* created = createFromJSONString(text);
        if (created == nullptr)
            this->previewError = "视图 JSON 实例化失败（未知 type / children 结构 / include 路径）";
        return created;
    }

    if (this->currentWidget.empty())
        return nullptr;

    AppearanceView* control = this->createPreviewControl();
    std::string compileError;
    if (!control->applyAppearanceJson(jsonOverride.empty() ? this->previewJson() : jsonOverride,
            compileError))
    {
        this->previewError = compileError;
        delete control;
        return nullptr;
    }

    // 状态：状态节点 / 状态工具栏都能驱动它（工具栏用真实时长，能看动效过程）
    const std::string state = (this->selection.kind == SelKind::STATE
                                  || this->selection.kind == SelKind::CHANNEL)
        ? this->selection.state
        : std::string();
    if (!state.empty())
        control->setAppearanceState(state, true);
    else
    {
        static const char* toolbarStates[] = { "", "focused", "pressed", "disabled", "selected" };
        const char* wanted = toolbarStates[this->previewStateIndex];
        if (wanted[0] != 0)
            control->setAppearanceState(wanted, true);
    }

    // 样例容器：把控件放进一个真实的横向容器里，看间距/父容器的效果
    if (this->previewContext)
    {
        Box* host = new Box(Axis::ROW);
        host->setGap(12.0f);
        host->setPadding(16.0f, 16.0f, 16.0f, 16.0f);
        host->setAlignItems(AlignItems::CENTER);
        host->addView(control);

        Rectangle* sibling = new Rectangle("card");
        sibling->setWidth(90.0f);
        sibling->setHeight(40.0f);
        host->addView(sibling);

        Rectangle* tail = new Rectangle("card");
        tail->setWidth(90.0f);
        tail->setHeight(40.0f);
        host->addView(tail);
        return host;
    }

    return control;
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

    this->previewView = this->buildPreviewView(std::string());

    if (this->compareView != nullptr)
    {
        delete this->compareView;
        this->compareView = nullptr;
    }
    if (this->previewCompare)
        this->compareView = this->buildPreviewView(this->savedText);
}

void DesignerWindow::drawPreviewCanvas(ImVec2 origin, ImVec2 size, View* view, bool checker)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 底：棋盘格方便看 shadow / 半透明，否则用页面底色
    const ImU32 dark = checker ? IM_COL32(38, 38, 38, 255) : IM_COL32(45, 45, 45, 255);
    const ImU32 lite = checker ? IM_COL32(48, 48, 48, 255) : dark;
    drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), dark);
    if (checker)
        for (int y = 0; y < (int)size.y; y += 16)
            for (int x = 0; x < (int)size.x; x += 16)
                if (((x / 16) + (y / 16)) % 2 == 0)
                    drawList->AddRectFilled(ImVec2(origin.x + (float)x, origin.y + (float)y),
                        ImVec2(std::min(origin.x + x + 16.0f, origin.x + size.x),
                            std::min(origin.y + y + 16.0f, origin.y + size.y)), lite);

    if (view != nullptr)
    {
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
}

void DesignerWindow::drawPreview()
{
    // ---- 控制行 1：画布 ----
    ImGui::TextColored(kAccent, "预览");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("##size", &this->canvasPreset,
        "自适应\0" "1280×720\0" "720×480\0" "640×360\0" "自定义\0");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("画布尺寸。自适应按文件类型给：整页 960×560、单元格 420×160、单层 360×200");

    if (this->canvasPreset == 0)
    {
        if (!this->currentViewFile.empty())
        {
            this->canvasWidth  = 960;
            this->canvasHeight = 560;
        }
        else if (this->selection.kind == SelKind::LAYER)
        {
            this->canvasWidth  = 360;
            this->canvasHeight = 200;
        }
        else if (this->currentWidget.rfind("cell", 0) == 0)
        {
            this->canvasWidth  = 420;
            this->canvasHeight = 160;
        }
        else
        {
            this->canvasWidth  = 420;
            this->canvasHeight = 220;
        }
    }
    else if (this->canvasPreset == 1)
    {
        this->canvasWidth  = 1280;
        this->canvasHeight = 720;
    }
    else if (this->canvasPreset == 2)
    {
        this->canvasWidth  = 720;
        this->canvasHeight = 480;
    }
    else if (this->canvasPreset == 3)
    {
        this->canvasWidth  = 640;
        this->canvasHeight = 360;
    }
    else
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("##w", &this->canvasWidth, 0, 0);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("##h", &this->canvasHeight, 0, 0);
    }
    this->canvasWidth  = std::max(40, std::min(this->canvasWidth, 4000));
    this->canvasHeight = std::max(30, std::min(this->canvasHeight, 4000));

    ImGui::SameLine();
    ImGui::Checkbox("网格底", &this->canvasChecker);
    ImGui::SameLine();
    if (ImGui::SmallButton("重置视图"))
    {
        this->canvasZoom = 2;
        this->canvasPanX = 0.0f;
        this->canvasPanY = 0.0f;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("滚轮缩放、中键（或按住空格）拖动平移");

    const float zoom = this->canvasZoom == 0 ? 0.5f : this->canvasZoom == 1 ? 0.75f
        : this->canvasZoom == 2 ? 1.0f : 1.5f;

    // ---- 控制行 2：状态工具栏 + 上下文/子树/对比 ----
    ImGui::TextColored(kMuted, "状态");
    ImGui::SameLine();
    static const char* stateLabels[] = { "默认", "focused", "pressed", "disabled", "selected" };
    for (int i = 0; i < 5; i++)
    {
        if (i > 0)
            ImGui::SameLine();
        const bool active = (this->previewStateIndex == i);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.8f, 0.64f, 1.0f));
        if (ImGui::SmallButton(stateLabels[i]))
        {
            this->previewStateIndex = i;
            this->previewDirty      = true;
        }
        if (active)
            ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("按状态看效果动效（用真实时长，能看到过程）");

    ImGui::SameLine();
    if (ImGui::Checkbox("样例容器", &this->previewContext))
        this->previewDirty = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("把控件放进一个横向容器里看间距/父容器效果（不再是孤立的一块）");

    if (!this->currentViewFile.empty())
    {
        ImGui::SameLine();
        if (ImGui::Checkbox("只渲染选中子树", &this->previewSubtree))
            this->previewDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("并排对比", &this->previewCompare))
        this->previewDirty = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("左边=正在编辑的，右边=磁盘上那份（改前改后一眼看出差别）");

    // ---- 差异标注：设计器与真机可能不一样的地方，写在明面上 ----
    {
        const float uiScale = (float)this->canvasWidth / 1280.0f;
        ImGui::TextColored(kMuted,
            "画布 %d×%d · UI 缩放按画布宽算 ≈ %.2f×（真机 = 窗口宽/1280）· 主题目录 %s",
            this->canvasWidth, this->canvasHeight, uiScale,
            this->currentTable.empty() && this->project.themeRoot.empty() ? "-"
                                                                         : this->project.themeDir().c_str());
    }

    const char* modeText = "选中「文件」→ 整个控件外观（真实控件）";
    switch (this->selection.kind)
    {
        case SelKind::BOX: modeText = "选中「box」→ 盒模型：虚线标出边界与 padding"; break;
        case SelKind::LAYER: modeText = "选中「某一层」→ 只画这一层"; break;
        case SelKind::STATE:
        case SelKind::CHANNEL: modeText = "选中「某个状态」→ 该状态生效的样子"; break;
        case SelKind::TABLE: modeText = "主题表没有预览（颜色/数值改完直接看控件页）"; break;
        default: break;
    }
    if (!this->currentViewFile.empty())
        modeText = this->previewSubtree ? "页面视图 → 只渲染选中的那一棵子树"
                                        : "页面视图 → 真实实例化整页";
    if (this->previewCompare)
        modeText = "并排对比：左=编辑中 / 右=磁盘上那份";
    ImGui::TextColored(kMuted, "%s", modeText);

    ImGui::BeginChild("##stage", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    // 滚轮缩放 + 中键/空格拖动
    if (ImGui::IsWindowHovered())
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel > 0.0f && this->canvasZoom < 3)
            this->canvasZoom++;
        else if (wheel < 0.0f && this->canvasZoom > 0)
            this->canvasZoom--;
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)
            || (ImGui::GetIO().KeyShift && ImGui::IsMouseDragging(ImGuiMouseButton_Left)))
        {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            this->canvasPanX += delta.x;
            this->canvasPanY += delta.y;
        }
    }

    const ImVec2 stagePos(ImGui::GetCursorScreenPos().x + this->canvasPanX,
        ImGui::GetCursorScreenPos().y + this->canvasPanY);
    const ImVec2 size((float)this->canvasWidth * zoom, (float)this->canvasHeight * zoom);

    this->ensurePreviewBuilt();

    if (this->selection.kind == SelKind::BOX)
        this->drawBoxGuides(stagePos, size);

    if (this->previewCompare)
    {
        const float half = std::max(60.0f, size.x);
        this->drawPreviewCanvas(stagePos, ImVec2(half, size.y), this->previewView,
            this->canvasChecker);
        ImGui::GetWindowDrawList()->AddText(ImVec2(stagePos.x + 6, stagePos.y + 4),
            IM_COL32(0, 255, 204, 255), "编辑中");
        const ImVec2 rightPos(stagePos.x + half + 24.0f, stagePos.y);
        this->drawPreviewCanvas(rightPos, ImVec2(half, size.y), this->compareView,
            this->canvasChecker);
        ImGui::GetWindowDrawList()->AddText(ImVec2(rightPos.x + 6, rightPos.y + 4),
            IM_COL32(255, 183, 77, 255), "磁盘上那份");
        ImGui::Dummy(ImVec2(half * 2.0f + 32.0f, size.y + 8.0f));
    }
    else
    {
        this->drawPreviewCanvas(stagePos, size, this->previewView, this->canvasChecker);
        ImGui::Dummy(ImVec2(size.x + 8.0f, size.y + 8.0f));
    }

    // 鼠标在画布上的坐标（对齐时有用）
    if (ImGui::IsWindowHovered())
    {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float localX = (mouse.x - stagePos.x) / zoom;
        const float localY = (mouse.y - stagePos.y) / zoom;
        if (localX >= 0.0f && localY >= 0.0f && localX <= this->canvasWidth
            && localY <= this->canvasHeight)
            ImGui::SetTooltip("画布坐标 (%.0f, %.0f)", localX, localY);
    }

    ImGui::EndChild();

    if (!this->previewError.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f), "预览编译失败：%s",
            this->previewError.c_str());
}

// ---------------------------------------------------------------- 属性表

void DesignerWindow::drawProps()
{
    if (this->currentWidget.empty() && this->currentViewFile.empty() && this->currentTable.empty())
    {
        ImGui::TextColored(kMuted, "选一个文件");
        return;
    }

    if (!this->currentTable.empty())
    {
        this->drawTableProps();
        return;
    }

    if (!this->currentViewFile.empty())
    {
        ImGui::TextColored(kAccent, "页面视图 %s", this->currentViewFile.c_str());
        ImGui::TextColored(kMuted, "直接改下面的 JSON：点「应用」会先做语法校验 + 试实例化，"
                                   "通过才进预览；「保存」走同一条（diff → 校验 → 原子写 + .bak）。");
        ImGui::Separator();

        const bool enterApplies = ImGui::InputTextMultiline("##viewjson", this->viewText,
            sizeof(this->viewText), ImVec2(-1, ImGui::GetContentRegionAvail().y - 40.0f));

        if (ImGui::Button("应用", ImVec2(90, 0)) || enterApplies)
        {
            std::string error;
            if (this->applyViewText(error))
                this->statusMessage = "已应用到预览（还没写盘）";
            else
                this->errorMessage = error;
        }
        ImGui::SameLine();
        if (ImGui::Button("还原到文件内容", ImVec2(140, 0)))
        {
            std::snprintf(this->viewText, sizeof(this->viewText), "%s",
                this->viewTextApplied.c_str());
            this->statusMessage = "已还原文本";
        }
        ImGui::SameLine();
        ImGui::TextColored(kMuted, "%s", this->dirty ? "● 有未保存改动" : "已同步");

        if (!this->errorMessage.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f), "%s", this->errorMessage.c_str());
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
                propLabel(label.c_str());
                ImGui::PushID(key);
                double dragged = value;
                const bool changedNumber = ImGui::DragScalar("##v", ImGuiDataType_Double,
                    &dragged, 0.5, nullptr, nullptr, "%.1f");
                ImGui::SameLine();
                if (ImGui::SmallButton("✕") && has)
                {
                    this->doc.removeBoxKey(key);
                    edited();
                }
                ImGui::PopID();
                if (changedNumber && has)
                {
                    this->doc.setBoxNumber(key, dragged);
                    edited();
                }
            }

            // box 允许加任何已知字段（以前只能改文件里已经写了的键）
            static const std::vector<std::string> boxCandidates = { "width", "height", "minWidth",
                "minHeight", "maxWidth", "radius", "paddingTop", "paddingSides", "paddingBottom",
                "gap", "grow", "shrink", "borderThickness", "opacity", "marginTop", "marginSides" };
            this->drawAddFieldCombo("##addBoxField", boxCandidates, [this, &edited](const std::string& key) {
                this->doc.setBoxNumber(key, 0.0);
                edited();
            });

            ImGui::Separator();
            ImGui::TextColored(kMuted, "布局");
            std::string axis = "column";
            this->doc.getBoxString("axis", axis);
            int axisIndex = (axis == "row") ? 1 : 0;
            propLabel("axis");
            if (ImGui::Combo("##axis", &axisIndex, "column\0row\0"))
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
            propLabel("justifyContent");
            if (ImGui::Combo("##justifyContent", &justifyIndex,
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
            propLabel("alignItems");
            if (ImGui::Combo("##alignItems", &alignIndex,
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
                if (this->colorField(key, value))
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

            ImGui::TextColored(kMuted, "第 %d 层", this->selection.index + 1);

            // op 可切换（以前只能改 JSON 文本）
            {
                const char* ops[] = { "rect", "text", "image", "shadow", "custom" };
                std::string current = this->doc.layerOp(this->selection.index);
                int opIndex         = 0;
                for (int i = 0; i < 5; i++)
                    if (current == ops[i])
                        opIndex = i;
                propLabel("op");
                if (ImGui::Combo("##op", &opIndex, "rect\0text\0image\0shadow\0custom\0"))
                {
                    (*layer)["op"] = ops[opIndex];
                    edited();
                }
            }

            for (auto it = layer->begin(); it != layer->end(); ++it)
            {
                const std::string key = it.key();
                ImGui::PushID(key.c_str());
                const std::string kind = valueKind(it.value());
                bool changed = false;

                if (kind == "number")
                {
                    double value = it.value().get<double>();
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
                    propLabel(key.c_str());
                    if (inputString((std::string("##") + key).c_str(), value) && !value.empty())
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
                    const bool looksLikeColor = (key == "fill" || key == "color" || key == "stroke"
                        || key == "borderColor" || key == "backgroundColor");
                    if (looksLikeColor)
                    {
                        std::string value = kind == "string" ? it.value().get<std::string>()
                                                             : compactValue(it.value());
                        if (this->colorField(key.c_str(), value))
                        {
                            it.value() = value;
                            changed    = true;
                        }
                    }
                    else
                    {
                        std::string value = kind == "string" ? it.value().get<std::string>()
                                                            : compactValue(it.value());
                        propLabel(key.c_str());
                        changed = inputString((std::string("##") + key).c_str(), value);
                        if (changed)
                            it.value() = value;
                    }
                }

                if (changed)
                    edited();

                // 删字段（id / op 不给删：层没有它们就没法引用）
                if (key != "id" && key != "op")
                {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("✕"))
                    {
                        this->doc.removeLayerKey(this->selection.index, key);
                        edited();
                        ImGui::PopID();
                        break;
                    }
                }
                ImGui::PopID();
            }

            // 按 op 给"可加的字段"清单（字段表不再只是"文件里写了什么"）
            {
                const std::string op = this->doc.layerOp(this->selection.index);
                std::vector<std::string> candidates = { "inset", "offset", "width", "height",
                    "opacity", "align", "valign", "radius" };
                if (op == "text")
                    candidates.insert(candidates.end(),
                        { "source", "text", "color", "fontSize", "wrap", "singleLine" });
                else if (op == "image")
                    candidates.insert(candidates.end(), { "source", "scaling", "tint" });
                else if (op == "shadow")
                    candidates.insert(candidates.end(),
                        { "fill", "feather", "spread", "cutout" });
                else
                    candidates.insert(candidates.end(), { "fill", "stroke", "strokeWidth" });

                this->drawAddFieldCombo("##addLayerField", candidates,
                    [this, edited](const std::string& key) {
                        AppearanceDoc::Json value = 0.0;
                        if (key == "id" || key == "source" || key == "text" || key == "align"
                            || key == "valign" || key == "scaling" || key == "fill" || key == "color"
                            || key == "stroke" || key == "tint")
                            value = std::string("#00000000");
                        if (key == "cutout")
                            value = false;
                        if (key == "inset" || key == "offset")
                            value = AppearanceDoc::Json::array({ 0, 0, 0, 0 });
                        this->doc.setLayerValue(this->selection.index, key, value);
                        edited();
                    });
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

            // 加通道：下拉选层 id + 下拉选属性名（以前要手打 <层id>.<属性名>）
            {
                static int layerIndex = 0;
                static int propIndex  = 0;
                const auto& props     = appearancePropNames();

                propLabel("加通道");
                ImGui::SetNextItemWidth(140);
                std::string preview = this->doc.layerId(layerIndex);
                if (preview.empty())
                    preview = "（选层）";
                if (ImGui::BeginCombo("##chLayer", preview.c_str()))
                {
                    for (int i = 0; i < this->doc.layerCount(); i++)
                    {
                        const bool selected = (i == layerIndex);
                        const std::string label = std::to_string(i + 1) + "  " + this->doc.layerId(i);
                        if (ImGui::Selectable(label.c_str(), selected))
                            layerIndex = i;
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                const std::string propName = props.empty() ? "fill" : props[(size_t)propIndex % props.size()];
                if (ImGui::BeginCombo("##chProp", propName.c_str()))
                {
                    for (size_t i = 0; i < props.size(); i++)
                        if (ImGui::Selectable(props[i].c_str(), (int)i == propIndex))
                            propIndex = (int)i;
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::Button("加"))
                {
                    const std::string id = this->doc.layerId(layerIndex);
                    if (!id.empty() && !props.empty())
                    {
                        this->doc.addChannel(state, id + "." + props[(size_t)propIndex % props.size()]);
                        edited();
                    }
                }
            }

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
                        propLabel("to");
                        if (inputString("##to", to))
                        {
                            AppearanceDoc::Json parsed
                                = AppearanceDoc::Json::parse(to, nullptr, false);
                            transition["to"] = parsed.is_discarded() ? AppearanceDoc::Json(to)
                                                                     : parsed;
                            edited();
                        }

                        double duration = transition.value("duration", 120.0);
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
                        propLabel("easing");
                        if (ImGui::Combo("##easing", &easingIndex,
                                "linear\0quadraticIn\0quadraticOut\0quadraticInOut\0cubicOut\0"
                                "backOut\0bounceOut\0elasticOut\0"))
                        {
                            transition["easing"] = easings[easingIndex];
                            edited();
                        }
                    }
                    if (ImGui::SmallButton("删掉这个通道"))
                    {
                        this->doc.removeChannel(state, index);
                        edited();
                        ImGui::PopID();
                        break;
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
    if (!this->currentFilePath.empty())
        this->currentDraft().dirty = true;
    this->statusMessage = "已修改（未保存）";
}

void DesignerWindow::saveAllDirty()
{
    if (this->readOnly)
    {
        this->errorMessage = "只读主题，不能写盘";
        return;
    }

    this->stashCurrent(); // 不先存当前这份的话，会把它上一次 stash 的旧内容写下去

    const std::string original = this->currentFilePath;
    int saved = 0;

    for (auto& entry : this->drafts)
    {
        if (!entry.second.dirty)
            continue;

        // 只保存"能确定是外观或页面视图"的文件
        std::string text = entry.second.doc.serialize();
        std::string error;
        if (!validateJsonText(text, error))
        {
            this->warnings.push_back(entry.first + "：" + error);
            continue;
        }
        View* probe = createFromJSONString(text);
        if (probe != nullptr)
            delete probe;
        else
        {
            std::shared_ptr<const Appearance> compiled
                = Appearance::compile(text, getTheme(), error);
            if (compiled == nullptr)
            {
                this->warnings.push_back(entry.first + "：编译失败 " + error);
                continue;
            }
        }

        if (writeTextFileAtomic(entry.first, text, true, error))
        {
            entry.second.dirty    = false;
            entry.second.savedText = text;
            entry.second.openHash = this->hashText(text);
            saved++;
        }
        else
            this->warnings.push_back(entry.first + "：" + error);
    }

    this->currentFilePath = original;
    if (saved > 0)
    {
        std::string reloadError;
        this->applyThemeFromProject(reloadError);
        this->dirty        = false;
        this->previewDirty = true;
        this->statusMessage = std::to_string(saved) + " 个文件已写入";
    }
    else
        this->statusMessage = "没有需要写入的改动";
}

void DesignerWindow::applyThemeFromProject(std::string& error)
{
    if (!loadThemeIntoGlobalFromDirectory(this->project.themeDir(), this->project.name))
        error = "主题装载失败：" + this->project.themeDir();
}

void DesignerWindow::requestSave()
{
    if (this->readOnly)
    {
        // 按钮虽然是灰的，但 Ctrl+S / 冲突弹层会绕过它 —— 基准主题绝不能被写
        this->errorMessage = "基准主题（default / ocean）只读：请先「新建项目」用模板建一份自己的主题";
        BKLV_LOG_WARNING("拒绝写盘：%s 是只读的基准主题", this->project.name.c_str());
        return;
    }

    if (this->fileChangedExternally() && !this->allowOverwrite)
    {
        this->showConflict = true;
        return;
    }

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

    if (this->readOnly)
    {
        this->errorMessage = "只读主题，不能写盘";
        return;
    }

    const std::string text = this->doc.serialize();

    std::string error;
    if (!validateJsonText(text, error))
    {
        this->errorMessage = error;
        return;
    }

    // 保存前先校验：写坏的 JSON 不落盘
    if (!this->currentTable.empty())
    {
        if (!validateJsonText(text, error))
        {
            this->errorMessage = error;
            return;
        }
    }
    else if (this->currentViewFile.empty())
    {
        std::shared_ptr<const Appearance> compiled = Appearance::compile(text, getTheme(), error);
        if (compiled == nullptr)
        {
            this->errorMessage = "编译失败（没写盘）：" + error;
            return;
        }
    }
    else
    {
        View* probe = createFromJSONString(text);
        if (probe == nullptr)
        {
            this->errorMessage = "视图实例化失败（没写盘）";
            return;
        }
        delete probe;
        if (!this->applyViewText(error))
        {
            this->errorMessage = "应用失败（没写盘）：" + error;
            return;
        }
        this->savedText = text;
        this->dirty     = false;
    }

    // 带冲突检测的原子写（core 层，能无窗口测）：磁盘内容变了就拒绝（除非刚点过"覆盖写"）
    const auto draftForWrite = this->drafts.find(this->currentFilePath);
    const std::string expected
        = draftForWrite != this->drafts.end() ? draftForWrite->second.openHash : std::string();
    if (!writeFileChecked(this->currentFilePath, expected, text, this->allowOverwrite, true, error))
    {
        this->errorMessage = error;
        this->showConflict = true; // 让用户选覆盖 / 重新载入
        return;
    }

    this->savedText = text;
    this->dirty     = false;

    // 草稿同步：脏标记清掉、指纹更新成刚写下去的内容
    Draft& draft   = this->currentDraft();
    draft.savedText = text;
    draft.dirty     = false;
    draft.openHash  = this->hashText(text);

    // 整主题校验结果进"校验面板"
    this->warnings.clear();
    std::string themeError;
    if (!validateThemeAppearances(themeError))
        this->warnings.push_back("整主题校验失败：" + themeError);
    for (const std::string& warning : this->doc.layerOrderWarnings())
        this->warnings.push_back("层序：" + warning);

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

    if (this->showHelp)
    {
        ImGui::OpenPopup("快捷键与约定");
        this->showHelp = false;
    }
    if (ImGui::BeginPopupModal("快捷键与约定", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(kAccent, "快捷键");
        ImGui::BulletText("Ctrl+S 保存（先看 diff）· Ctrl+Z / Ctrl+Y 撤销 · 重做");
        ImGui::BulletText("Ctrl+D 复制当前层 · Alt+↑ / Alt+↓ 上移 / 下移当前层");
        ImGui::BulletText("F1 校验面板 · F2 预览最大化 · 滚轮缩放画布 · 中键/Shift+左键 平移");
        ImGui::Separator();
        ImGui::TextColored(kAccent, "约定（改样式前值得知道）");
        ImGui::BulletText("尺寸/字号按 720p 写，运行时按窗口宽等比缩放；opacity/duration 不缩放");
        ImGui::BulletText("层序：shadow 在底色前；hlBg 在内容层前；hlGlow / hlBorder 最后");
        ImGui::BulletText("主题引用 @theme/color.a.b 或 @theme/metric.a.b，key 不存在会编译失败");
        ImGui::BulletText("外观必须在 theme.json 的 widgets[] 里登记（设计器里「新建外观」会自动登记）");
        ImGui::BulletText("写盘 = 临时文件 + rename 原子替换，并留一份 .bak");
        ImGui::Separator();
        if (ImGui::Button("关闭", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (this->showNewAppearance)
    {
        ImGui::OpenPopup("新建外观文件");
        this->showNewAppearance = false;
    }
    if (ImGui::BeginPopupModal("新建外观文件", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(kMuted, "会在 %s/widgets/ 下建文件，并登记进 theme.json", this->project.themeDir().c_str());
        ImGui::SetNextItemWidth(320);
        ImGui::InputText("外观名", this->newAppearanceName, sizeof(this->newAppearanceName));
        ImGui::SetNextItemWidth(320);
        ImGui::Combo("模板", &this->newAppearanceTemplate,
            "空壳\0rectangle（一块底）\0button（底 + 文字）\0cell（行高 70 + 标题）\0card（卡片底）\0");
        static const char* templates[] = { "empty", "rectangle", "button", "cell", "card" };
        if (ImGui::Button("创建", ImVec2(120, 0)))
        {
            std::string error;
            if (this->createAppearanceFile(this->newAppearanceName,
                    templates[this->newAppearanceTemplate], error))
                ImGui::CloseCurrentPopup();
            else
                this->errorMessage = error;
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (this->showDeleteAppearance)
    {
        ImGui::OpenPopup("删除外观");
        this->showDeleteAppearance = false;
    }
    if (ImGui::BeginPopupModal("删除外观", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f),
            "要删掉 %s 吗？会同时从 theme.json 摘掉这个登记。", this->currentWidget.c_str());
        ImGui::TextColored(kMuted, "（文件本身删掉，.bak 也清掉；想反悔只能靠 git）");
        if (ImGui::Button("删除", ImVec2(120, 0)))
        {
            std::string error;
            if (this->deleteCurrentAppearance(error))
                ImGui::CloseCurrentPopup();
            else
                this->errorMessage = error;
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // 未保存改动拦截：切文件前弹一次，不再静默丢改动
    if (this->showUnsavedGuard)
    {
        ImGui::OpenPopup("有未保存的改动");
        this->showUnsavedGuard = false;
    }
    if (ImGui::BeginPopupModal("有未保存的改动", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "%s",
            this->currentFilePath.c_str());
        ImGui::TextUnformatted("这个文件有未保存的改动，切走之前要怎么办？");
        ImGui::Separator();
        if (ImGui::Button("保存并切换", ImVec2(140, 0)))
        {
            this->doSave();
            const std::string path = this->pendingPath;
            const std::string widget = this->pendingWidget;
            const std::string view = this->pendingViewFile;
            this->pendingPath.clear();
            this->pendingWidget.clear();
            this->pendingViewFile.clear();
            this->dirty = false;
            this->stashCurrent();
            std::string error;
            if (!view.empty())
                this->openViewFile(view);
            else if (!this->openAppearance(widget, error))
                this->errorMessage = error;
            (void)path;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("丢弃改动", ImVec2(120, 0)))
        {
            this->dirty = false;
            this->discardCurrentDraft();
            const std::string widget = this->pendingWidget;
            const std::string view = this->pendingViewFile;
            this->pendingPath.clear();
            this->pendingWidget.clear();
            this->pendingViewFile.clear();
            std::string error;
            if (!view.empty())
                this->openViewFile(view);
            else if (!this->openAppearance(widget, error))
                this->errorMessage = error;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(100, 0)))
        {
            this->pendingPath.clear();
            this->pendingWidget.clear();
            this->pendingViewFile.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 外部改动：保存前发现文件被别人改了
    if (this->showConflict)
    {
        ImGui::OpenPopup("文件被外部改动过");
        this->showConflict = false;
    }
    if (ImGui::BeginPopupModal("文件被外部改动过", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("磁盘上的这个文件和我打开时不一样了（别的编辑器 / git checkout）。");
        ImGui::TextColored(kMuted, "%s", this->currentFilePath.c_str());
        ImGui::Separator();
        if (ImGui::Button("覆盖写（保留我的）", ImVec2(180, 0)))
        {
            this->showConflict = false;
            const auto found = this->drafts.find(this->currentFilePath);
            if (found != this->drafts.end())
                found->second.openHash.clear();
            this->allowOverwrite = true; // 只放行这一次，避免弹层反复弹
            this->requestSave();
            this->allowOverwrite = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("重新载入（丢我的）", ImVec2(180, 0)))
        {
            this->dirty = false;
            this->discardCurrentDraft();
            std::string error;
            if (this->currentViewFile.empty())
                this->openAppearance(this->currentWidget, error);
            else
                this->openViewFile(this->currentViewFile);
            this->statusMessage = "已重新载入磁盘上的内容";
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // 改动总览：所有脏文件 + 各自的差异条数 + 一键全部保存
    if (this->showOverview)
    {
        ImGui::OpenPopup("改动总览");
        this->showOverview = false;
    }
    if (ImGui::BeginPopupModal("改动总览", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        int dirtyCount = 0;
        for (const auto& entry : this->drafts)
        {
            if (!entry.second.dirty)
                continue;
            dirtyCount++;

            AppearanceDoc::Json before
                = AppearanceDoc::Json::parse(entry.second.savedText, nullptr, false);
            int changes = 0;
            if (!before.is_discarded())
                changes = (int)diffJson(before, entry.second.doc.root()).size();

            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "●");
            ImGui::SameLine();
            ImGui::Text("%s", entry.first.c_str());
            ImGui::SameLine();
            ImGui::TextColored(kMuted, "（%d 处改动）", changes);
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("切到##") + entry.first).c_str()))
            {
                this->currentFilePath = entry.first;
                this->restoreDraftFor(entry.first);
                this->currentWidget = widgetNameFromPath(entry.first);
                ImGui::CloseCurrentPopup();
            }
        }
        if (dirtyCount == 0)
            ImGui::TextColored(kMuted, "没有未保存的改动");
        ImGui::Separator();
        if (ImGui::Button("全部保存", ImVec2(120, 0)))
        {
            this->saveAllDirty();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("关闭", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // 校验面板：整主题校验结果 + 当前文件的层序提醒 + 最近一次错误
    if (this->showWarnings)
    {
        ImGui::OpenPopup("校验 / 警告");
        this->showWarnings = false;
    }
    if (ImGui::BeginPopupModal("校验 / 警告", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(kMuted, "保存时会跑整主题校验；这里也能手动跑一次。");
        if (ImGui::Button("现在校验整主题", ImVec2(160, 0)))
        {
            this->warnings.clear();
            std::string error;
            if (validateThemeAppearances(error))
                this->warnings.push_back("整主题校验通过：" + this->project.name);
            else
                this->warnings.push_back("整主题校验失败：" + error);
        }
        ImGui::Separator();
        for (const std::string& warning : this->warnings)
            ImGui::TextWrapped("%s", warning.c_str());
        for (const std::string& warning : this->doc.layerOrderWarnings())
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "层序：%s", warning.c_str());
        if (this->warnings.empty() && this->doc.layerOrderWarnings().empty())
            ImGui::TextColored(kMuted, "没有警告");
        if (!this->errorMessage.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f), "%s", this->errorMessage.c_str());
        ImGui::Separator();
        if (ImGui::Button("关闭", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (this->showSaveAs)
    {
        ImGui::OpenPopup("另存为");
        this->showSaveAs = false;
    }

    if (ImGui::BeginPopupModal("另存为", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(kMuted, "导出一份 JSON 到指定路径（不影响当前文件）");
        ImGui::SetNextItemWidth(560);
        ImGui::InputText("##saveas", this->saveAsPath, sizeof(this->saveAsPath));
        if (ImGui::Button("写入", ImVec2(120, 0)))
        {
            std::string error;
            if (writeTextFileAtomic(this->saveAsPath, this->doc.serialize(), false, error))
            {
                this->statusMessage = std::string("已另存到 ") + this->saveAsPath;
                ImGui::CloseCurrentPopup();
            }
            else
                this->errorMessage = error;
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
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

    // 快捷键（ImGui 的键盘输入在 SDL3 后端默认开着，直接用 io.KeysDown 的封装）
    if (ImGui::GetCurrentContext() != nullptr && !ImGui::GetIO().WantTextInput)
    {
        const bool ctrl = ImGui::GetIO().KeyCtrl;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
            this->requestSave();
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
            this->undo();
        if (ctrl && (ImGui::IsKeyPressed(ImGuiKey_Y, false)
                        || (ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))))
            this->redo();
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && this->selection.kind == SelKind::LAYER)
        {
            this->doc.duplicateLayer(this->selection.index);
            this->markEdited();
            this->previewDirty = true;
        }
        if (ImGui::GetIO().KeyAlt && this->selection.kind == SelKind::LAYER)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false) && this->doc.moveLayer(this->selection.index, -1))
            {
                this->selection.index -= 1;
                this->markEdited();
                this->previewDirty = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false) && this->doc.moveLayer(this->selection.index, 1))
            {
                this->selection.index += 1;
                this->markEdited();
                this->previewDirty = true;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F1, false))
            this->showWarnings = !this->showWarnings;
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
            this->previewMaximized = !this->previewMaximized;
    }

    this->drawTopBar();

    const float leftWidth  = this->leftCollapsed ? 26.0f : (float)this->leftWidthUi;
    const float rightWidth = (float)this->rightWidthUi;
    const ImVec2 avail = ImGui::GetContentRegionAvail();

    if (this->previewMaximized)
    {
        ImGui::BeginChild("##previewMax", ImVec2(0, 0), true);
        this->drawPreview();
        ImGui::EndChild();
        this->drawModals();
        ImGui::End();
        return;
    }

    ImGui::BeginChild("##left", ImVec2(leftWidth, 0), true);
    if (this->leftCollapsed)
    {
        if (ImGui::Button("»"))
            this->leftCollapsed = false;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("展开左栏");
    }
    else
    {
        if (ImGui::SmallButton("«"))
            this->leftCollapsed = true;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("折叠左栏（给预览让地方）");
        ImGui::SameLine();
        ImGui::TextColored(kAccent, "文件");
        ImGui::BeginChild("##files", ImVec2(0, 250), false);
        this->drawFileList();
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::TextColored(kAccent, "节点树");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##treeFilter", "过滤字段 / 层名…", this->treeFilter,
            sizeof(this->treeFilter));
        ImGui::BeginChild("##tree", ImVec2(0, ImGui::GetContentRegionAvail().y - 78), false);
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

    // 帧级自动入栈：这一帧结束时发现文档和上一帧不同，就把上一帧的状态压进撤销栈。
    // 输入框是"改完才返回 true"，所以只能在帧边界比较，粒度 = 一次改动。
    if (!this->currentFilePath.empty())
    {
        const std::string now = this->doc.serialize();
        // 拖动期间不压栈（否则一次拖拽塞满撤销栈、Ctrl+Z 变成逐像素回退）；
        // 松手那一帧再压，粒度 = 一次操作
        const bool interacting = ImGui::IsAnyItemActive();
        if (!interacting && !this->lastFrameText.empty() && now != this->lastFrameText)
            this->pushUndoSnapshot();
        this->lastFrameText = now;
    }
}

} // namespace designer

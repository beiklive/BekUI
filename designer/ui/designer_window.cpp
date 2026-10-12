#include "designer/ui/designer_window.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "bklv/3rd/imgui/imgui.h"
#include "designer/ui/designer_style.hpp"
#include "designer/core/diff.hpp"
#include "designer/core/json_patch.hpp"
#include "designer/core/file_kind.hpp"
#include "designer/core/theme_edit.hpp"
#include "designer/core/theme_key.hpp"
#include "designer/core/theme_usage.hpp"
#include "designer/core/ui_state.hpp"
#include "designer/core/theme_files.hpp"

namespace fs = std::filesystem;

using namespace bklv;

namespace designer
{
namespace
{

// 配色跟着主题走（见 designer_style.hpp）

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
constexpr float kLabelWidth = 172.0f;

void propLabel(const char* label)
{
    ImGui::TextColored(mutedColor(), "%s", label);
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
    // 传 step：上下方向键 ±1、Ctrl+方向键 ±10（ImGui 的 step_fast）。
    // 以前 step 是 nullptr，方向键完全不动 —— 微调只能靠全选重打。
    static const double kStep     = 1.0;
    static const double kStepFast = 10.0;
    const bool changed = ImGui::InputScalar((std::string("##") + label).c_str(),
        ImGuiDataType_Double, &value, &kStep, &kStepFast, "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("方向键 ±1，Ctrl+方向键 ±10；框里也能直接输入");
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
    this->loadUiState();
    applyDesignerStyle();

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
    this->refreshThemes();
    this->refreshThemeUsage();

    // 默认打开第一个外观文件，界面一进来就有的看
    if (!this->widgetFiles.empty())
        this->openAppearance(widgetNameFromPath(this->widgetFiles.front()), error);
    return true;
}

void DesignerWindow::refreshThemeUsage()
{
    std::string error;
    this->themeUsage = collectThemeKeyUsage(this->project.themeDir(), error);
}

void DesignerWindow::refreshThemes()
{
    this->themeNames.clear();
    this->defaultThemeName.clear();
    std::string error;
    readThemesFile(this->project.themeRoot, this->themeNames, this->defaultThemeName, error);
    if (this->themeNames.empty())
        this->themeNames.push_back(this->project.name); // 至少留当前主题
}

void DesignerWindow::switchTheme(const std::string& name)
{
    if (name == this->project.name)
        return;

    // 切主题：基准主题（default/ocean）只读浏览，自建主题可直接编辑
    Project next;
    next.mode      = ProjectMode::INPLACE;
    next.name      = name;
    next.themeRoot = this->project.themeRoot;
    if (name == this->project.templateName)
        next.mode = ProjectMode::FORK;

    // 有未保存改动先拦：切主题会清草稿，静默丢改动是数据丢失（自审抓到的）
    bool hasDirty = this->dirty;
    for (const auto& entry : this->drafts)
        if (entry.second.dirty)
            hasDirty = true;
    if (hasDirty)
    {
        this->pendingTheme   = name;
        this->showUnsavedGuard = true;
        return;
    }

    std::string error;
    this->drafts.clear(); // 不同主题的文件互不相干，草稿不跨主题带
    if (!this->setProject(next, isBaseTheme(name), error))
        this->errorMessage = error;
    else
        this->statusMessage = "已切到主题 " + name + (isBaseTheme(name) ? "（只读浏览）" : "");
}

void DesignerWindow::refreshFileList()
{
    this->widgetFiles.clear();
    this->viewFiles.clear();

    std::string error;
    readWidgetFileList(this->project.themeDir(), this->widgetFiles, error);

    // 最近修改：外观 + 页面视图各取前几个（文件列表顶部的快捷入口）
    this->recentFiles.clear();
    {
        std::vector<DirEntry> recent;
        std::string error;
        if (listFilesByMtime(this->project.themeDir() + "/widgets", 4, recent, error))
            this->recentFiles.insert(this->recentFiles.end(), recent.begin(), recent.end());
        recent.clear();
        if (listFilesByMtime(this->project.resourcesDir() + "/views", 2, recent, error))
            this->recentFiles.insert(this->recentFiles.end(), recent.begin(), recent.end());
    }

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
    this->lastAppearanceName = widgetName;
    this->currentViewFile.clear();
    this->currentTable.clear();
    this->currentFilePath  = path;
    this->savedText        = text;
    this->dirty            = false;
    // 换文件必须清：面板里的绑定/映射指向的是上一次建出来的视图（重建后就悬垂了）
    this->pageBindings.clear();
    this->viewMap.clear();
    this->focusOrder.clear();
    this->selection        = Selection{};
    this->previewDirty     = true;
    this->errorMessage.clear();
    this->statusMessage    = "打开 " + widgetName;

    // 这份文件的第一份草稿：记指纹 + 把当前内容压进撤销栈的第 0 步
    // （doc 也必须存：切回同一文件走的是"恢复草稿"分支，不存就会拿到一份空文档）
    Draft& draft    = this->drafts[path];
    draft.doc       = this->doc;
    draft.savedText = text;
    draft.openHash  = this->hashText(text);
    draft.dirty     = false;
    draft.history.reset(text);
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
    this->lastViewFile    = fileName;
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

    // 换页面：绑定/映射先清空，示例数据按文件各自恢复（不串页）
    this->pageBindings.clear();
    this->viewMap.clear();
    this->focusOrder.clear();
    this->bindingSamples[0] = 0;
    const auto savedSamples  = this->bindingSamplesByFile.find(path);
    if (savedSamples != this->bindingSamplesByFile.end())
        std::snprintf(this->bindingSamples, sizeof(this->bindingSamples), "%s",
            savedSamples->second.c_str());
    Draft& draft    = this->drafts[this->currentFilePath];
    draft.doc       = this->doc;
    draft.savedText = text;
    draft.openHash  = this->hashText(text);
    draft.dirty     = false;
    draft.history.reset(text);
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
    // 和切外观/切页面一样要清：留着上一份的绑定指针在重建后就悬垂了
    this->pageBindings.clear();
    this->viewMap.clear();
    this->focusOrder.clear();
    this->previewDirty     = true;
    this->lastFrameText    = text;

    Draft& draft    = this->drafts[path];
    draft.doc       = this->doc;
    draft.savedText = text;
    draft.openHash  = this->hashText(text);
    draft.dirty     = false;
    draft.history.reset(text);
    this->refreshThemeUsage();
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

    // "另存为变体"走的就是这条路：模板为空壳、但把当前编辑中的内容原样带过去
    if (templateName == "empty" && !this->currentWidget.empty()
        && !this->currentFilePath.empty())
    {
        AppearanceDoc::Json content = this->doc.root();
        std::string compileError;
        if (Appearance::compile(content.dump(), getTheme(), compileError) == nullptr)
        {
            error = "当前外观编译不过，先修好再另存：" + compileError;
            return false;
        }
        if (!createWidgetAppearance(this->project.themeDir(), name, content.dump(2) + "\n", error))
            return false;

        this->refreshFileList();
        this->refreshThemeUsage();
        std::string reloadError;
        this->applyThemeFromProject(reloadError);
        std::string openError;
        this->openAppearance(name, openError);
        this->statusMessage = "已另存为新外观 " + name + "（已登记进 theme.json）";
        return true;
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
    this->refreshThemeUsage(); // 新建的外观可能带来新的引用
    std::string reloadError;
    this->applyThemeFromProject(reloadError);
    std::string openError;
    this->openAppearance(name, openError);
    this->statusMessage = "新建外观 " + name + "（已登记进 theme.json）";
    return true;
}

bool DesignerWindow::saveAsNewAppearance(const std::string& name, std::string& error)
{
    if (this->currentWidget.empty())
    {
        error = "先打开一个外观";
        return false;
    }
    // 用当前编辑中的内容（含未保存改动）另存，core 会做重名检查 + 登记 + 编译前校验
    return this->createAppearanceFile(name, "empty", error);
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
    this->refreshThemeUsage(); // 删掉的外观带走的引用要跟着消失
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
        ImGui::TextColored(mutedColor(), "取色会写成 #RRGGBBAA 字面量（不再跟主题走）");
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
    if (spec.rfind("view:", 0) == 0)
    {
        this->selection = Selection{};
        this->selection.kind = SelKind::VIEW;
        this->selection.key  = spec.substr(5);
        this->pendingExpandPath = this->selection.key;
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

// ---------------------------------------------------------------- 界面状态持久化

void DesignerWindow::loadUiState()
{
    UiState state;
    if (!designer::loadUiState(resourcePath("designer_state.json"), state))
        return; // 第一次跑本来就没有

    this->leftWidthUi      = state.leftWidth;
    this->rightWidthUi     = state.rightWidth;
    this->leftCollapsed    = state.leftCollapsed;
    this->canvasPreset     = state.canvasPreset;
    this->canvasChecker    = state.canvasChecker;
    this->canvasZoom       = state.canvasZoom;
    this->showGrid         = state.showGrid;
    this->onlyLayers       = state.onlyLayers;
    this->previewContext   = state.previewContext;
    this->previewCompare   = state.previewCompare;
    this->previewMaximized = state.previewMaximized;
    this->previewSubtree   = state.previewSubtree;
    this->lastAppearanceName = state.lastAppearance;
    this->lastViewFile       = state.lastViewFile;
    this->startupAppearance  = state.lastAppearance;
    this->startupViewFile    = state.lastViewFile;
}

void DesignerWindow::saveUiState()
{
    UiState state;
    state.leftWidth        = this->leftWidthUi;
    state.rightWidth       = this->rightWidthUi;
    state.leftCollapsed    = this->leftCollapsed;
    state.canvasPreset     = this->canvasPreset;
    state.canvasChecker    = this->canvasChecker;
    state.canvasZoom       = this->canvasZoom;
    state.showGrid         = this->showGrid;
    state.onlyLayers       = this->onlyLayers;
    state.previewContext   = this->previewContext;
    state.previewCompare   = this->previewCompare;
    state.previewMaximized = this->previewMaximized;
    state.previewSubtree   = this->previewSubtree;
    state.lastAppearance   = this->lastAppearanceName;
    state.lastViewFile     = this->currentViewFile.empty() ? this->lastViewFile
                                                          : this->currentViewFile;

    std::string error;
    designer::saveUiState(resourcePath("designer_state.json"), state, error);
}

bool DesignerWindow::restoreLastFileIfAny()
{
    if (!this->startupViewFile.empty())
        for (const std::string& file : this->viewFiles)
            if (file == this->startupViewFile)
                return this->openViewFile(this->startupViewFile);

    if (!this->startupAppearance.empty())
        for (const std::string& relative : this->widgetFiles)
            if (widgetNameFromPath(relative) == this->startupAppearance)
            {
                std::string error;
                return this->openAppearance(this->startupAppearance, error);
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

    this->currentDraft().history.push(this->doc.serialize());
}

void DesignerWindow::undo()
{
    if (this->currentFilePath.empty())
        return;

    std::string restored;
    if (!this->currentDraft().history.undo(restored))
    {
        this->statusMessage = "没有可撤销的步骤";
        return;
    }

    // 这一步撤掉了什么：拿撤销前后两份文档比一下，列前几条
    {
        const AppearanceDoc::Json before
            = AppearanceDoc::Json::parse(restored, nullptr, false);
        std::string summary;
        int shown = 0;
        for (const std::string& path : changedPaths(before, this->doc.root()))
        {
            if (shown++ >= 3)
            {
                summary += " 等";
                break;
            }
            summary += (summary.empty() ? "" : "、") + path;
        }
        this->historySummary = summary;
    }

    std::string error;
    this->doc.parse(restored, error);
    this->viewTextApplied = this->doc.serialize();
    std::snprintf(this->viewText, sizeof(this->viewText), "%s", this->viewTextApplied.c_str());
    // 必须同步"上一帧内容"：不同步的话下一帧边界会把它当成"又改了一次"再压一步，
    // 而 push() 会清空重做链 —— 结果就是**重做永远用不了**（自审抓到的）
    this->lastFrameText = this->doc.serialize();
    this->dirty        = true;
    this->previewDirty = true;
    this->statusMessage = this->historySummary.empty()
        ? "已撤销"
        : "已撤销：" + this->historySummary;
}

void DesignerWindow::redo()
{
    if (this->currentFilePath.empty())
        return;

    std::string restored;
    if (!this->currentDraft().history.redo(restored))
    {
        this->statusMessage = "没有可重做的步骤";
        return;
    }

    {
        const AppearanceDoc::Json after = AppearanceDoc::Json::parse(restored, nullptr, false);
        std::string summary;
        int shown = 0;
        for (const std::string& path : changedPaths(this->doc.root(), after))
        {
            if (shown++ >= 3)
            {
                summary += " 等";
                break;
            }
            summary += (summary.empty() ? "" : "、") + path;
        }
        this->historySummary = summary;
    }

    std::string error;
    this->doc.parse(restored, error);
    this->viewTextApplied = this->doc.serialize();
    std::snprintf(this->viewText, sizeof(this->viewText), "%s", this->viewTextApplied.c_str());
    this->lastFrameText = this->doc.serialize(); // 同上：否则下一帧又会压一步
    this->dirty        = true;
    this->previewDirty = true;
    this->statusMessage = this->historySummary.empty()
        ? "已重做"
        : "已重做：" + this->historySummary;
}

// ---------------------------------------------------------------- 顶栏

void DesignerWindow::drawTopBar()
{
    // 第一行：标题 + 项目信息（长路径中间省略，别把按钮挤歪）
    ImGui::PushStyleColor(ImGuiCol_Text, accentColor());
    ImGui::TextUnformatted("bklv 设计器");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    if (this->hasProject)
    {
        std::string root = this->project.themeRoot;
        if (root.size() > 46)
            root = "…" + root.substr(root.size() - 45);
        ImGui::TextColored(mutedColor(), "%s（%s%s）· %s", this->project.name.c_str(),
            this->project.mode == ProjectMode::FORK ? "模板新建，来自 " : "直接编辑",
            this->project.templateName.c_str(), root.c_str());
        if (this->readOnly)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "· 基准主题只读，改样式请「新建项目」");
        }
    }
    else
        ImGui::TextColored(mutedColor(), "还没有打开项目");

    // 第二行：动作
    // 组间分隔：多给一点间距 + 一条细竖线。**不能用 Dummy**——Dummy 会让光标换行，
    // 顶栏会被拆成好几行（试过一次，白白吃掉面板高度）
    auto groupBreak = [] {
        ImGui::SameLine(0.0f, 16.0f);
        const ImVec2 pos   = ImGui::GetCursorScreenPos();
        const float height = ImGui::GetFrameHeight();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x - 9.0f, pos.y + 2.0f),
            ImVec2(pos.x - 9.0f, pos.y + height - 2.0f),
            ImGui::ColorConvertFloat4ToU32(borderColor()));
    };

    if (ImGui::Button("打开项目"))
    {
        this->showOpenProject = true;
        // 起点：当前项目的主题根（没有就用 home）
        this->openBrowseAt(this->hasProject ? this->project.themeRoot : homeDirectory());
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("用文件浏览器选一个 theme.json：可以直接编辑它，也可以拿它当模板建新主题");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    if (ImGui::BeginCombo("##theme", this->project.name.c_str()))
    {
        for (const std::string& name : this->themeNames)
        {
            std::string label = name;
            if (name == this->defaultThemeName)
                label += "   （默认）";
            if (ImGui::Selectable(label.c_str(), name == this->project.name))
                this->switchTheme(name);
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("切换正在编辑的主题（default / ocean 是只读的基准主题）");
    ImGui::SameLine();
    ImGui::BeginDisabled(this->readOnly || this->project.name == this->defaultThemeName);
    if (ImGui::Button("设为默认"))
    {
        std::string error;
        std::string currentDefault;
        std::vector<std::string> available;
        if (readThemesFile(this->project.themeRoot, available, currentDefault, error)
            && writeThemesFile(this->project.themeRoot, available, this->project.name, error))
        {
            this->defaultThemeName = this->project.name;
            this->statusMessage    = "已把 " + this->project.name + " 设为默认主题";
        }
        else
            this->errorMessage = error;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("写进 themes.json 的 default：以后启动就用它（只读主题不给改）");
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
    groupBreak();
    ImGui::BeginDisabled(this->currentFilePath.empty());
    if (ImGui::Button("还原"))
        this->revert();
    ImGui::SameLine();
    ImGui::BeginDisabled(this->currentFilePath.empty() || this->dirty);
    if (ImGui::Button("重新载入"))
    {
        // 把磁盘上的内容读回来（有未保存改动时按钮是灰的，避免盖掉自己的东西）
        this->discardCurrentDraft();
        std::string error;
        if (this->currentViewFile.empty() && this->currentTable.empty())
            this->openAppearance(this->currentWidget, error);
        else if (!this->currentTable.empty())
            this->openTableFile(this->currentTable);
        else
            this->openViewFile(this->currentViewFile);
        this->statusMessage = "已从磁盘重新载入";
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("丢掉内存里的内容、按磁盘上的文件重新读一遍（有未保存改动时会灰掉）");
    ImGui::SameLine();
    if (ImGui::Button("另存…"))
    {
        std::snprintf(this->saveAsPath, sizeof(this->saveAsPath), "%s",
            this->currentFilePath.c_str());
        this->showSaveAs = true;
    }
    ImGui::EndDisabled();
    groupBreak();
    ImGui::BeginDisabled((this->readOnly && this->currentViewFile.empty())
        || this->currentFilePath.empty());
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 1.0f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.03f, 0.13f, 0.11f, 1.0f));
    const bool savePressed = ImGui::Button("保存（先看 diff）");
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
    if (savePressed)
        this->requestSave();

    groupBreak();
    ImGui::BeginDisabled(this->currentFilePath.empty());
    if (ImGui::Button("撤销"))
        this->undo();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ctrl+Z%s", this->historySummary.empty()
                ? ""
                : ("\n上一步动过：" + this->historySummary).c_str());
    ImGui::SameLine();
    if (ImGui::Button("重做"))
        this->redo();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ctrl+Y");
    ImGui::EndDisabled();

    groupBreak();
    {
        int dirtyCount = 0;
        for (const auto& entry : this->drafts)
            if (entry.second.dirty)
                dirtyCount++;
        if (ImGui::Button("改动总览"))
            this->showOverview = true;
        ImGui::SameLine();
        ImGui::TextColored(dirtyCount > 0 ? ImVec4(1.0f, 0.72f, 0.3f, 1.0f) : mutedColor(),
            dirtyCount > 0 ? "%d 个文件有未保存改动" : "都已同步", dirtyCount);
    }
    ImGui::SameLine();
    if (ImGui::Button("校验面板"))
        this->showWarnings = true;
    ImGui::SameLine();
    if (ImGui::Button("日志"))
        this->debugWindow.toggle();
    ImGui::SameLine();
    if (ImGui::Button("焦点顺序"))
        this->showFocusOrder = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("按视图树顺序列出可聚焦控件（整页预览时有效），并可在预览里标号");
    ImGui::SameLine();
    if (ImGui::Button("快捷键/约定"))
        this->showHelp = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ctrl+S 保存 · Ctrl+Z/Y 撤销重做 · Ctrl+D 复制层 · Alt+↑↓ 移层\n"
                          "F1 校验 · F2 预览最大化 · F3 跳到下一处改动");

    ImGui::SameLine();
    if (this->dirty)
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "● 当前文件未保存");
    else if (this->readOnly)
        ImGui::TextColored(mutedColor(), "（只读浏览）");
    else
        ImGui::TextColored(mutedColor(), "已同步");

    ImGui::Separator();
}

// ---------------------------------------------------------------- 文件列表

void DesignerWindow::drawFileList()
{
    static int rowIndex = 0;
    auto fileRow = [](const std::string& label, bool selected, bool isDirty, const char* tag) {
        const ImVec2 start = ImGui::GetCursorScreenPos();
        std::string text   = std::string(isDirty ? "● " : "  ") + label;
        if (tag != nullptr)
            text += std::string("   [") + tag + "]";

        // 斑马纹：隔行一层极淡的底，长列表更容易跟行
        if (rowIndex++ % 2 == 1 && !selected)
        {
            const float width = ImGui::GetContentRegionAvail().x;
            ImGui::GetWindowDrawList()->AddRectFilled(start,
                ImVec2(start.x + width, start.y + ImGui::GetTextLineHeight() + 2.0f),
                IM_COL32(255, 255, 255, 8), 4.0f);
        }

        const bool clicked = ImGui::Selectable(text.c_str(), selected);
        if (selected)
        {
            // 选中项左侧一条强调色（样板房里就是这么标"当前在看哪个文件"的）
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(start.x - 4.0f, start.y + 1.0f),
                ImVec2(start.x - 1.0f, start.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(accentColor()), 2.0f);
        }
        return clicked;
    };

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##filter", "过滤外观名…", this->fileFilter, sizeof(this->fileFilter));

    // 最近修改：改样式时反复打开的就那几个，放最上面省得每次翻列表
    if (!this->recentFiles.empty() && this->fileFilter[0] == 0)
    {
        ImGui::TextColored(accentColor(), "最近修改");
        for (const DirEntry& entry : this->recentFiles)
        {
            const std::string name = entry.name.size() > 5
                ? entry.name.substr(0, entry.name.size() - 5)
                : entry.name; // 去掉 .json
            const bool isCurrent = (name == this->currentWidget);
            if (fileRow(name, isCurrent, isCurrent && this->dirty, "最近"))
            {
                std::string error;
                if (!this->openAppearance(name, error))
                    this->errorMessage = error;
            }
        }
        ImGui::Spacing();
    }

    ImGui::TextColored(mutedColor(), "主题外观 · %d 个（%s）", (int)this->widgetFiles.size(),
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
    ImGui::TextColored(mutedColor(), "页面视图 · %d 个（views/）", (int)this->viewFiles.size());
    if (!this->readOnly)
    {
        if (ImGui::SmallButton("新建页面…"))
            this->showNewView = true;
    }
    if (this->viewFiles.empty())
        ImGui::TextColored(mutedColor(), "  （这个项目里没有）");
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
        if (ImGui::Button("另存为变体…"))
        {
            std::snprintf(this->variantName, sizeof(this->variantName), "%s_v2",
                this->currentWidget.c_str());
            this->showSaveAsVariant = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("把当前外观（含未保存改动）复制成一个新外观并登记 —— 做变体用");
        ImGui::SameLine();
        if (ImGui::Button("删除当前外观"))
            this->showDeleteAppearance = true;
    }

    ImGui::Separator();
    ImGui::TextColored(mutedColor(), "主题表（可编辑）");
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
    if (node.contains("bind"))
        label += "   [bind " + compactValue(node["bind"]) + "]";
    if (node.contains("include"))
        label += "   [include " + compactValue(node["include"]) + "]";

    // include 节点：把被包含的文件展开成子节点（递归有上限，include 环由库那边兜底）
    if (node.contains("include") && node["include"].is_string())
    {
        const std::string includePath = node["include"].get<std::string>();
        ImGui::PushID(path.c_str());
        this->viewNodeCount++;
        const bool open = ImGui::TreeNodeEx("##include",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
            "[include]  %s", includePath.c_str());
        if (open)
        {
            if (this->includeDepthGuard >= 6)
                ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "（嵌套太深，不再展开）");
            else
            {
                const AppearanceDoc::Json* included
                    = this->includeCache.get(resourcePath(includePath));
                if (included == nullptr)
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f),
                        "被包含的文件读不到或不是合法 JSON：%s", includePath.c_str());
                else
                {
                    this->includeDepthGuard++;
                    this->drawViewTree(*included, path + "#include");
                    this->includeDepthGuard--;
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
        return;
    }

    ImGui::PushID(path.c_str());
    this->viewNodeCount++;

    const bool selected = this->selection.kind == SelKind::VIEW && this->selection.key == path;
    // 反选过来时强制展开到那个节点（用户手动折叠过也照样展开一次）
    if (!this->pendingExpandPath.empty()
        && this->pendingExpandPath.rfind(path, 0) == 0)
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
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
                this->drawViewTree(child, path + "/children/" + std::to_string(index));
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
        this->drawViewTree(this->doc.root(), "");
        this->pendingExpandPath.clear();
        if (this->viewNodeCount <= 0)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f), "这个 JSON 里没有可展开的视图节点");
        return;
    }

    if (this->currentWidget.empty())
    {
        ImGui::TextColored(mutedColor(), "先从上面挑一个外观文件");
        return;
    }

    const ImGuiTreeNodeFlags leafFlags = ImGuiTreeNodeFlags_Leaf
        | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;

    // 文件
    {
        const bool selected = this->selection.kind == SelKind::FILE;
        if (this->treeOpenRequest != 0)
            ImGui::SetNextItemOpen(this->treeOpenRequest == 1, ImGuiCond_Always);
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
            if (this->treeOpenRequest != 0)
                ImGui::SetNextItemOpen(this->treeOpenRequest == 1, ImGuiCond_Always);
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
                std::snprintf(label, sizeof(label), "%s#%d  %s  %s",
                    this->isLayerChanged(i) ? "• " : "", i + 1,
                    this->doc.layerOp(i).c_str(), this->doc.layerId(i).c_str());
                const std::string id = "##layer" + std::to_string(i);
                // "只看层"时把它画成叶子：点一下选中层、不再展开一堆字段
                const ImGuiTreeNodeFlags layerFlags = this->onlyLayers
                    ? (ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen)
                    : ImGuiTreeNodeFlags_DefaultOpen;
                if (this->treeOpenRequest != 0)
                    ImGui::SetNextItemOpen(this->treeOpenRequest == 1, ImGuiCond_Always);
                const bool open = ImGui::TreeNodeEx(id.c_str(), layerFlags
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
                // "只看层"时**不能**进这个子块：Leaf|NoTreePushOnOpen 下 open 恒为 true，
                // 子块会照跑并调 TreePop() —— 那相当于多调一次 PopID（ImGui 会报
                // "Calling PopID() too many times!"，每层报一次）。而且字段也根本没被隐藏。
                if (open && !this->onlyLayers)
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

    // 层序硬约定的提醒 + 一键按约定重排
    const std::vector<std::string> orderWarnings = this->doc.layerOrderWarnings();
    this->treeOpenRequest = 0; // 只在点的那一帧生效，之后用户还能自己折起来

    for (const std::string& warning : orderWarnings)
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "⚠ %s", warning.c_str());
    if (!orderWarnings.empty() && !this->readOnly)
    {
        if (ImGui::SmallButton("按约定重排层序"))
        {
            if (this->doc.reorderLayersByConvention())
            {
                this->markEdited();
                this->previewDirty = true;
                this->selection    = Selection{};
                this->selection.kind = SelKind::FILE;
                this->statusMessage
                    = "已按 shadow → 底色 → hlBg → 内容 → hlGlow/hlBorder 重排（保存前可撤销）";
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Ctrl+Z 可以撤销；重排只动顺序，不改任何字段");
    }
}

// ---------------------------------------------------------------- 左下状态

void DesignerWindow::drawStatus()
{
    int nodes = 1 + (int)this->doc.boxKeys().size();
    for (int i = 0; i < this->doc.layerCount(); i++)
        nodes += 1 + (this->doc.layer(i) != nullptr ? (int)this->doc.layer(i)->size() : 0);
    for (const std::string& state : this->doc.stateNames())
        nodes += 1 + this->doc.channelCount(state);

    // 页脚：一条比面板略"低"的底色，和上面内容区分开
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x - 4.0f, pos.y),
            ImVec2(pos.x + width + 4.0f, pos.y + 62.0f),
            ImGui::ColorConvertFloat4ToU32(currentDesignerPalette().sunken), 6.0f);
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
    }
    if (!this->currentViewFile.empty())
    {
        ImGui::TextColored(accentColor(), "视图节点 %d", this->viewNodeCount);
        ImGui::SameLine();
        ImGui::TextColored(mutedColor(), "· 字段 %d", this->viewFieldCount);
    }
    else
    {
        ImGui::TextColored(accentColor(), "节点 %d", nodes);
        ImGui::SameLine();
        ImGui::TextColored(mutedColor(), "· 层 %d · 状态 %d", this->doc.layerCount(),
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
    ImGui::TextColored(mutedColor(), "%s", path.c_str());

    if (!this->statusMessage.empty())
        ImGui::TextColored(mutedColor(), "%s", this->statusMessage.c_str());
    if (!this->errorMessage.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f), "%s", this->errorMessage.c_str());
}

// ---------------------------------------------------------------- 预览

AppearanceView* DesignerWindow::createPreviewControlFor(const std::string& name) const
{

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

AppearanceView* DesignerWindow::createPreviewControl() const
{
    return this->createPreviewControlFor(this->currentWidget);
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

bool DesignerWindow::drawEnumCombo(const char* id, const std::string& key, std::string& value)
{
    // 这几个字段就那么几个合法取值，文本框写错了要等编译报错才知道
    static const std::pair<const char*, const std::vector<std::string>> tables[] = {
        { "align", { "left", "center", "right" } },
        { "valign", { "top", "center", "bottom" } },
        { "scaling", { "fit", "fill", "stretch", "center" } },
        { "axis", { "row", "column" } },
        { "direction", { "ltr", "rtl" } },
    };

    for (const auto& entry : tables)
    {
        if (key != entry.first)
            continue;

        int index = 0;
        for (size_t i = 0; i < entry.second.size(); i++)
            if (value == entry.second[i])
                index = (int)i;

        const std::string current = value.empty() ? entry.second[0] : value;
        propLabel(key.c_str());
        ImGui::PushID(id);
        if (ImGui::BeginCombo("##enum", current.c_str()))
        {
            for (size_t i = 0; i < entry.second.size(); i++)
                if (ImGui::Selectable(entry.second[i].c_str(), (int)i == index))
                {
                    value = entry.second[i];
                    ImGui::PopID();
                    return true;
                }
            ImGui::EndCombo();
        }
        ImGui::PopID();
        return false;
    }
    return false;
}

bool DesignerWindow::colorField(const char* label, std::string& value)
{
    bool changed = false;

    // 色板预览：@theme 引用也解出来给个准色
    ImU32 preview = 0;
    // 查表要用**带 color. 前缀的全名**（主题加载时就是这么展平的）；
    // 以前在这里把前缀剥掉了，结果色板与对比度检查都在查不存在的键（踩过）
    if (const std::string key = themeKeyFromReference(value); !key.empty())
        preview = getTheme().color(key, 0);
    else
        preview = parseHexColor(value); // "#RRGGBB[AA]"

    propLabel(label);

    ImGui::ColorButton((std::string("##sw") + label).c_str(), ImGui::ColorConvertU32ToFloat4(preview),
        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(22, 18));
    if (!themeKeyFromReference(value).empty())
    {
        const std::string lookup = themeKeyFromReference(value);
        const auto found = this->themeUsage.find(lookup);
        const int users  = found == this->themeUsage.end() ? 0 : (int)found->second.size();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%s", value.c_str(),
                users == 0 ? "没有别的地方引用这个 key"
                           : (std::to_string(users) + " 个外观引用它").c_str());
    }
    ImGui::SameLine();

    // 文本占中间；右边要给"取色"和"主题 key"两个控件留位置（以前留 96 不够，被裁）
    ImGui::SetNextItemWidth(-152.0f);
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
    // 文件里的键是 `common.red` / `radius.md`（没有 color./metric. 外层），
    // 而外观里的引用是 `@theme/color.common.red` / `@theme/metric.radius.md`。
    // 查反向引用表时要补上前缀，否则一律显示"没被引用"（这个 bug 是截图抓到的）。
    const std::string keyPrefix
        = this->currentTable.rfind("metrics", 0) == 0 ? "metric." : "color.";

    panelHeader("主题表", this->currentTable.c_str());
    ImGui::TextColored(mutedColor(), "改完「保存」= 写回这个文件 + 热重载（预览立刻变）");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##tableFilter", "过滤 key…", this->tableFilter, sizeof(this->tableFilter));
    ImGui::TextColored(mutedColor(), "%s", this->currentFilePath.c_str());
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
                      ImGui::SameLine();
                      {
                          const auto found = this->themeUsage.find(keyPrefix + key);
                          const int users  = found == this->themeUsage.end()
                              ? 0
                              : (int)found->second.size();
                          if (users == 0)
                          {
                              ImGui::TextColored(mutedColor(), "· 没被引用");
                              if (ImGui::IsItemHovered())
                                  ImGui::SetTooltip("没有任何外观引用这个 key（改它不会有变化）");
                          }
                          else
                          {
                              ImGui::TextColored(mutedColor(), "· %d 处引用", users);
                              ImGui::SameLine();
                              if (ImGui::SmallButton("谁在用"))
                              {
                                  this->usagePopupKey = key;
                                  ImGui::OpenPopup("引用者");
                              }
                          }
                      }
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
                      ImGui::TextColored(mutedColor(), "      %s", key.c_str());
                      ImGui::SameLine();
                      {
                          const auto found = this->themeUsage.find(keyPrefix + key);
                          const int users  = found == this->themeUsage.end()
                              ? 0
                              : (int)found->second.size();
                          if (users == 0)
                          {
                              ImGui::TextColored(mutedColor(), "· 没被引用");
                              if (ImGui::IsItemHovered())
                                  ImGui::SetTooltip("没有任何外观引用这个 key（改它不会有变化）");
                          }
                          else
                          {
                              ImGui::TextColored(mutedColor(), "· %d 处引用", users);
                              ImGui::SameLine();
                              if (ImGui::SmallButton("谁在用"))
                              {
                                  this->usagePopupKey = key;
                                  ImGui::OpenPopup("引用者");
                              }
                          }
                      }
                  }
                  else
                      ImGui::TextColored(mutedColor(), "%s = %s", key.c_str(),
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

    // "谁在用"弹层：列出引用这个 key 的外观，点一下就切过去（改配色时最有用）
    if (ImGui::BeginPopup("引用者"))
    {
        ImGui::TextColored(mutedColor(), "引用 %s 的外观（点一下切过去）", this->usagePopupKey.c_str());
        ImGui::Separator();
        const auto found = this->themeUsage.find(this->usagePopupKey);
        if (found == this->themeUsage.end() || found->second.empty())
            ImGui::TextColored(mutedColor(), "没有引用者");
        else
            for (const std::string& widget : found->second)
                if (ImGui::Selectable(widget.c_str()))
                {
                    std::string error;
                    if (!this->openAppearance(widget, error))
                        this->errorMessage = error;
                    ImGui::CloseCurrentPopup();
                }
        ImGui::EndPopup();
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
            // 按 JSON Pointer（/children/N）取出那一棵子树，单独实例化
            const AppearanceDoc::Json* node = &this->doc.root();
            const std::string& path         = this->selection.key;
            size_t pos                      = 0;
            while (pos < path.size() && node != nullptr)
            {
                if (path.compare(pos, 10, "/children/") == 0)
                {
                    const int index = std::atoi(path.c_str() + pos + 10);
                    if (node->contains("children") && (*node)["children"].is_array()
                        && index >= 0 && index < (int)(*node)["children"].size())
                        node = &(*node)["children"][(size_t)index];
                    else
                        node = nullptr;
                    pos += 10;
                    while (pos < path.size() && path[pos] != '/')
                        pos++;
                }
                else
                    break;
            }
            if (node != nullptr && node->is_object())
                text = node->dump();
        }

        this->viewMap.clear();
        this->pageBindings.clear();
        View* created = createFromJSONString(text, &this->viewMap, &this->pageBindings);
        if (created == nullptr)
            this->previewError = "视图 JSON 实例化失败（未知 type / children 结构 / include 路径）";

        // 绑定的字段在预览里没有数据源，用"示例数据"补上，否则列表/文字全是空的
        if (!this->pageBindings.empty() && this->bindingSamples[0] == 0)
        {
            AppearanceDoc::Json samples = AppearanceDoc::Json::object();
            for (const bklv::Binding& binding : this->pageBindings)
                samples[binding.field] = "示例 " + binding.field;
            std::snprintf(this->bindingSamples, sizeof(this->bindingSamples), "%s",
                samples.dump(2).c_str());
        }
        this->applyBindingSamples();
        this->bindingSamplesByFile[this->currentFilePath] = this->bindingSamples;
        this->applySamplesPending = false;
        return created;
    }

    // 主题表没有自己的预览：拿最近打开的那个外观来当"改颜色的效果预览"
    // （保存后主题会热重载，这里再建一次就能看到新颜色）
    if (!this->currentTable.empty())
    {
        if (this->lastAppearanceName.empty())
            return nullptr;

        AppearanceView* control = this->createPreviewControlFor(this->lastAppearanceName);
        std::string text;
        std::string readError;
        const std::string path
            = this->project.themeDir() + "/" + widgetPathFromName(this->lastAppearanceName);
        if (readTextFile(path, text, readError))
        {
            std::string applyError;
            if (!control->applyAppearanceJson(text, applyError))
            {
                this->previewError = applyError;
                delete control;
                return nullptr;
            }
        }
        return control;
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
    // 只改了示例数据：不重建（重建会把新套的值冲掉），直接在当前视图上再套一遍
    if (!this->previewDirty && this->applySamplesPending)
    {
        const int applied = this->applyBindingSamples();
        this->applySamplesPending = false;
        this->statusMessage = "已套用 " + std::to_string(applied) + " 条示例数据";
        return;
    }

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

    // 对比度检查：文字层的颜色 vs 控件底色（< 4.5 提示，WCAG AA 的正文门槛）
    this->contrastWarnings.clear();
    if (!this->currentWidget.empty())
    {
        std::string background;
        this->doc.getBoxString("backgroundColor", background);
        ImU32 backgroundU32 = 0;
        if (const std::string backgroundKey = themeKeyFromReference(background);
            !backgroundKey.empty())
            backgroundU32 = getTheme().color(backgroundKey, 0);
        else
            backgroundU32 = parseHexColor(background);

        // 底色怎么取：文字层之前最后一个"有底色"的 rect 层最贴近实际
        // （只用 box.backgroundColor 会误报——很多控件是把底色画在 rect 层里的）
        const auto& allLayers = this->doc.root().value("layers", AppearanceDoc::Json::array());
        for (const AppearanceDoc::Json& layer : allLayers)
        {
            if (!layer.is_object())
                continue;

            if (layer.value("op", std::string()) == "rect")
            {
                const std::string fill = layer.value("fill", std::string());
                ImU32 fillColor        = 0;
                if (const std::string fillKey = themeKeyFromReference(fill); !fillKey.empty())
                    fillColor = getTheme().color(fillKey, 0);
                else
                    fillColor = parseHexColor(fill);
                if ((fillColor & IM_COL32_A_MASK) != 0)
                    backgroundU32 = fillColor;
                continue;
            }

            if (layer.value("op", std::string()) != "text")
                continue;
            const std::string color = layer.value("color", std::string());
            ImU32 textU32 = 0;
            if (const std::string textKey = themeKeyFromReference(color); !textKey.empty())
                textU32 = getTheme().color(textKey, 0);
            else
                textU32 = parseHexColor(color);

            auto channel = [](ImU32 c, int shift) { return (float)((c >> shift) & 0xFF) / 255.0f; };
            auto luminance = [&](ImU32 c) {
                auto lin = [](float v) { return v <= 0.03928f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
                return 0.2126f * lin(channel(c, IM_COL32_R_SHIFT))
                    + 0.7152f * lin(channel(c, IM_COL32_G_SHIFT))
                    + 0.0722f * lin(channel(c, IM_COL32_B_SHIFT));
            };
            const float l1 = luminance(textU32);
            const float l2 = luminance(backgroundU32);
            const float ratio = (std::max(l1, l2) + 0.05f) / (std::min(l1, l2) + 0.05f);
            if (ratio < 4.5f)
            {
                char buffer[160];
                std::snprintf(buffer, sizeof(buffer),
                    "文字层 %s 与底色的对比度只有 %.1f:1（正文建议 ≥ 4.5:1）",
                    layer.value("id", std::string("?")).c_str(), ratio);
                this->contrastWarnings.push_back(buffer);
                BKLV_LOG_WARNING("对比度检查：%s", buffer);
            }
        }
    }

    // 焦点顺序：整页预览时按视图树顺序列出可聚焦控件
    this->focusOrder.clear();
    if (!this->currentViewFile.empty())
        for (const bklv::InstantiatedView& entry : this->viewMap)
            if (entry.view != nullptr && entry.view->isFocusable())
                this->focusOrder.push_back(entry.pointer);

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

    // 对齐参考：10px 网格 + 中线（看间距、看居中对不对得齐）
    if (this->showGrid)
    {
        const ImU32 gridColor = IM_COL32(255, 255, 255, 18);
        for (float x = 0; x <= size.x; x += 10.0f)
            drawList->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + size.y),
                gridColor);
        for (float y = 0; y <= size.y; y += 10.0f)
            drawList->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + size.x, origin.y + y),
                gridColor);
        drawList->AddLine(ImVec2(origin.x + size.x * 0.5f, origin.y),
            ImVec2(origin.x + size.x * 0.5f, origin.y + size.y), IM_COL32(0, 255, 204, 40));
        drawList->AddLine(ImVec2(origin.x, origin.y + size.y * 0.5f),
            ImVec2(origin.x + size.x, origin.y + size.y * 0.5f), IM_COL32(0, 255, 204, 40));
    }

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

int DesignerWindow::applyBindingSamples()
{
    if (this->pageBindings.empty() || this->bindingSamples[0] == 0)
        return 0;

    const AppearanceDoc::Json samples
        = AppearanceDoc::Json::parse(this->bindingSamples, nullptr, false);
    if (samples.is_discarded() || !samples.is_object())
        return 0;

    int applied = 0;
    for (const bklv::Binding& binding : this->pageBindings)
    {
        if (!samples.contains(binding.field))
            continue;

        const AppearanceDoc::Json& value = samples[binding.field];
        const std::string text = value.is_string() ? value.get<std::string>() : value.dump();
        if (applyViewAttribute(binding.view, binding.attribute, text))
            applied++;
    }
    return applied;
}

void DesignerWindow::drawViewHighlight(ImVec2 origin, float zoom)
{
    if (this->selection.kind != SelKind::VIEW || this->selection.key.empty())
        return;

    for (const bklv::InstantiatedView& entry : this->viewMap)
    {
        if (entry.pointer != this->selection.key || entry.view == nullptr)
            continue;

        const Rect frame = entry.view->getFrame();
        const ImVec2 topLeft(origin.x + frame.x * zoom, origin.y + frame.y * zoom);
        const ImVec2 bottomRight(
            topLeft.x + frame.width * zoom, topLeft.y + frame.height * zoom);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRect(topLeft, bottomRight, IM_COL32(0, 255, 204, 255), 2.0f, 0, 2.0f);
        // 角标：让"框住的是哪一个"一眼看清
        const char* label = entry.pointer.c_str();
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        drawList->AddRectFilled(ImVec2(topLeft.x, topLeft.y - textSize.y - 4.0f),
            ImVec2(topLeft.x + textSize.x + 8.0f, topLeft.y), IM_COL32(0, 255, 204, 220));
        drawList->AddText(ImVec2(topLeft.x + 4.0f, topLeft.y - textSize.y - 3.0f),
            IM_COL32(8, 32, 28, 255), label);
        return;
    }
}

void DesignerWindow::handlePreviewClick(ImVec2 origin, float zoom)
{
    if (this->currentViewFile.empty() || this->viewMap.empty())
        return;
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        return;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float localX = (mouse.x - origin.x) / zoom;
    const float localY = (mouse.y - origin.y) / zoom;

    // 命中最深的那个节点（后出现的通常是子节点，取面积最小的最稳）
    const bklv::InstantiatedView* hit = nullptr;
    float bestArea                    = 0.0f;
    for (const bklv::InstantiatedView& entry : this->viewMap)
    {
        if (entry.view == nullptr || entry.pointer.empty())
            continue;
        const Rect frame = entry.view->getFrame();
        if (localX < frame.x || localY < frame.y || localX > frame.x + frame.width
            || localY > frame.y + frame.height)
            continue;

        const float area = frame.width * frame.height;
        if (hit == nullptr || area < bestArea)
        {
            hit      = &entry;
            bestArea = area;
        }
    }

    if (hit == nullptr)
        return;

    this->selection      = Selection{};
    this->selection.kind = SelKind::VIEW;
    this->selection.key  = hit->pointer;
    this->pendingExpandPath = hit->pointer; // 让左树展开到它
    this->statusMessage  = "反选到 " + hit->pointer;
}

void DesignerWindow::drawPreview()
{
    // ---- 控制行 1：画布 ----
    panelHeader("预览", this->dirty
            ? ("未保存 · 改动 " + std::to_string(this->changedFieldPaths.size()) + " 处").c_str()
            : "选中什么渲染什么");
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
    ImGui::Checkbox("对齐网格", &this->showGrid);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("10px 网格 + 中线：对间距、看居中最有用");
    ImGui::SameLine();
    if (ImGui::SmallButton("重置视图"))
    {
        this->canvasZoom = 2;
        this->canvasPanX = 0.0f;
        this->canvasPanY = 0.0f;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("滚轮缩放、中键（或按住空格）拖动平移");

    float zoom = this->canvasZoom == 0 ? 0.5f : this->canvasZoom == 1 ? 0.75f
        : this->canvasZoom == 2 ? 1.0f : 1.5f;

    // 自适应模式下让画布尽量铺满预览区（挑一个放得下的最大档），
    // 省得中间一大片空白、看着像没画完
    if (this->canvasPreset == 0)
    {
        const ImVec2 stage = ImGui::GetContentRegionAvail();
        // 左边可能还有层序条（132 + 间距），算画布能不能放下时要扣掉，
        // 否则自适应会挑一个大一档的缩放、画布右侧被裁（踩过）
        const bool stripVisible = !this->currentWidget.empty() && this->doc.layerCount() > 0
            && !this->previewCompare;
        const float stageW = stage.x - (stripVisible ? 148.0f : 0.0f);
        const float needW  = (float)this->canvasWidth;
        const float needH  = (float)this->canvasHeight - 96.0f; // 上面的控制行
        const float fit    = std::min(stageW / std::max(1.0f, needW),
                          stage.y / std::max(1.0f, needH));
        const float choices[] = { 0.5f, 0.75f, 1.0f, 1.5f };
        for (const float choice : choices)
            if (choice <= fit + 0.001f)
                zoom = choice;
    }

    // ---- 控制行 2：状态工具栏 + 上下文/子树/对比 ----
    ImGui::TextColored(mutedColor(), "状态");
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

    // 外部仓库：预览里的 @res/ 仍按设计器自己的 resources 解析，得说明白
    {
        static bool warnedExternal = false;
        const std::string ownResources = resourcePath("resources");
        if (!warnedExternal && this->project.hasResourcesOutside(ownResources))
        {
            warnedExternal = true;
            BKLV_LOG_WARNING("这个项目的 resources 不在设计器旁边（%s）：预览里图片的 @res/ 仍按"
                             "设计器自己的 resources 解析，可能显示不出正确素材",
                this->project.resourcesDir().c_str());
        }
    }

    // ---- 差异标注：设计器与真机可能不一样的地方，写在明面上 ----
    {
        const float uiScale = (float)this->canvasWidth / 1280.0f;
        std::string shortDir = this->project.themeDir();
        {
            // 只显示路径最后两段，标注一行放得下
            const size_t last = shortDir.find_last_of('/');
            const size_t prev = last == std::string::npos ? std::string::npos
                                                          : shortDir.find_last_of('/', last - 1);
            if (prev != std::string::npos && prev + 1 < shortDir.size())
                shortDir = "…" + shortDir.substr(prev);
        }
        ImGui::TextColored(mutedColor(), "画布 %d×%d · UI 缩放 ≈ %.2f×（真机=窗口宽/1280）· %s",
            this->canvasWidth, this->canvasHeight, uiScale, shortDir.c_str());
        if (this->canvasWidth != 1280)
        {
            if (ImGui::SmallButton("按真机 720p 画布"))
            {
                this->canvasPreset   = 1; // 1280×720
                this->canvasChecker  = true;
                this->previewDirty   = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("主题里的尺寸都按 1280×720 写：量间距、看字号对齐时用这个画布最准");
        }
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
    {
        modeText = this->previewSubtree ? "页面视图 → 只渲染选中的那一棵子树"
                                        : "页面视图 → 真实实例化整页";
        // 页面里有绑定的话，预览拿不到数据（列表/卡片会是空的），得说明白
        if (this->doc.root().dump().find("\"bind\"") != std::string::npos)
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f),
                "⚠ 这个页面有数据绑定（bind）：预览里没有数据源，绑定出来的内容会是空的");
    }
    if (this->previewCompare)
        modeText = "并排对比：左=编辑中 / 右=磁盘上那份";
    ImGui::TextColored(mutedColor(), "%s", modeText);

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

    // 层序条：按绘制顺序列出各层（第一个画的在最上面），点一下就选中那一层。
    // 这是"我在改哪一层、它在绘制顺序里的位置"最省事的答案 —— 比在预览里猜哪块是哪层靠谱。
    if (this->currentWidget.empty() == false && this->doc.layerCount() > 0
        && !this->previewCompare)
    {
        ImGui::BeginChild("##layerStrip", ImVec2(132.0f, 0.0f), true);
        ImGui::TextColored(mutedColor(), "绘制顺序");
        for (int i = 0; i < this->doc.layerCount(); i++)
        {
            const bool selected = this->selection.kind == SelKind::LAYER
                && this->selection.index == i;
            std::string label = "    #" + std::to_string(i + 1) + " " + this->doc.layerOp(i);

            // 用一个色块示意这层画出来的颜色（能解出色值就解，解不出用暗色）
            AppearanceDoc::Json fillValue;
            std::string fillText;
            if (this->doc.getLayerValue(i, "fill", fillValue) && fillValue.is_string())
                fillText = fillValue.get<std::string>();
            ImU32 swatch = 0;
            if (const std::string key = themeKeyFromReference(fillText); !key.empty())
                swatch = getTheme().color(key, 0);
            else
                swatch = parseHexColor(fillText);
            const bool translucent = (swatch & IM_COL32_A_MASK) < IM_COL32(255, 255, 255, 255);

            const ImVec2 rowStart = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable((label + "##" + std::to_string(i)).c_str(), selected,
                    ImGuiSelectableFlags_None, ImVec2(0, 20)))
            {
                this->selection = Selection{};
                this->selection.kind  = SelKind::LAYER;
                this->selection.index = i;
                this->previewDirty    = true;
            }
            // 透明/半透明用棋盘纹示意（和预览画布的"网格底"一个意思）：
            // 直接画一块淡色的话，看不出"这层是透明的"
            const ImVec2 chipMin(rowStart.x + 3.0f, rowStart.y + 3.0f);
            const ImVec2 chipMax(rowStart.x + 17.0f, rowStart.y + 17.0f);
            if (translucent)
            {
                ImGui::GetWindowDrawList()->AddRectFilled(chipMin, chipMax,
                    IM_COL32(70, 70, 70, 255), 3.0f);
                for (float x = chipMin.x; x < chipMax.x; x += 4.0f)
                    for (float y = chipMin.y; y < chipMax.y; y += 4.0f)
                        if (((int)((x - chipMin.x) / 4.0f) + (int)((y - chipMin.y) / 4.0f)) % 2 == 0)
                            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x, y),
                                ImVec2(std::min(x + 4.0f, chipMax.x), std::min(y + 4.0f, chipMax.y)),
                                IM_COL32(110, 110, 110, 255));
                // 再叠一层这层自己的颜色，能看出半透明的色偏
                if ((swatch & IM_COL32_A_MASK) != 0)
                    ImGui::GetWindowDrawList()->AddRectFilled(chipMin, chipMax, swatch, 3.0f);
            }
            else
                ImGui::GetWindowDrawList()->AddRectFilled(chipMin, chipMax, swatch, 3.0f);
        }
        ImGui::EndChild();
        ImGui::SameLine();
    }

    // 画布在预览区里居中（以前贴左上角，看着像没画完）
    const ImVec2 stageAvail = ImGui::GetContentRegionAvail();
    const ImVec2 size((float)this->canvasWidth * zoom, (float)this->canvasHeight * zoom);
    const float centeringX  = std::max(0.0f, (stageAvail.x - size.x) * 0.5f);
    const float centeringY  = std::max(0.0f, (stageAvail.y - size.y) * 0.5f);
    const ImVec2 stagePos(ImGui::GetCursorScreenPos().x + centeringX + this->canvasPanX,
        ImGui::GetCursorScreenPos().y + centeringY + this->canvasPanY);

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
        if (!this->currentViewFile.empty() && !this->previewSubtree)
        {
            this->drawViewHighlight(stagePos, zoom);
            this->handlePreviewClick(stagePos, zoom);

            // 焦点顺序标号（在预览里按顺序打 1 2 3…）
            if (this->highlightFocusOrder)
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                int order            = 0;
                for (const std::string& pointer : this->focusOrder)
                {
                    order++;
                    for (const bklv::InstantiatedView& entry : this->viewMap)
                    {
                        if (entry.pointer != pointer || entry.view == nullptr)
                            continue;
                        const Rect frame = entry.view->getFrame();
                        const ImVec2 at(stagePos.x + frame.x * zoom + 2.0f,
                            stagePos.y + frame.y * zoom + 2.0f);
                        drawList->AddCircleFilled(ImVec2(at.x + 9.0f, at.y + 9.0f), 10.0f,
                            IM_COL32(255, 183, 77, 235));
                        char number[8];
                        std::snprintf(number, sizeof(number), "%d", order);
                        drawList->AddText(ImVec2(at.x + (order < 10 ? 5.0f : 2.0f), at.y + 2.0f),
                            IM_COL32(30, 20, 0, 255), number);
                        break;
                    }
                }
            }
        }
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
        ImGui::TextColored(mutedColor(), "选一个文件");
        return;
    }

    if (!this->currentTable.empty())
    {
        this->drawTableProps();
        return;
    }

    if (!this->currentViewFile.empty())
    {
        panelHeader("页面视图", this->currentViewFile.c_str());
        ImGui::TextColored(mutedColor(), "直接改下面的 JSON：点「应用」会先做语法校验 + 试实例化，"
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
        ImGui::TextColored(mutedColor(), "%s", this->dirty ? "● 有未保存改动" : "已同步");

        if (!this->errorMessage.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f), "%s", this->errorMessage.c_str());
        return;
    }

    panelHeader("属性", this->currentWidget.c_str());
    ImGui::TextColored(mutedColor(), "%s", this->currentFilePath.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("复制 JSON"))
    {
        // 复制"当前选中的那一段"：层 / box / 整个外观 —— 贴进代码或 issue 里都方便
        std::string text;
        if (this->selection.kind == SelKind::LAYER && this->doc.layer(this->selection.index))
            text = this->doc.layer(this->selection.index)->dump(2);
        else if (this->selection.kind == SelKind::BOX)
            text = this->doc.root().value("box", AppearanceDoc::Json::object()).dump(2);
        else
            text = this->doc.root().dump(2);

        ImGui::SetClipboardText(text.c_str());
        this->statusMessage = "已复制 " + std::to_string(text.size()) + " 字节 JSON 到剪贴板";
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("复制当前选中节点那一段的 JSON（选中层 → 该层；选中 box → box；否则整份）");
    if (!this->changedFieldPaths.empty())
    {
        if (ImGui::CollapsingHeader(("改动 " + std::to_string(this->changedFieldPaths.size())
                + " 处（点条目定位）###changedList").c_str()))
        {
            ImGui::Indent(10.0f);
            for (const std::string& path : this->changedFieldPaths)
            {
                if (ImGui::Selectable((path + "##chg" + path).c_str(), false,
                        ImGuiSelectableFlags_AllowDoubleClick))
                    this->selectByChangePath(path);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("点一下：跳到这一处");
            }
            ImGui::Unindent(10.0f);
        }

        if (ImGui::SmallButton("下一处改动 (F3)"))
            this->selectNextChange(1);
        ImGui::SameLine();
        if (ImGui::SmallButton("上一处"))
            this->selectNextChange(-1);
        ImGui::SameLine();
        ImGui::TextColored(accentColor(), "%d 处", (int)this->changedFieldPaths.size());
    }

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##propFilter", "过滤字段（比如 fill / radius）…",
        this->propFilter, sizeof(this->propFilter));
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
            ImGui::TextColored(mutedColor(), "盒模型");
            const char* numberKeys[] = { "height", "minWidth", "radius", "paddingTop",
                "paddingSides", "gap" };
            for (const char* key : numberKeys)
            {
                if (this->propFilter[0] != 0 && std::string(key).find(this->propFilter)
                        == std::string::npos)
                    continue;
                double value = 0.0;
                const bool has = this->doc.getBoxNumber(key, value);
                const std::string label = this->isFieldChanged(std::string("box.") + key)
                    ? std::string("• ") + key // 改过的字段前面点一个强调色圆点
                    : key;                    // 未设置的状态用暗色点标出来，不挤标签
                if (label[0] == '\xe2' || label.rfind("• ", 0) == 0)
                    ImGui::TextColored(accentColor(), "•");
                if (label.rfind("• ", 0) == 0)
                    ImGui::SameLine();
                propLabel(label.rfind("• ", 0) == 0 ? label.c_str() + 2 : label.c_str());
                if (!has)
                {
                    ImGui::SameLine(kLabelWidth - 12.0f);
                    ImGui::TextColored(mutedColor(), "·");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("这个字段当前没写（改了才会写进来）");
                    ImGui::SameLine();
                }
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

            ImGui::Spacing();
            ImGui::TextColored(accentColor(), "布局");
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

            ImGui::Spacing();
            ImGui::TextColored(accentColor(), "颜色");
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

            ImGui::TextColored(mutedColor(), "第 %d 层", this->selection.index + 1);

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
                if (this->propFilter[0] != 0
                    && key.find(this->propFilter) == std::string::npos)
                {
                    ImGui::PopID();
                    continue; // 过滤词不匹配的字段不画
                }

                const bool fieldChanged
                    = this->isFieldChanged("layers[" + std::to_string(this->selection.index)
                        + "]." + key);
                if (fieldChanged)
                {
                    ImGui::TextColored(accentColor(), "•");
                    ImGui::SameLine();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("这个字段相对磁盘上的版本改过");
                }

                const std::string kind = valueKind(it.value());
                bool changed = false;

                // 枚举字段给下拉（align / valign / scaling / axis / direction）
                if (kind == "string" && (key == "align" || key == "valign" || key == "scaling"
                                            || key == "axis" || key == "direction"))
                {
                    std::string text = it.value().get<std::string>();
                    if (this->drawEnumCombo(key.c_str(), key, text))
                    {
                        it.value() = text;
                        changed    = true;
                    }
                    if (changed)
                        edited();
                    ImGui::PopID();
                    continue;
                }

                if (kind == "number")
                {
                    double value = it.value().get<double>();
                    changed = inputNumberFloat(key.c_str(), value);
                    if (changed)
                        it.value() = numberValue(value);
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
                if (key != "id" && key != "op" && this->doc.layerCount() > 1)
                {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("套到其它层"))
                        this->openFieldSync(key);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("把这一层的 %s 套到别的层（多个层要设同一套参数时省事）",
                            key.c_str());
                }

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

            if (this->propFilter[0] != 0 && layer->empty())
                ImGui::TextColored(mutedColor(), "（没有匹配 \"%s\" 的字段）", this->propFilter);

            ImGui::Spacing();
            ImGui::TextColored(accentColor(), "层操作");
            if (ImGui::SmallButton("复制层"))
            {
                if (const AppearanceDoc::Json* layer = this->doc.layer(this->selection.index))
                {
                    this->layerClipboard    = *layer;
                    this->hasLayerClipboard = true;
                    this->statusMessage = "已复制第 " + std::to_string(this->selection.index + 1)
                        + " 层（可粘到别的外观里）";
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("复制整层到剪贴板，可以切到别的外观再粘");
            ImGui::SameLine();
            ImGui::BeginDisabled(!this->hasLayerClipboard);
            if (ImGui::SmallButton("粘贴层"))
            {
                const int index = this->doc.appendLayer(this->layerClipboard);
                if (index >= 0)
                {
                    this->pushUndoSnapshot();
                    this->markEdited();
                    this->previewDirty = true;
                    this->selection    = Selection{};
                    this->selection.kind  = SelKind::LAYER;
                    this->selection.index = index;
                    this->statusMessage = "已粘贴为第 " + std::to_string(index + 1) + " 层"
                        + (this->doc.layerId(index).empty()
                               ? ""
                               : "（id " + this->doc.layerId(index) + "）");
                }
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", this->hasLayerClipboard
                        ? "追加到当前外观末尾（id 会自动改唯一）"
                        : "剪贴板里还没有层：先在某个外观里点「复制层」");
            ImGui::Separator();
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
            ImGui::TextColored(mutedColor(), "状态 %s", state.c_str());

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
                    ImGui::TextColored(accentColor(), "%s", it.key().c_str());
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
            ImGui::TextColored(mutedColor(), "选中层 / box / 状态看属性");
            break;
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(mutedColor(), "layers（顺序 = 绘制顺序）");
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
    // 只读保护的是**主题文件**；页面视图在 resources/views/ 下，照常可存
    /* 逐份判断，见循环里的 kind 分支 */

    // 当前页面视图里"改了没点应用"的文本也要先应用，否则全部保存会把它漏掉
    {
        std::string pendingError;
        if (!this->applyPendingViewText(pendingError))
        {
            this->errorMessage = pendingError;
            return;
        }
    }

    this->stashCurrent(); // 不先存当前这份的话，会把它上一次 stash 的旧内容写下去

    const std::string original = this->currentFilePath;
    int saved                  = 0;
    int conflicts              = 0;

    for (auto& entry : this->drafts)
    {
        if (!entry.second.dirty)
            continue;

        const std::string& path = entry.first;
        std::string text        = entry.second.doc.serialize();
        std::string error;

        if (!validateJsonText(text, error))
        {
            this->warnings.push_back(path + "：" + error);
            continue;
        }

        // 按文件类型选校验方式：外观要编译、页面要试实例化、主题表只看语法。
        // 以前是"先试实例化、失败再试编译"，主题表两种都失败 → **主题表根本存不进**
        const FileKind kind = classifyThemeFile(path, this->project.themeDir(),
            this->project.resourcesDir());
        if (kind == FileKind::VIEW)
        {
            View* probe = createFromJSONString(text);
            if (probe == nullptr)
            {
                this->warnings.push_back(path + "：视图实例化失败，跳过");
                continue;
            }
            delete probe;
        }
        else if (kind == FileKind::APPEARANCE)
        {
            if (Appearance::compile(text, getTheme(), error) == nullptr)
            {
                this->warnings.push_back(path + "：编译失败 " + error);
                continue;
            }
        }
        else if (kind == FileKind::UNKNOWN)
        {
            this->warnings.push_back(path + "：认不出是什么文件，跳过");
            continue;
        }

        if (this->readOnly && kind != FileKind::VIEW)
        {
            this->warnings.push_back(path + "：基准主题只读，跳过（页面视图不受限）");
            continue;
        }

        // 和单个保存一样走带冲突检测的写：磁盘被外部改过就跳过并报告
        if (writeFileChecked(path, entry.second.openHash, text, false, true, error))
        {
            entry.second.dirty     = false;
            entry.second.savedText = text;
            entry.second.openHash  = this->hashText(text);
            saved++;
        }
        else
        {
            conflicts++;
            this->warnings.push_back(path + "：" + error);
        }
    }

    this->currentFilePath = original;
    if (saved > 0)
    {
        this->refreshThemeUsage();
        std::string reloadError;
        this->applyThemeFromProject(reloadError);
        this->dirty         = false;
        this->previewDirty  = true;
        this->statusMessage = std::to_string(saved) + " 个文件已写入"
            + (conflicts > 0 ? "，" + std::to_string(conflicts) + " 个被外部改动挡住" : "");
    }
    else
        this->statusMessage
            = conflicts > 0 ? "都被外部改动挡住了，见校验面板" : "没有需要写入的改动";
}

void DesignerWindow::applyThemeFromProject(std::string& error)
{
    if (!loadThemeIntoGlobalFromDirectory(this->project.themeDir(), this->project.name))
    {
        error = "主题装载失败：" + this->project.themeDir();
        return;
    }

    // 配色跟着主题走：换主题 / 热重载之后设计器自己也换色
    applyDesignerStyle();
}

bool DesignerWindow::applyPendingViewText(std::string& error)
{
    // 页面视图：文本改过但没点「应用」时，先自动应用一次
    // （不然 diff 和保存用的都是上一次应用过的内容，用户刚打的字静默丢掉）
    if (this->currentViewFile.empty() || this->viewText == this->viewTextApplied)
        return true;

    if (!this->applyViewText(error))
    {
        error = "文本还没通过校验，先修好再保存：" + error;
        return false;
    }
    this->statusMessage = "文本已自动应用（原来是改了没点「应用」）";
    return true;
}

void DesignerWindow::requestSave()
{
    if (this->readOnly && this->currentViewFile.empty())
    {
        // 按钮虽然是灰的，但 Ctrl+S / 冲突弹层会绕过它 —— 基准主题绝不能被写。
        // 页面视图不属于主题（在 resources/views/ 下），只读时照样能改。
        this->errorMessage = "基准主题（default / ocean）只读：请先「新建项目」用模板建一份自己的主题";
        BKLV_LOG_WARNING("拒绝写盘：%s 是只读的基准主题", this->project.name.c_str());
        return;
    }

    if (this->fileChangedExternally() && !this->allowOverwrite)
    {
        this->showConflict = true;
        return;
    }

    std::string applyError;
    if (!this->applyPendingViewText(applyError))
    {
        this->errorMessage = applyError;
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
    this->diffPaths.clear();
    for (const JsonChange& change : changes)
    {
        this->diffLines.push_back(changeSummary(change));
        this->diffPaths.push_back(change.path);
    }

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

    // 自查钩子 / 拦截弹层都会直接走这里，所以这条也要自己兜住"改了没点应用"
    {
        std::string pendingError;
        if (!this->applyPendingViewText(pendingError))
        {
            this->errorMessage = pendingError;
            return;
        }
    }

    if (this->readOnly && this->currentViewFile.empty())
    {
        this->errorMessage = "只读主题，不能写盘（页面视图不受限）";
        return;
    }

    // 沿用原文件的缩进风格；内容与磁盘一致就不写（省一次无意义的整文件重写 + .bak 轮换）
    const char indentChar  = detectIndentChar(this->savedText);
    const int indentWidth  = detectIndentWidth(this->savedText);
    const std::string text = serializeJson(this->doc.root(), indentChar, indentWidth);

    std::string error;
    if (text == this->savedText)
    {
        this->dirty = false;
        if (!this->currentFilePath.empty())
            this->currentDraft().dirty = false;
        this->statusMessage = "内容没变，跳过写盘";
        return;
    }
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
        // 草稿的脏标记也要清：不清的话文件列表一直显示 ●（改动总览里也挂着）
        const auto draftForView = this->drafts.find(this->currentFilePath);
        if (draftForView != this->drafts.end())
        {
            draftForView->second.dirty     = false;
            draftForView->second.savedText = text;
        }
    }

    // 增量写盘：只把变化了的顶层段落到原文本上（手写排版/空行/段序都保住）。
    // 做不到（增删了顶层键）就退回整文件重写，并把原因写进日志。
    std::string writeText = text;
    {
        std::string patched;
        std::string patchError;
        if (patchTopLevel(this->savedText, text, patched, patchError))
            writeText = patched;
        else
            BKLV_LOG_INFO("增量写盘不可用（%s）：退回整文件重写", patchError.c_str());
    }

    // 带冲突检测的原子写（core 层，能无窗口测）：磁盘内容变了就拒绝（除非刚点过"覆盖写"）
    const auto draftForWrite = this->drafts.find(this->currentFilePath);
    const std::string expected
        = draftForWrite != this->drafts.end() ? draftForWrite->second.openHash : std::string();
    if (!writeFileChecked(this->currentFilePath, expected, writeText, this->allowOverwrite, true, error))
    {
        this->errorMessage = error;
        this->showConflict = true; // 让用户选覆盖 / 重新载入
        return;
    }

    this->savedText = writeText;
    this->dirty     = false;

    // 草稿同步：脏标记清掉、指纹更新成刚写下去的内容
    Draft& draft   = this->currentDraft();
    draft.savedText = writeText;
    draft.dirty     = false;
    draft.openHash  = this->hashText(text);

    // 整主题校验结果进"校验面板"
    this->warnings.clear();
    std::string themeError;
    if (!validateThemeAppearances(themeError))
        this->warnings.push_back("整主题校验失败：" + themeError);
    for (const std::string& warning : this->doc.layerOrderWarnings())
        this->warnings.push_back("层序：" + warning);

    this->includeCache.clear(); // 被包含的文件也可能刚被改过
    this->refreshThemeUsage(); // 刚写完，引用关系可能变了

    // 主题表改动的影响清单：这次动到的 key 影响了哪些控件
    if (!this->currentTable.empty())
    {
        const std::string keyPrefix
            = this->currentTable.rfind("metrics", 0) == 0 ? "metric." : "color.";
        AppearanceDoc::Json beforeTable
            = AppearanceDoc::Json::parse(this->savedText, nullptr, false);

        std::vector<std::string> affected;
        if (!beforeTable.is_discarded())
        {
            for (const JsonChange& change : diffJson(beforeTable, this->doc.root()))
            {
                // 差异路径形如 common.red / radius.md：取前两段当 key
                std::string key = change.path;
                const size_t second = key.find('.', key.find('.') + 1);
                if (second != std::string::npos)
                    key = key.substr(0, second);
                const auto found = this->themeUsage.find(keyPrefix + key);
                if (found == this->themeUsage.end())
                    continue;
                for (const std::string& widget : found->second)
                    if (std::find(affected.begin(), affected.end(), widget) == affected.end())
                        affected.push_back(widget);
            }
        }
        if (!affected.empty())
        {
            std::string list;
            for (const std::string& widget : affected)
                list += (list.empty() ? "" : "、") + widget;
            BKLV_LOG_INFO("主题表改动影响 %d 个外观：%s", (int)affected.size(), list.c_str());
            this->statusMessage += "；影响 " + std::to_string(affected.size()) + " 个外观";
        }
    }

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

void DesignerWindow::openBrowseAt(const std::string& path)
{
    this->browsePath = path.empty() ? homeDirectory() : path;
    std::snprintf(this->browsePathInput, sizeof(this->browsePathInput), "%s",
        this->browsePath.c_str());
    this->browseSelection.clear();
    this->openProjectError.clear();
    this->refreshBrowse();
}

void DesignerWindow::selectByChangePath(const std::string& path)
{
    const PathTarget target = parseChangePath(path);
    this->selection         = Selection{};
    switch (target.kind)
    {
        case PathTarget::Kind::BOX:
            this->selection.kind = SelKind::BOX;
            break;
        case PathTarget::Kind::BOX_FIELD:
            this->selection.kind = SelKind::BOX;
            this->selection.key  = target.key;
            break;
        case PathTarget::Kind::LAYER:
            this->selection.kind  = SelKind::LAYER;
            this->selection.index = target.layerIndex;
            break;
        case PathTarget::Kind::LAYER_FIELD:
            this->selection.kind  = SelKind::LAYER_FIELD;
            this->selection.index = target.layerIndex;
            this->selection.key   = target.key;
            break;
        default:
            this->selection.kind = SelKind::FILE;
            this->statusMessage  = "这一处改动在面板上定位不到（" + path + "）";
            this->previewDirty   = true;
            return;
    }
    this->previewDirty  = true;
    this->statusMessage = "已定位到 " + path;
}

int DesignerWindow::selectionIndexForTest() const
{
    return this->selection.index;
}

void DesignerWindow::drawSelectionBreadcrumb()
{
    // 一眼看清"我现在改的是哪个文件的哪一层哪个字段"，而且能点着往回退
    auto crumb = [this](const char* text, SelKind kind, int index, const char* key,
        bool last) {
        const bool isCurrent = (this->selection.kind == kind)
            && (kind == SelKind::FILE || this->selection.index == index)
            && (key == nullptr || this->selection.key == key);
        if (isCurrent)
            ImGui::TextColored(accentColor(), "%s", text);
        else
            ImGui::TextColored(mutedColor(), "%s", text);

        if (!last)
        {
            const bool hovered = ImGui::IsItemHovered();
            if (hovered)
            {
                ImGui::SetTooltip("点一下回到这里");
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                this->selection      = Selection{};
                this->selection.kind = kind;
                this->selection.index = index;
                if (key != nullptr)
                    this->selection.key = key;
                this->previewDirty   = true;
            }
            ImGui::SameLine();
            ImGui::TextColored(mutedColor(), "/");
            ImGui::SameLine();
        }
    };

    ImGui::TextColored(mutedColor(), "选中");
    ImGui::SameLine();

    if (!this->currentWidget.empty())
    {
        crumb(this->currentWidget.c_str(), SelKind::FILE, -1, nullptr,
            this->selection.kind == SelKind::FILE);
    }
    else if (!this->currentViewFile.empty())
    {
        crumb(this->currentViewFile.c_str(), SelKind::FILE, -1, nullptr,
            this->selection.kind == SelKind::FILE);
    }
    else if (!this->currentTable.empty())
    {
        crumb(this->currentTable.c_str(), SelKind::TABLE, -1, nullptr,
            this->selection.kind == SelKind::TABLE);
    }

    switch (this->selection.kind)
    {
        case SelKind::BOX:
            crumb("盒模型", SelKind::BOX, -1, nullptr, true);
            break;
        case SelKind::LAYER:
            crumb(("#" + std::to_string(this->selection.index + 1) + " 层").c_str(),
                SelKind::LAYER, this->selection.index, nullptr, true);
            break;
        case SelKind::LAYER_FIELD:
            crumb(("#" + std::to_string(this->selection.index + 1) + " 层").c_str(),
                SelKind::LAYER, this->selection.index, nullptr, false);
            crumb(this->selection.key.c_str(), SelKind::LAYER_FIELD, this->selection.index,
                this->selection.key.c_str(), true);
            break;
        default:
            break;
    }
    ImGui::NewLine();
}

void DesignerWindow::selectNextChange(int step)
{
    // 自己先刷一次：这个集合平时是在帧里算的，从别处（自查钩子/快捷键之外的调用）进来
    // 可能还没算过 —— 依赖调用时机会变成一个很难查的空集合（踩过）
    this->refreshChangedPaths();

    if (this->changedFieldPaths.empty())
    {
        this->statusMessage = "这份文件还没有未保存的改动";
        return;
    }

    // changedFieldPaths 是有序集合，按路径顺序循环走
    std::vector<std::string> paths(this->changedFieldPaths.begin(), this->changedFieldPaths.end());
    size_t index = 0;
    const auto found = std::find(paths.begin(), paths.end(), this->changeCursorPath);
    if (found != paths.end())
    {
        const ptrdiff_t current = std::distance(paths.begin(), found);
        index = (size_t)((current + step + (ptrdiff_t)paths.size()) % (ptrdiff_t)paths.size());
    }
    else
        index = step >= 0 ? 0 : paths.size() - 1;

    this->changeCursorPath = paths[index];
    this->selectByChangePath(this->changeCursorPath);
    this->statusMessage = "改动 " + std::to_string(index + 1) + "/"
        + std::to_string(paths.size()) + "：" + this->changeCursorPath;
}

bool DesignerWindow::isLayerChanged(int index) const
{
    const std::string prefix = "layers[" + std::to_string(index) + "].";
    for (const std::string& path : this->changedFieldPaths)
        if (path.rfind(prefix, 0) == 0)
            return true;
    return false;
}

void DesignerWindow::refreshChangedPaths()
{
    // 缓存键用**文档自身的序列化**：之前用 viewTextApplied，它在某些路径上还是空串，
    // 与初始值相同 → 直接早退，集合永远是空的（自查钩子把这个照出来了）
    const std::string now = this->currentFilePath.empty() ? std::string() : this->doc.serialize();
    if (now == this->changedPathsSource)
        return; // 文档没变，不用重算

    this->changedPathsSource = now;
    this->changedFieldPaths.clear();
    if (!this->dirty || this->currentFilePath.empty())
        return;

    // 和"磁盘上那份"比：给属性表和树里的字段打标记，一眼看出刚改了什么
    // savedText 就是"磁盘上那份"（打开/保存时同步过）
    const AppearanceDoc::Json before
        = AppearanceDoc::Json::parse(this->currentDraft().savedText, nullptr, false);
    if (before.is_discarded())
        return;
    this->changedFieldPaths = changedPaths(before, this->doc.root());
}

void DesignerWindow::refreshBrowse()
{
    std::string error;
    if (!listDirectory(this->browsePath, this->browseEntries, error))
    {
        this->browseEntries.clear();
        this->openProjectError = error;
    }
}

void DesignerWindow::drawOpenProjectModal()
{
    if (this->showOpenProject)
    {
        ImGui::OpenPopup("打开项目");
        this->showOpenProject = false;
    }

    ImGui::SetNextWindowSize(ImVec2(760, 540), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("打开项目", nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;

    // ---- 模式 ----
    ImGui::TextColored(accentColor(), "要做什么");
    ImGui::RadioButton("用模板新建一个主题（原主题不动）", &this->newProjectMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("直接编辑选中的主题", &this->newProjectMode, 1);
    ImGui::Separator();

    // ---- 路径栏 ----
    if (ImGui::Button("上一级"))
        this->openBrowseAt(parentDirectory(this->browsePath));
    ImGui::SameLine();
    if (ImGui::Button("主目录"))
        this->openBrowseAt(homeDirectory());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-160.0f);
    if (ImGui::InputText("##path", this->browsePathInput, sizeof(this->browsePathInput),
            ImGuiInputTextFlags_EnterReturnsTrue))
        this->openBrowseAt(this->browsePathInput);
    ImGui::SameLine();
    if (ImGui::Button("前往", ImVec2(70, 0)))
        this->openBrowseAt(this->browsePathInput);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("也可以直接把路径粘进来回车");

    // ---- 列表 ----
    ImGui::BeginChild("##browse", ImVec2(0, 300), true);
    for (const DirEntry& entry : this->browseEntries)
    {
        if (!entry.isDirectory)
            continue; // 这里只关心主题目录；文件不在这里选

        const bool selected = (entry.path == this->browseSelection);
        std::string label = "[目录] " + entry.name;
        if (entry.hasThemeManifest)
            label += "   ← 主题（有 theme.json）";

        if (ImGui::Selectable(label.c_str(), selected,
                ImGuiSelectableFlags_AllowDoubleClick))
        {
            this->browseSelection  = entry.path;
            this->openProjectError.clear();
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                this->openBrowseAt(entry.path);
        }
        if (ImGui::IsItemHovered() && entry.hasThemeManifest)
            ImGui::SetTooltip("双击进入；点「%s」直接用它",
                this->newProjectMode == 0 ? "当模板新建" : "打开");
    }
    if (this->browseEntries.empty())
        ImGui::TextColored(mutedColor(), "  （这个目录里没有子目录）");
    ImGui::EndChild();

    // ---- 选中信息 + 新主题名 ----
    std::string themeRoot;
    std::string themeName;
    std::string infoError;
    const bool selectionIsTheme
        = !this->browseSelection.empty()
        && themeInfoFromManifest(this->browseSelection, themeRoot, themeName, infoError);

    if (this->browseSelection.empty())
        ImGui::TextColored(mutedColor(), "选中一个标着「主题」的目录");
    else if (!selectionIsTheme)
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "%s：%s", this->browseSelection.c_str(),
            infoError.c_str());
    else if (this->newProjectMode == 0)
    {
        ImGui::TextColored(mutedColor(), "模板：%s（在 %s 下）", themeName.c_str(), themeRoot.c_str());
        ImGui::SetNextItemWidth(240);
        ImGui::InputText("新主题名", this->newThemeName, sizeof(this->newThemeName));
    }
    else
        ImGui::TextColored(mutedColor(), "将直接编辑：%s（%s）", themeName.c_str(),
            this->browseSelection.c_str());

    if (!this->openProjectError.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.55f, 1.0f), "%s", this->openProjectError.c_str());

    // ---- 确认 ----
    const bool canConfirm = selectionIsTheme
        && (this->newProjectMode == 1 || this->newThemeName[0] != 0);
    ImGui::BeginDisabled(!canConfirm);
    if (ImGui::Button(this->newProjectMode == 0 ? "当模板新建" : "打开并编辑", ImVec2(150, 0)))
    {
        Project created;
        std::string error;
        const bool ok = this->newProjectMode == 0
            ? createProjectFromTemplate(themeRoot, themeName, this->newThemeName, created, error)
            : openProjectInPlace(themeRoot, themeName, created, error);

        if (ok && this->setProject(created, isBaseTheme(created.name), error))
        {
            this->statusMessage = "已打开项目 " + created.name;
            ImGui::CloseCurrentPopup();
        }
        else
            this->openProjectError = error;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("取消", ImVec2(100, 0)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void DesignerWindow::drawModals()
{


    if (this->showFocusOrder)
    {
        ImGui::OpenPopup("焦点顺序");
        this->showFocusOrder = false;
    }
    if (ImGui::BeginPopupModal("焦点顺序", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (this->currentViewFile.empty())
            ImGui::TextColored(mutedColor(), "打开一个页面视图（views/*.json）才有焦点顺序");
        else if (this->focusOrder.empty())
            ImGui::TextColored(mutedColor(), "这棵视图树里没有可聚焦控件");
        else
        {
            ImGui::TextColored(mutedColor(), "按视图树顺序（方向键导航大致就是这个顺序）");
            for (size_t i = 0; i < this->focusOrder.size(); i++)
                ImGui::BulletText("%d. %s", (int)i + 1, this->focusOrder[i].c_str());
        }

        // 数据绑定：哪个节点绑了哪个字段（预览里没有数据源，绑定出来的内容是空的）
        ImGui::Separator();
        ImGui::TextColored(accentColor(), "数据绑定");
        if (this->pageBindings.empty())
            ImGui::TextColored(mutedColor(), "这个页面没有 bind");
        else
            for (const bklv::Binding& binding : this->pageBindings)
            {
                std::string path = "(未在映射里)";
                for (const bklv::InstantiatedView& entry : this->viewMap)
                    if (entry.view == binding.view)
                    {
                        path = entry.pointer.empty() ? "/" : entry.pointer;
                        break;
                    }
                ImGui::BulletText("%s  →  字段 %s", path.c_str(), binding.field.c_str());
            }
        if (!this->pageBindings.empty())
        {
            ImGui::TextColored(mutedColor(), "示例数据（JSON：字段 → 值）：预览里用它填绑定");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextMultiline("##samples", this->bindingSamples,
                sizeof(this->bindingSamples), ImVec2(-1, 86.0f));
            if (ImGui::Button("套用示例数据", ImVec2(140, 0)))
            {
                // 不在这里直接套：页面里的绑定指针可能已经因为重建而失效。
                // 置个标记，让下一帧"建完预览之后"再套（那条路径上的指针一定是新的）。
                this->bindingSamplesByFile[this->currentFilePath] = this->bindingSamples;
                this->applySamplesPending = true;
                this->previewDirty        = true;
                this->statusMessage       = "示例数据已更新，正在重新套用";
            }
            ImGui::SameLine();
            ImGui::TextColored(mutedColor(), "（改了字段名要重新打开页面）");
        }
        ImGui::Separator();
        ImGui::Checkbox("在预览里标号", &this->highlightFocusOrder);
        ImGui::SameLine();
        if (ImGui::Button("关闭", ImVec2(100, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (this->showHelp)
    {
        ImGui::OpenPopup("快捷键与约定");
        this->showHelp = false;
    }
    if (ImGui::BeginPopupModal("快捷键与约定", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(accentColor(), "快捷键");
        ImGui::BulletText("Ctrl+S 保存（先看 diff）· Ctrl+Z / Ctrl+Y 撤销 · 重做");
        ImGui::BulletText("Ctrl+D 复制当前层 · Alt+↑ / Alt+↓ 上移 / 下移当前层");
        ImGui::BulletText("F1 校验面板 · F2 预览最大化 · F3 跳到下一处改动（Shift+F3 往回）");
        ImGui::BulletText("滚轮缩放画布 · 中键/Shift+左键 平移 · 方向键在数值框里 ±1（Ctrl 十步）");
        ImGui::BulletText("层操作：复制层 / 粘贴层（可跨外观）· 改过的层与字段前面会带 • 点");
        ImGui::Separator();
        ImGui::TextColored(accentColor(), "约定（改样式前值得知道）");
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

    if (this->showFieldSync)
    {
        ImGui::OpenPopup("把字段套到其它层");
        this->showFieldSync = false;
    }
    ImGui::SetNextWindowSize(ImVec2(520.0f, 380.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("把字段套到其它层", nullptr, ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextColored(mutedColor(), "字段：%s（源 = 第 %d 层）", this->syncField.c_str(),
            this->selection.index + 1);
        ImGui::TextColored(mutedColor(), "勾上要一起改的层；没有这个字段的层会被新加上");
        ImGui::Separator();

        ImGui::BeginChild("##syncList", ImVec2(0.0f, 220.0f), true);
        for (int i = 0; i < this->doc.layerCount(); i++)
        {
            if (i == this->selection.index)
                continue;
            const AppearanceDoc::Json* layer = this->doc.layer(i);
            const bool hasField = layer != nullptr && layer->contains(this->syncField);
            bool checked        = i < (int)this->syncTargets.size() && this->syncTargets[i] != 0;
            const std::string label = "第 " + std::to_string(i + 1) + " 层  "
                + this->doc.layerOp(i) + " " + this->doc.layerId(i)
                + (hasField ? "" : "   （没有这个字段，会新加）");
            if (ImGui::Checkbox((label + "##sync" + std::to_string(i)).c_str(), &checked)
                && i < (int)this->syncTargets.size())
                this->syncTargets[i] = checked ? 1 : 0;
        }
        ImGui::EndChild();

        if (ImGui::Button("应用", ImVec2(120, 0)))
        {
            std::vector<int> targets;
            for (int i = 0; i < (int)this->syncTargets.size(); i++)
                if (this->syncTargets[i] != 0)
                    targets.push_back(i);

            const int changed
                = this->doc.applyFieldToLayers(this->selection.index, this->syncField, targets);
            if (changed > 0)
            {
                this->pushUndoSnapshot();
                this->markEdited();
                this->previewDirty = true;
            }
            this->statusMessage = changed > 0
                ? ("已把 " + this->syncField + " 套到 " + std::to_string(changed) + " 个层")
                : "这些层的值本来就一样，没有改动";
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    this->drawOpenProjectModal();

    if (this->showSaveAsVariant)
    {
        ImGui::OpenPopup("另存为变体");
        this->showSaveAsVariant = false;
    }
    if (ImGui::BeginPopupModal("另存为变体", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(mutedColor(), "从 %s 复制一份新外观（含未保存改动），并登记进 theme.json",
            this->currentWidget.c_str());
        ImGui::SetNextItemWidth(320);
        ImGui::InputText("新外观名", this->variantName, sizeof(this->variantName));
        if (ImGui::Button("创建", ImVec2(120, 0)))
        {
            std::string error;
            if (this->saveAsNewAppearance(this->variantName, error))
                ImGui::CloseCurrentPopup();
            else
                this->errorMessage = error;
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (this->showNewView)
    {
        ImGui::OpenPopup("新建页面视图");
        this->showNewView = false;
    }
    if (ImGui::BeginPopupModal("新建页面视图", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(mutedColor(), "会在 %s/views/ 下建文件（views 不扫目录，写出来就能用）",
            this->project.resourcesDir().c_str());
        ImGui::SetNextItemWidth(320);
        ImGui::InputText("页面名", this->newViewName, sizeof(this->newViewName));
        ImGui::SetNextItemWidth(320);
        ImGui::Combo("模板", &this->newViewTemplate,
            "空页面\0整页（页头 + 内容 + 底栏）\0卡片\0");
        static const char* viewTemplates[] = { "empty", "page", "card" };
        if (ImGui::Button("创建", ImVec2(120, 0)))
        {
            std::string error;
            if (createViewFile(this->project.resourcesDir() + "/views", this->newViewName,
                    viewTemplates[this->newViewTemplate], error))
            {
                this->refreshFileList();
                this->openViewFile(std::string(this->newViewName) + ".json");
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

    if (this->showNewAppearance)
    {
        ImGui::OpenPopup("新建外观文件");
        this->showNewAppearance = false;
    }
    if (ImGui::BeginPopupModal("新建外观文件", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(mutedColor(), "会在 %s/widgets/ 下建文件，并登记进 theme.json", this->project.themeDir().c_str());
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
        ImGui::TextColored(mutedColor(), "（文件本身删掉，.bak 也清掉；想反悔只能靠 git）");
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

    // 未保存改动拦截：切文件 / 切主题前弹一次，不再静默丢改动
    if (this->showUnsavedGuard)
    {
        ImGui::OpenPopup("有未保存的改动");
        this->showUnsavedGuard = false;
    }

    std::string themeToSwitch; // 点了按钮之后在弹层外面真正执行（不能在弹层里 return，会跳过 EndPopup）
    bool discardAndSwitch = false;
    if (ImGui::BeginPopupModal("有未保存的改动", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (!this->pendingTheme.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "要切到主题「%s」",
                this->pendingTheme.c_str());
            ImGui::TextUnformatted("有未保存的改动，切主题会清掉草稿，怎么办？");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "%s",
                this->currentFilePath.c_str());
            ImGui::TextUnformatted("这个文件有未保存的改动，切走之前要怎么办？");
        }
        ImGui::Separator();

        if (ImGui::Button("保存并切换", ImVec2(140, 0)))
        {
            if (!this->pendingTheme.empty())
            {
                this->saveAllDirty();
                themeToSwitch = this->pendingTheme;
            }
            else
            {
                this->doSave();
                if (!this->pendingViewFile.empty())
                    this->openViewFile(this->pendingViewFile);
                else
                {
                    std::string openError;
                    this->openAppearance(this->pendingWidget, openError);
                }
                this->dirty = false;
                this->stashCurrent();
            }
            this->pendingPath.clear();
            this->pendingWidget.clear();
            this->pendingViewFile.clear();
            this->pendingTheme.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("丢弃改动", ImVec2(120, 0)))
        {
            this->dirty = false;
            if (!this->pendingTheme.empty())
            {
                themeToSwitch     = this->pendingTheme;
                discardAndSwitch  = true;
                this->pendingTheme.clear();
            }
            else
            {
                this->discardCurrentDraft();
                if (!this->pendingViewFile.empty())
                    this->openViewFile(this->pendingViewFile);
                else
                {
                    std::string openError;
                    this->openAppearance(this->pendingWidget, openError);
                }
                this->pendingPath.clear();
                this->pendingWidget.clear();
                this->pendingViewFile.clear();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(100, 0)))
        {
            this->pendingPath.clear();
            this->pendingWidget.clear();
            this->pendingViewFile.clear();
            this->pendingTheme.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 真正切主题（在弹层外做）
    if (!themeToSwitch.empty() && themeToSwitch != this->project.name)
    {
        Project next;
        next.mode      = ProjectMode::INPLACE;
        next.name      = themeToSwitch;
        next.themeRoot = this->project.themeRoot;
        std::string switchError;
        this->drafts.clear();
        if (!this->setProject(next, isBaseTheme(themeToSwitch), switchError))
            this->errorMessage = switchError;
        else
            this->statusMessage = "已切到主题 " + themeToSwitch
                + (isBaseTheme(themeToSwitch) ? "（只读浏览）" : "");
        (void)discardAndSwitch;
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
        ImGui::TextColored(mutedColor(), "%s", this->currentFilePath.c_str());
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
            ImGui::TextColored(mutedColor(), "（%d 处改动）", changes);
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("切到##") + entry.first).c_str()))
            {
                this->stashCurrent(); // 先把当前文件的改动存进草稿，切过去才不会丢
                this->currentFilePath = entry.first;
                this->currentWidget   = widgetNameFromPath(entry.first);
                this->currentViewFile.clear();
                this->currentTable.clear();
                this->restoreDraftFor(entry.first);
                this->currentWidget = widgetNameFromPath(entry.first);
                ImGui::CloseCurrentPopup();
            }
        }
        if (dirtyCount == 0)
            ImGui::TextColored(mutedColor(), "没有未保存的改动");
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
        ImGui::TextColored(mutedColor(), "保存时会跑整主题校验；这里也能手动跑一次。");
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
        for (const std::string& warning : this->contrastWarnings)
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "对比度：%s", warning.c_str());
        if (this->warnings.empty() && this->doc.layerOrderWarnings().empty())
            ImGui::TextColored(mutedColor(), "没有警告");
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
        ImGui::TextColored(mutedColor(), "导出一份 JSON 到指定路径（不影响当前文件）");
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

    ImGui::SetNextWindowSize(ImVec2(820.0f, 430.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("保存前确认改动", nullptr, ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextColored(mutedColor(), "写入前会先编译校验；不通过就不写盘。会留一份 .bak。");
        ImGui::Separator();

        // 按"改在哪儿"分组 + 可折叠：一处控件动了好几个字段时不再是一长条平铺列表。
        // 每条都能点：点了就把树/属性表定位到那一处。
        if (ImGui::SmallButton(this->diffExpandAll ? "全部收起" : "全部展开"))
            this->diffExpandAll = !this->diffExpandAll;
        ImGui::SameLine();
        ImGui::TextColored(mutedColor(), "共 %d 处改动", (int)this->diffLines.size());
        ImGui::Separator();

        ImGui::BeginChild("##diffList", ImVec2(0.0f, 300.0f), true);

        std::vector<JsonChange> changesVec;
        changesVec.reserve(this->diffLines.size());
        for (size_t i = 0; i < this->diffLines.size(); i++)
        {
            // diffLines 是给人看的字符串，这里只需要路径 → 重新构造一条只带 path 的记录
            JsonChange change;
            change.path = i < this->diffPaths.size() ? this->diffPaths[i] : std::string();
            changesVec.push_back(change);
        }

        const auto grouped = groupChanges(changesVec);
        size_t index       = 0;
        for (const auto& entry : grouped)
        {
            const std::string title = groupLabel(entry.first) + "（"
                + std::to_string(entry.second.size()) + "）###group" + entry.first;

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
            if (this->diffExpandAll)
                flags |= ImGuiTreeNodeFlags_DefaultOpen;

            if (ImGui::TreeNodeEx(title.c_str(), flags))
            {
                ImGui::Indent(12.0f);
                // 组内按原顺序取对应的那几行（grouped 里存的是同一批 path）
                for (const JsonChange& change : entry.second)
                {
                    // 找到这一条在 diffLines 里的下标（路径唯一）
                    size_t lineIndex = 0;
                    for (size_t i = 0; i < this->diffPaths.size(); i++)
                        if (this->diffPaths[i] == change.path)
                        {
                            lineIndex = i;
                            break;
                        }
                    const std::string line = lineIndex < this->diffLines.size()
                        ? this->diffLines[lineIndex]
                        : change.path;
                    if (ImGui::Selectable((line + "##diff" + std::to_string(index++)).c_str(),
                            false, ImGuiSelectableFlags_AllowDoubleClick))
                        this->selectByChangePath(change.path);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s\n点一下：定位到这一处", change.path.c_str());
                }
                ImGui::Unindent(12.0f);
                ImGui::TreePop();
            }
        }

        ImGui::EndChild();
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

    // 第一帧再套一次：attach 时 ImGui 上下文可能还没建好（applyDesignerStyle 会自己判空）
    static bool styleApplied = false;
    if (!styleApplied)
    {
        applyDesignerStyle();
        styleApplied = true;
    }

    this->refreshChangedPaths();

    // 跳到下一处改动：F3 往下、Shift+F3 往上（只在不输入文字时响应）
    if (ImGui::GetCurrentContext() != nullptr && !ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_F3, false))
            this->selectNextChange(ImGui::GetIO().KeyShift ? -1 : 1);
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
        panelHeader("文件", "● 未保存");
        ImGui::BeginChild("##files", ImVec2(0, 250), false);
        this->drawFileList();
        ImGui::EndChild();
        ImGui::Separator();
        panelHeader("节点树", "点哪层渲染哪层");
        if (ImGui::SmallButton("全展开"))
            this->treeOpenRequest = 1;
        ImGui::SameLine();
        if (ImGui::SmallButton("只到层"))
            this->treeOpenRequest = 2;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("把下面的字段收起来，只留层（和「只看层」同一件事，但只生效一次）");
        ImGui::SameLine();
        ImGui::Checkbox("只看层", &this->onlyLayers);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("把每层下面的字段行收起来，只留层本身 —— 找控件、调顺序时更快");
        ImGui::SameLine();
        ImGui::TextColored(mutedColor(), "%d 层", this->doc.layerCount());
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
    this->drawPreview();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##props", ImVec2(rightWidth, 0), true);
    this->drawProps();
    ImGui::EndChild();

    this->drawModals();
    ImGui::End();

    // 日志窗口（复用库里的调试窗口，读的是日志环形缓冲）
    if (this->debugWindow.isEnabled())
        this->debugWindow.draw();

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

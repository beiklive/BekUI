#include "designer/core/project.hpp"

#include <nlohmann/json.hpp>
#include "designer/core/theme_files.hpp"

using Json = nlohmann::ordered_json;

namespace designer
{

std::string Project::resourcesDir() const
{
    const size_t slash = this->themeRoot.find_last_of('/');
    if (slash == std::string::npos)
        return this->themeRoot;
    return this->themeRoot.substr(0, slash);
}

bool isBaseTheme(const std::string& themeName)
{
    return themeName == "default" || themeName == "ocean";
}

bool createProjectFromTemplate(const std::string& themeRoot, const std::string& templateName,
    const std::string& newName, Project& out, std::string& error)
{
    if (newName.empty())
    {
        error = "主题名不能为空";
        return false;
    }
    if (isBaseTheme(newName))
    {
        error = "主题名不能占用基准主题名（default / ocean）";
        return false;
    }

    const std::string source = themeRoot + "/" + templateName;
    const std::string target = themeRoot + "/" + newName;
    if (!directoryExists(source))
    {
        error = "模板主题不存在：" + templateName;
        return false;
    }
    if (!copyDirectory(source, target, error))
        return false;

    // 登记进 themes.json（同目录下另一个主题，只加名字，不改默认主题）
    std::vector<std::string> available;
    std::string defaultName;
    std::string readError;
    if (!readThemesFile(themeRoot, available, defaultName, readError))
    {
        available.clear();
        defaultName = "default";
    }
    if (std::find(available.begin(), available.end(), newName) == available.end())
        available.push_back(newName);

    if (!writeThemesFile(themeRoot, available, defaultName, error))
        return false;

    // theme.json 里的 name 字段跟着改，避免主题自己报旧名字
    std::string themeJsonText;
    if (readTextFile(target + "/theme.json", themeJsonText, error))
    {
        Json themeJson = Json::parse(themeJsonText, nullptr, false);
        if (!themeJson.is_discarded() && themeJson.is_object())
        {
            themeJson["name"] = newName;
            std::string writeError;
            writeTextFileAtomic(target + "/theme.json", themeJson.dump(2) + "\n", false, writeError);
        }
    }

    out.mode         = ProjectMode::FORK;
    out.name         = newName;
    out.themeRoot    = themeRoot;
    out.templateName = templateName;
    return true;
}

bool openProjectInPlace(const std::string& themeRoot, const std::string& themeName,
    Project& out, std::string& error)
{
    if (isBaseTheme(themeName))
    {
        error = "default / ocean 是基准主题，请用「模板新建」改出一份自己的主题";
        return false;
    }
    if (!directoryExists(themeRoot + "/" + themeName))
    {
        error = "主题目录不存在：" + themeName;
        return false;
    }

    out.mode         = ProjectMode::INPLACE;
    out.name         = themeName;
    out.themeRoot    = themeRoot;
    out.templateName.clear();
    return true;
}

bool saveProjectFile(const Project& project, const std::string& path, std::string& error)
{
    Json doc = Json::object();
    doc["mode"]     = (project.mode == ProjectMode::FORK) ? "fork" : "inplace";
    doc["name"]     = project.name;
    doc["themeRoot"] = project.themeRoot;
    doc["template"] = project.templateName;
    return writeTextFileAtomic(path, doc.dump(2) + "\n", false, error);
}

bool loadProjectFile(const std::string& path, Project& out, std::string& error)
{
    std::string text;
    if (!readTextFile(path, text, error))
        return false;

    const Json doc = Json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object())
    {
        error = "项目记录不是合法 JSON";
        return false;
    }

    out.mode = (doc.value("mode", std::string("fork")) == "inplace") ? ProjectMode::INPLACE
                                                                     : ProjectMode::FORK;
    out.name         = doc.value("name", std::string());
    out.themeRoot    = doc.value("themeRoot", std::string());
    out.templateName = doc.value("template", std::string());
    if (out.name.empty() || out.themeRoot.empty())
    {
        error = "项目记录缺少 name / themeRoot";
        return false;
    }
    return true;
}

} // namespace designer

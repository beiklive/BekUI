#include "designer/core/theme_edit.hpp"

#include <algorithm>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "designer/core/theme_files.hpp"

namespace fs = std::filesystem;
using Json   = nlohmann::ordered_json;

namespace designer
{

namespace
{

// theme.json 的 widgets[] 加/减一项；文件其他内容原样保留
bool updateWidgetList(const std::string& themeDir, const std::string& relative, bool add,
    std::string& error)
{
    const std::string themePath = themeDir + "/theme.json";
    std::string text;
    if (!readTextFile(themePath, text, error))
        return false;

    Json doc = Json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object())
    {
        error = "theme.json 不是合法对象";
        return false;
    }

    std::vector<std::string> widgets;
    if (doc.contains("widgets") && doc["widgets"].is_array())
        for (const Json& entry : doc["widgets"])
            if (entry.is_string())
                widgets.push_back(entry.get<std::string>());

    if (add)
    {
        if (std::find(widgets.begin(), widgets.end(), relative) == widgets.end())
            widgets.push_back(relative);
    }
    else
        widgets.erase(std::remove(widgets.begin(), widgets.end(), relative), widgets.end());

    doc["widgets"] = Json::array();
    for (const std::string& entry : widgets)
        doc["widgets"].push_back(entry);

    return writeTextFileAtomic(themePath, doc.dump(2) + "\n", true, error);
}

} // namespace

bool createWidgetAppearance(const std::string& themeDir, const std::string& name,
    const std::string& contentJson, std::string& error)
{
    if (name.empty())
    {
        error = "名字不能为空";
        return false;
    }
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
    {
        error = "名字里不能带路径分隔符";
        return false;
    }

    const std::string relative = widgetPathFromName(name);
    const std::string path     = themeDir + "/" + relative;

    std::error_code code;
    if (fs::exists(path, code))
    {
        // 不覆盖已有外观：这是用户的东西
        error = "已经存在同名的外观文件：" + relative + "（换一个名字，或直接打开它改）";
        return false;
    }

    // 先登记、再落文件：登记失败时不会留下一个没登记的孤儿文件
    if (!updateWidgetList(themeDir, relative, true, error))
        return false;

    if (!writeTextFileAtomic(path, contentJson, false, error))
    {
        std::string rollbackError;
        updateWidgetList(themeDir, relative, false, rollbackError); // 回滚登记
        return false;
    }
    return true;
}

bool deleteWidgetAppearance(const std::string& themeDir, const std::string& name,
    std::string& error)
{
    const std::string relative = widgetPathFromName(name);
    const std::string path     = themeDir + "/" + relative;

    if (!directoryExists(themeDir))
    {
        error = "主题目录不存在：" + themeDir;
        return false;
    }

    if (!updateWidgetList(themeDir, relative, false, error))
        return false;

    std::error_code code;
    if (fs::exists(path, code))
    {
        const std::string graveyard = path + ".deleted";
        fs::remove(graveyard, code); // 覆盖上一次的同名墓碑
        fs::rename(path, graveyard, code);
        if (code)
        {
            error = "改名失败：" + code.message();
            return false;
        }
    }
    return true;
}

std::string fnvHash(const std::string& text)
{
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

bool writeFileChecked(const std::string& path, const std::string& expectedHash,
    const std::string& text, bool force, bool backup, std::string& error)
{
    if (!force && !expectedHash.empty())
    {
        std::string onDisk;
        std::string readError;
        if (readTextFile(path, onDisk, readError) && fnvHash(onDisk) != expectedHash)
        {
            error = "文件已被外部改动（拒绝覆盖）：" + path;
            return false;
        }
    }
    return writeTextFileAtomic(path, text, backup, error);
}

} // namespace designer

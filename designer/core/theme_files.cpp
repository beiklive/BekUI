#include "designer/core/theme_files.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
#include "bklv/core/logger.hpp"

namespace fs = std::filesystem;
using Json   = nlohmann::ordered_json;

namespace designer
{

bool readThemesFile(const std::string& themeRoot, std::vector<std::string>& available,
    std::string& defaultName, std::string& error)
{
    available.clear();
    defaultName.clear();

    std::string text;
    if (!readTextFile(themeRoot + "/themes.json", text, error))
        return false;

    const Json doc = Json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object())
    {
        error = "themes.json 不是合法对象";
        return false;
    }

    if (doc.contains("default") && doc["default"].is_string())
        defaultName = doc["default"].get<std::string>();

    if (doc.contains("available") && doc["available"].is_array())
    {
        for (const auto& item : doc["available"])
            if (item.is_string())
                available.push_back(item.get<std::string>());
    }
    return true;
}

bool writeThemesFile(const std::string& themeRoot, const std::vector<std::string>& available,
    const std::string& defaultName, std::string& error)
{
    Json doc     = Json::object();
    doc["default"] = defaultName;
    doc["available"] = Json::array();
    for (const std::string& name : available)
        doc["available"].push_back(name);

    return writeTextFileAtomic(themeRoot + "/themes.json", doc.dump(2) + "\n", true, error);
}

bool readWidgetFileList(const std::string& themeDir, std::vector<std::string>& files,
    std::string& error)
{
    files.clear();

    std::string text;
    if (!readTextFile(themeDir + "/theme.json", text, error))
        return false;

    const Json doc = Json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("widgets")
        || !doc["widgets"].is_array())
    {
        error = "theme.json 里没有 widgets 清单";
        return false;
    }

    for (const auto& item : doc["widgets"])
        if (item.is_string())
            files.push_back(item.get<std::string>());
    return true;
}

std::string widgetNameFromPath(const std::string& relativePath)
{
    std::string name = relativePath;
    const size_t slash = name.find_last_of('/');
    if (slash != std::string::npos)
        name = name.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos)
        name = name.substr(0, dot);
    return name;
}

std::string widgetPathFromName(const std::string& widgetName)
{
    return "widgets/" + widgetName + ".json";
}

bool readTextFile(const std::string& path, std::string& out, std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        error = "打不开文件：" + path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

bool writeTextFileAtomic(const std::string& path, const std::string& text, bool backup,
    std::string& error)
{
    try
    {
        const fs::path target(path);
        const fs::path tmp = target.string() + ".tmp";

        if (backup && fs::exists(target))
        {
            const fs::path bak = target.string() + ".bak";
            fs::copy_file(target, bak, fs::copy_options::overwrite_existing);
        }

        {
            std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
            if (!file.is_open())
            {
                error = "写不了临时文件：" + tmp.string();
                return false;
            }
            file << text;
            file.flush();
        }

        fs::rename(tmp, target); // 同目录 rename 是原子替换
        return true;
    }
    catch (const std::exception& e)
    {
        error = std::string("写盘失败：") + e.what();
        return false;
    }
}

bool copyDirectory(const std::string& from, const std::string& to, std::string& error)
{
    try
    {
        if (!fs::exists(from) || !fs::is_directory(from))
        {
            error = "模板目录不存在：" + from;
            return false;
        }
        if (fs::exists(to))
        {
            error = "目标目录已存在：" + to;
            return false;
        }

        fs::create_directories(to);
        fs::copy(from, to,
            fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        return true;
    }
    catch (const std::exception& e)
    {
        error = std::string("复制主题失败：") + e.what();
        return false;
    }
}

bool directoryExists(const std::string& path)
{
    std::error_code code;
    return fs::is_directory(path, code);
}

bool validateJsonText(const std::string& text, std::string& error)
{
    const Json doc = Json::parse(text, nullptr, false);
    if (doc.is_discarded())
    {
        error = "JSON 语法错误（括号/逗号/引号）";
        return false;
    }
    return true;
}

} // namespace designer

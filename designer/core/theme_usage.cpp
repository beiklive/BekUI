#include "designer/core/theme_usage.hpp"

#include <algorithm>
#include <set>

#include "designer/core/theme_files.hpp"

namespace designer
{

namespace
{

bool isKeyChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '.' || c == '_' || c == '-';
}

} // namespace

std::vector<std::string> collectThemeKeysInText(const std::string& text)
{
    static const std::string prefix = "@theme/";

    std::set<std::string> unique;
    size_t pos = 0;
    while ((pos = text.find(prefix, pos)) != std::string::npos)
    {
        size_t end = pos + prefix.size();
        while (end < text.size() && isKeyChar(text[end]))
            end++;

        std::string key = text.substr(pos + prefix.size(), end - pos - prefix.size());
        // 省前缀的写法（@theme/accent.base）归一成点分全名
        if (!key.empty() && key.rfind("color.", 0) != 0 && key.rfind("metric.", 0) != 0)
            key = "color." + key;

        if (!key.empty())
            unique.insert(key);
        pos = end;
    }

    return std::vector<std::string>(unique.begin(), unique.end());
}

std::map<std::string, std::vector<std::string>> collectThemeKeyUsage(const std::string& themeDir,
    std::string& error)
{
    std::map<std::string, std::vector<std::string>> usage;

    std::vector<std::string> widgetFiles;
    if (!readWidgetFileList(themeDir, widgetFiles, error))
        return usage;

    for (const std::string& relative : widgetFiles)
    {
        std::string text;
        std::string readError;
        if (!readTextFile(themeDir + "/" + relative, text, readError))
            continue;

        const std::string widget = widgetNameFromPath(relative);
        for (const std::string& key : collectThemeKeysInText(text))
        {
            std::vector<std::string>& users = usage[key];
            if (std::find(users.begin(), users.end(), widget) == users.end())
                users.push_back(widget);
        }
    }

    for (auto& entry : usage)
        std::sort(entry.second.begin(), entry.second.end());
    return usage;
}

} // namespace designer

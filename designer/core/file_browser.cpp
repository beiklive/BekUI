#include "designer/core/file_browser.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace designer
{

std::string joinPath(const std::string& directory, const std::string& name)
{
    if (directory.empty())
        return name;
    if (directory.back() == '/' || directory.back() == '\\')
        return directory + name;
    return directory + "/" + name;
}

std::string shortenPath(const std::string& path, int keepParts)
{
    if (path.empty() || keepParts <= 0)
        return path;

    // 从后往前数分隔符，数够 keepParts 段就切
    int parts = 0;
    for (size_t index = path.size(); index > 0; index--)
    {
        if (path[index - 1] != '/' && path[index - 1] != '\\')
            continue;
        parts++;
        if (parts >= keepParts)
        {
            // index-1 就是那一段前面的分隔符
            if (index - 1 == 0)
                return path; // 已经是根了，没必要加省略号
            return "…/" + path.substr(index);
        }
    }
    return path; // 段数不够，原样返回
}

std::string parentDirectory(const std::string& path)
{
    std::string trimmed = path;
    while (trimmed.size() > 1 && (trimmed.back() == '/' || trimmed.back() == '\\'))
        trimmed.pop_back();

    const size_t slash = trimmed.find_last_of("/\\");
    if (slash == std::string::npos)
        return trimmed;
    if (slash == 0)
        return trimmed.substr(0, 1); // 根目录
    return trimmed.substr(0, slash);
}

std::string homeDirectory()
{
    if (const char* home = std::getenv("HOME"))
        return home;
    if (const char* profile = std::getenv("USERPROFILE"))
        return profile;
    return "/";
}

std::string findThemeManifest(const std::string& directory)
{
    const std::string candidate = joinPath(directory, "theme.json");
    std::error_code code;
    if (fs::is_regular_file(candidate, code))
        return candidate;
    return std::string();
}

bool listDirectory(const std::string& path, std::vector<DirEntry>& out, std::string& error)
{
    out.clear();

    std::error_code code;
    if (!fs::is_directory(path, code))
    {
        error = "不是一个目录：" + path;
        return false;
    }

    for (const auto& entry : fs::directory_iterator(path, code))
    {
        const std::string name = entry.path().filename().string();
        if (!name.empty() && name[0] == '.')
            continue; // 隐藏项不展示（.git / .DS_Store 之类）

        DirEntry item;
        item.name        = name;
        item.path        = entry.path().string();
        item.isDirectory = entry.is_directory(code);
        if (item.isDirectory)
            item.hasThemeManifest = !findThemeManifest(item.path).empty();
        out.push_back(item);
    }

    // 目录在前，各自按名字排序（不区分大小写）
    std::sort(out.begin(), out.end(), [](const DirEntry& a, const DirEntry& b) {
        if (a.isDirectory != b.isDirectory)
            return a.isDirectory;
        std::string left = a.name, right = b.name;
        std::transform(left.begin(), left.end(), left.begin(), ::tolower);
        std::transform(right.begin(), right.end(), right.begin(), ::tolower);
        return left < right;
    });
    return true;
}

bool listFilesByMtime(const std::string& directory, size_t count,
    std::vector<DirEntry>& out, std::string& error)
{
    out.clear();

    std::error_code code;
    if (!fs::is_directory(directory, code))
    {
        error = "不是一个目录：" + directory;
        return false;
    }

    std::vector<std::pair<fs::file_time_type, DirEntry>> found;
    for (const auto& entry : fs::directory_iterator(directory, code))
    {
        if (!entry.is_regular_file(code))
            continue;
        if (entry.path().extension() != ".json")
            continue;

        DirEntry item;
        item.name        = entry.path().filename().string();
        item.path        = entry.path().string();
        item.isDirectory = false;
        found.emplace_back(fs::last_write_time(entry.path(), code), item);
    }

    // 新的在前；时间相同按名字排，保证结果稳定（测试要用）
    std::sort(found.begin(), found.end(),
        [](const std::pair<fs::file_time_type, DirEntry>& a,
            const std::pair<fs::file_time_type, DirEntry>& b) {
            if (a.first != b.first)
                return a.first > b.first;
            return a.second.name < b.second.name;
        });

    for (size_t i = 0; i < found.size() && i < count; i++)
        out.push_back(found[i].second);
    return true;
}

bool themeInfoFromManifest(const std::string& manifestPath, std::string& themeRoot,
    std::string& themeName, std::string& error)
{
    themeRoot.clear();
    themeName.clear();

    std::string directory = manifestPath;
    // 允许直接传主题目录：用 fs::path 取文件名判断，别手算长度
    // （手算时把 "theme.json" 的 10 个字符写成了 11，结果永远不匹配 —— 测试抓到的）
    if (fs::path(directory).filename() == "theme.json")
    {
        const size_t slash = directory.find_last_of("/\\");
        if (slash == std::string::npos)
        {
            error = "路径里没有目录分隔符：" + manifestPath;
            return false;
        }
        directory = directory.substr(0, slash);
    }

    // 必须真的有 theme.json
    const std::string manifest = joinPath(directory, "theme.json");
    std::error_code code;
    if (!fs::is_regular_file(manifest, code))
    {
        error = "这里没有 theme.json：" + manifest;
        return false;
    }

    const size_t slash = directory.find_last_of("/\\");
    if (slash == std::string::npos)
    {
        error = "取不出主题名：" + directory;
        return false;
    }

    themeName = directory.substr(slash + 1);
    themeRoot = slash == 0 ? directory.substr(0, 1) : directory.substr(0, slash);
    if (themeName.empty() || themeRoot.empty())
    {
        error = "主题名或主题根为空：" + directory;
        return false;
    }
    return true;
}

} // namespace designer

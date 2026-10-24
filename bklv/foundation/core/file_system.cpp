#include "bklv/foundation/core/file_system.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace bklv
{
namespace fsx
{
namespace
{

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

// Switch 上只有 sdmc:/ 与 romfs:/，不读环境变量；不包 #ifndef 会得到
// "defined but not used" 警告（Switch 构建是 -Wall -Wextra 零警告的）。
#ifndef __SWITCH__
std::string envPath(const char* name)
{
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
}
#endif

} // namespace

std::vector<Root> roots()
{
    std::vector<Root> out;
#ifdef __SWITCH__
    out.push_back({ "SD 卡", "sdmc:/" });
    out.push_back({ "内置资源", "romfs:/" });
#else
    const std::string home = envPath("HOME").empty() ? envPath("USERPROFILE") : envPath("HOME");
    if (!home.empty())
        out.push_back({ "主目录", home });

    std::error_code ec;
    const std::string cwd = std::filesystem::current_path(ec).generic_string();
    if (!ec && !cwd.empty() && cwd != home)
        out.push_back({ "当前目录", cwd });
#endif
    return out;
}

bool list(const std::string& directory, std::vector<Entry>& out, std::string& error)
{
    out.clear();
    error.clear();

    std::error_code ec;
    std::filesystem::directory_iterator it(directory, ec);
    if (ec)
    {
        error = "打不开目录：" + directory + "（" + ec.message() + "）";
        return false;
    }

    for (const auto& item : it)
    {
        const std::string name = item.path().filename().generic_string();
        if (name.empty() || name == "." || name == "..")
            continue;

        Entry entry;
        entry.name = name;
        entry.path = item.path().generic_string();

        std::error_code typeEc;
        entry.directory = item.is_directory(typeEc);
        if (!entry.directory)
        {
            std::error_code sizeEc;
            const auto size = item.file_size(sizeEc);
            if (!sizeEc)
                entry.size = size;
        }
        out.push_back(entry);
    }

    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
        if (a.directory != b.directory)
            return a.directory;
        return lower(a.name) < lower(b.name);
    });
    return true;
}

std::string parent(const std::string& directory)
{
    if (directory.empty())
        return std::string();

    std::error_code ec;
    const std::filesystem::path path(directory);
    const std::filesystem::path up = path.parent_path();
    if (up.empty() || up == path)
        return std::string();
    return up.generic_string();
}

std::string join(const std::string& directory, const std::string& name)
{
    return (std::filesystem::path(directory) / name).generic_string();
}

} // namespace fsx
} // namespace bklv

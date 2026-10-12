#include "designer/core/file_kind.hpp"

namespace designer
{

namespace
{

bool startsWith(const std::string& text, const std::string& prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::string withoutTrailingSlash(std::string path)
{
    while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        path.pop_back();
    return path;
}

} // namespace

FileKind classifyThemeFile(const std::string& path, const std::string& themeDir,
    const std::string& resourcesDir)
{
    const std::string theme = withoutTrailingSlash(themeDir) + "/";
    const std::string res   = withoutTrailingSlash(resourcesDir) + "/";

    if (startsWith(path, res + "views/"))
        return FileKind::VIEW;
    if (path == theme + "colors.json" || path == theme + "metrics.json")
        return FileKind::TABLE;
    if (startsWith(path, theme + "widgets/"))
        return FileKind::APPEARANCE;
    return FileKind::UNKNOWN;
}

const char* fileKindName(FileKind kind)
{
    switch (kind)
    {
        case FileKind::APPEARANCE: return "外观";
        case FileKind::VIEW: return "页面视图";
        case FileKind::TABLE: return "主题表";
        default: return "未知";
    }
}

} // namespace designer

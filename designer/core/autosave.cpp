#include "designer/core/autosave.hpp"

#include <ctime>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace designer
{

namespace
{
using Json = nlohmann::ordered_json;
}

std::string autosavePath(const std::string& themeDir)
{
    std::string base = themeDir;
    while (!base.empty() && base.back() == '/')
        base.pop_back();
    return base + "/.designer-autosave.json";
}

bool writeAutosave(const std::string& themeDir, const std::string& file, const std::string& text,
    std::string& error)
{
    Json document;
    document["file"]    = file;
    document["text"]    = text;
    document["savedAt"] = (long long)std::time(nullptr);

    std::ofstream out(autosavePath(themeDir), std::ios::binary | std::ios::trunc);
    if (!out)
    {
        error = "写不了自动保存草稿：" + autosavePath(themeDir);
        return false;
    }
    out << document.dump(2) << "\n";
    if (!out)
    {
        error = "写入自动保存草稿失败";
        return false;
    }
    return true;
}

bool readAutosave(const std::string& themeDir, AutosaveInfo& out, std::string& error)
{
    std::ifstream in(autosavePath(themeDir), std::ios::binary);
    if (!in)
        return false; // 没有草稿

    std::ostringstream buffer;
    buffer << in.rdbuf();
    try
    {
        const Json document = Json::parse(buffer.str());
        out.file            = document.value("file", std::string());
        out.text            = document.value("text", std::string());
        out.savedAt         = document.value("savedAt", (long long)0);
    }
    catch (const std::exception& e)
    {
        error = std::string("自动保存草稿损坏：") + e.what();
        return false;
    }
    if (out.file.empty() || out.text.empty())
    {
        error = "自动保存草稿缺字段";
        return false;
    }
    return true;
}

bool clearAutosave(const std::string& themeDir)
{
    return std::remove(autosavePath(themeDir).c_str()) == 0;
}

} // namespace designer

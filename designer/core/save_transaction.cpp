#include "designer/core/save_transaction.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace designer
{

long long fileMtime(const std::string& path)
{
    struct stat info;
    if (::stat(path.c_str(), &info) != 0)
        return 0;
    return (long long)info.st_mtime;
}

namespace
{

bool readWhole(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    out = buffer.str();
    return true;
}

} // namespace

SaveResult saveText(const std::string& path, const std::string& text, const SaveOptions& options,
    std::string& error)
{
    // ① 外部修改检查（有期望 mtime 且允许检查时）
    if (options.expectedMtime != 0 && !options.force)
    {
        const long long current = fileMtime(path);
        if (current != 0 && current != options.expectedMtime)
        {
            error = "文件在编辑期间被外部修改过（重新加载 / 强制覆盖）";
            return SaveResult::EXTERNALLY_MODIFIED;
        }
    }

    // ② 校验：不合法就什么都不写（连临时文件都不留）
    if (options.validate)
    {
        std::string validationError;
        if (!options.validate(text, validationError))
        {
            error = validationError;
            return SaveResult::VALIDATION_FAILED;
        }
    }

    // ③ 临时文件 -> .bak -> rename：中途失败时原文件仍是完整的旧内容
    const std::string tempPath = path + ".tmp";
    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            error = "写不了临时文件：" + tempPath;
            return SaveResult::IO_FAILED;
        }
        out.write(text.data(), (std::streamsize)text.size());
        out.flush();
        if (!out)
        {
            error = "写临时文件失败：" + tempPath;
            std::remove(tempPath.c_str());
            return SaveResult::IO_FAILED;
        }
    }

    if (options.makeBackup)
    {
        std::string previous;
        if (readWhole(path, previous))
        {
            std::ofstream backup(path + ".bak", std::ios::binary | std::ios::trunc);
            if (backup)
            {
                backup.write(previous.data(), (std::streamsize)previous.size());
                backup.flush();
            }
        }
    }

    if (std::rename(tempPath.c_str(), path.c_str()) != 0)
    {
        error = "改名失败：" + tempPath + " -> " + path;
        std::remove(tempPath.c_str());
        return SaveResult::IO_FAILED;
    }

    return SaveResult::OK;
}

} // namespace designer

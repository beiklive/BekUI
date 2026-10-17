#pragma once

// 保存事务：**校验通过才落盘**，落盘先写临时文件再 rename，旧文件留一份 .bak。
// 失败时原文件、.bak 都不动（DESIGN.md §8 P1 验收②）。

#include <functional>
#include <string>

namespace designer
{

enum class SaveResult
{
    OK,
    VALIDATION_FAILED, // 内容不合法：不落盘
    IO_FAILED,         // 写临时文件/改名失败：原文件保持
    EXTERNALLY_MODIFIED, // 文件在编辑期间被外部改过，调用方要确认覆盖
};

struct SaveOptions
{
    // 校验器：返回 false 并填 error 时拒绝保存。空 = 不校验。
    std::function<bool(const std::string& text, std::string& error)> validate;
    bool makeBackup = true;   // 覆盖前把旧内容复制到 <path>.bak
    bool force      = false;  // 覆盖外部修改（跳过 mtime 检查）
    long long expectedMtime = 0; // 0 = 不做外部修改检查
};

// 读文件 mtime（秒）；不存在返回 0
long long fileMtime(const std::string& path);

SaveResult saveText(const std::string& path, const std::string& text, const SaveOptions& options,
    std::string& error);

} // namespace designer

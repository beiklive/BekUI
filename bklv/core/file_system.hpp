#pragma once

// 跨平台文件系统小工具（文件浏览器用）：平台预设根路径 + 目录列举 + 上级路径。
//
// 平台差异只在这里：
//   · Switch：SD 卡（sdmc:/）与内置资源（romfs:/）——libnx 默认挂好两个 devoptab；
//   · 桌面：主目录 + 当前工作目录（Windows 用 USERPROFILE，其余用 HOME）。
// 目录列举统一走 std::filesystem 的 error_code 版本（不抛异常）。

#include <cstdint>
#include <string>
#include <vector>

namespace bklv
{
namespace fsx
{

struct Entry
{
    std::string name;
    std::string path;      // 拼好的完整路径（带平台分隔符）
    bool        directory = false;
    uint64_t    size      = 0; // 文件字节数；目录为 0
};

struct Root
{
    std::string label;
    std::string path;
};

// 当前平台的根路径预设
std::vector<Root> roots();

// 列举目录（"." ".." 已过滤；目录在前、名字不区分大小写排序）
bool list(const std::string& directory, std::vector<Entry>& out, std::string& error);

// 上一级目录；已经是根/空时返回空串
std::string parent(const std::string& directory);

// 拼接路径
std::string join(const std::string& directory, const std::string& name);

} // namespace fsx
} // namespace bklv

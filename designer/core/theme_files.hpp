#pragma once

// 设计器的"文件层"：只碰主题目录里的文件，不碰任何界面。
// 刻意做成无窗口可测的一层（见 tests/designer_core_test.cpp）：
// 主题登记、外观清单、原子的写盘 + 备份、JSON 语法校验都在这里。

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace designer
{

// 读 <themeRoot>/themes.json：{"default": "xxx", "available": ["...", ...]}
// 文件不存在时返回 false 并给 error（调用方决定要不要当成"还没有主题"）
bool readThemesFile(const std::string& themeRoot, std::vector<std::string>& available,
    std::string& defaultName, std::string& error);

// 写回 themes.json（登记新主题就是 available 里加一项）
bool writeThemesFile(const std::string& themeRoot, const std::vector<std::string>& available,
    const std::string& defaultName, std::string& error);

// 读 <themeDir>/theme.json 里的 widgets[]（外观文件的相对路径，如 "widgets/button.json"）
bool readWidgetFileList(const std::string& themeDir, std::vector<std::string>& files,
    std::string& error);

// 外观名（短名，如 "button.primary"）←→ 文件相对路径（"widgets/button.primary.json"）
std::string widgetNameFromPath(const std::string& relativePath);
std::string widgetPathFromName(const std::string& widgetName);

bool readTextFile(const std::string& path, std::string& out, std::string& error);

// 原子写：先写 <path>.tmp 再 rename 覆盖；backup = true 时先复制一份 <path>.bak。
// 只在这一个函数里碰用户的主题文件，出问题也不会留半个文件。
bool writeTextFileAtomic(const std::string& path, const std::string& text, bool backup,
    std::string& error);

// 递归复制目录（模板新建用）；目标已存在时返回 false
bool copyDirectory(const std::string& from, const std::string& to, std::string& error);

// 目录是否存在
bool directoryExists(const std::string& path);

// JSON 语法校验（保序解析，不改变键序）
bool validateJsonText(const std::string& text, std::string& error);

// 文件原来的缩进风格：写回去时沿用它，免得整份文件被重排（写盘=整文件重写，
// 至少要保住缩进习惯；键序由保序 JSON 保住）
char detectIndentChar(const std::string& text);   // 空格 或 制表符
int detectIndentWidth(const std::string& text);   // 1..8，认不出来给 2

std::string serializeJson(const nlohmann::ordered_json& doc, char indentChar, int indentWidth);

} // namespace designer

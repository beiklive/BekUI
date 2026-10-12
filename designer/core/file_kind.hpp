#pragma once

// 判断一份草稿是"哪种文件"：外观 / 页面视图 / 主题表。
//
// 这个判断决定了保存前该用哪种校验（外观要编译、页面要试实例化、主题表只看语法）。
// 以前"全部保存"里是靠"先试实例化、失败再试编译"来猜的 —— 主题表两种都失败，
// 于是**主题表根本存不进**（只留一条警告）。抽出来之后能测。

#include <string>

namespace designer
{

enum class FileKind
{
    APPEARANCE, // <themeDir>/widgets/*.json
    VIEW,       // <resourcesDir>/views/*.json
    TABLE,      // <themeDir>/colors.json 或 metrics.json
    UNKNOWN,    // 既不是主题文件也不是页面视图 → 保存时拒绝
};

FileKind classifyThemeFile(const std::string& path, const std::string& themeDir,
    const std::string& resourcesDir);

const char* fileKindName(FileKind kind);

} // namespace designer

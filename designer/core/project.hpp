#pragma once

// 项目模型：设计器"在编辑什么"。
//
// 两种模式（新建项目时二选一）：
//   FORK    用模板新建一个主题（复制整个主题目录 + 在 themes.json 里登记），原主题不动
//   INPLACE 直接编辑现有主题（default / ocean 这类基准主题被拒绝，必须走 FORK）
//
// 项目记录里存**主题目录的绝对路径**，所以主题在别的仓库（用户自己的项目）也能编辑。

#include <string>
#include <vector>

namespace designer
{

enum class ProjectMode
{
    FORK = 0,
    INPLACE = 1,
};

struct Project
{
    ProjectMode mode = ProjectMode::FORK;
    std::string name;        // 项目名 = 主题名
    std::string themeRoot;   // 主题根目录：<...>/resources/theme
    std::string templateName; // FORK 时记住模板名（INPLACE 为空）

    // <themeRoot>/<name>
    std::string themeDir() const { return this->themeRoot + "/" + this->name; }
    // 主题所在仓库的 resources 目录（相对路径解析要用）
    std::string resourcesDir() const;
};

// 基准主题不允许就地改（改坏了所有控件陪葬）
bool isBaseTheme(const std::string& themeName);

// 模板新建：复制 <themeRoot>/<templateName> → <themeRoot>/<newName>，
// 并把 <newName> 加进 themes.json 的 available
bool createProjectFromTemplate(const std::string& themeRoot, const std::string& templateName,
    const std::string& newName, Project& out, std::string& error);

// 直接编辑已有主题
bool openProjectInPlace(const std::string& themeRoot, const std::string& themeName,
    Project& out, std::string& error);

// 项目记录（设计器自己的小文件，不进主题目录）
bool saveProjectFile(const Project& project, const std::string& path, std::string& error);
bool loadProjectFile(const std::string& path, Project& out, std::string& error);

} // namespace designer

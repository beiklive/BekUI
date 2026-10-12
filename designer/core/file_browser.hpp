#pragma once

// 打开项目要用的文件浏览：列目录、找 theme.json、从 theme.json 反推主题目录与主题名。
//
// 放在 core 是因为"选错目录 / theme.json 里没有 widgets"这类判断要能测；
// UI 只负责把结果画出来。

#include <string>
#include <vector>

namespace designer
{

struct DirEntry
{
    std::string name; // 只有名字
    std::string path; // 完整路径
    bool isDirectory = false;
    bool hasThemeManifest = false; // 目录里有 theme.json
};

// 列目录：目录在前、各自按名字排序；跳过隐藏项（以 . 开头）
bool listDirectory(const std::string& path, std::vector<DirEntry>& out, std::string& error);

// 从 `<themeRoot>/<themeName>/theme.json` 推出 themeRoot 与 themeName。
// 传入的是主题目录也认（会自动补 /theme.json）。
bool themeInfoFromManifest(const std::string& manifestPath, std::string& themeRoot,
    std::string& themeName, std::string& error);

// 目录本身是不是一个主题（里面有 theme.json）→ 返回那个 theme.json 的路径，否则空串
std::string findThemeManifest(const std::string& directory);

// 父目录（已经是根时返回自己）
std::string parentDirectory(const std::string& path);

// 用户的 home 目录（浏览器的起点）
std::string homeDirectory();

// 按修改时间从新到旧列出目录里的文件（只挑 .json），最多 count 个。
// 用来做"最近修改"快捷入口：改样式时反复打开的那几个文件不必每次翻列表。
bool listFilesByMtime(const std::string& directory, size_t count,
    std::vector<DirEntry>& out, std::string& error);

// 把路径拼成 `a/b`（去掉多余的斜杠）
std::string joinPath(const std::string& directory, const std::string& name);

} // namespace designer

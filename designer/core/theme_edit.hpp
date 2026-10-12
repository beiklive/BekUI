#pragma once

// 主题文件的"结构性编辑"：新建 / 删除外观文件 + 同步 theme.json 的 widgets 清单。
//
// 放在 core 层（不碰界面）是有意的：这几步会改用户的文件，必须能无窗口测。
// 之前它们写在 UI 文件里，等于测不到 —— 审查时抓到的。

#include <string>

namespace designer
{

// 新建 <themeDir>/widgets/<name>.json。名字重复会被拒绝（不覆盖已有文件）。
// contentJson 由调用方给（UI 那边生成模板并先跑过编译校验）。
bool createWidgetAppearance(const std::string& themeDir, const std::string& name,
    const std::string& contentJson, std::string& error);

// 删除外观：从 theme.json 的 widgets[] 里摘掉，并把文件**改名**成 <file>.deleted
// （不是直接删——设计器里手一抖就没了，改名还能捞回来）。
bool deleteWidgetAppearance(const std::string& themeDir, const std::string& name,
    std::string& error);

// 新建页面视图 <viewsDir>/<name>.json。重名拒绝；views 不扫目录，写出来就能用。
// template: empty / page（页头+内容+底栏）/ card
bool createViewFile(const std::string& viewsDir, const std::string& name,
    const std::string& templateName, std::string& error);

// 文件内容指纹（FNV-1a）：用来判断"磁盘上的文件在我打开之后被改过没有"
std::string fnvHash(const std::string& text);

// 带冲突检测的原子写：磁盘内容与 expectedHash 不一致时拒绝写（除非 force）
bool writeFileChecked(const std::string& path, const std::string& expectedHash,
    const std::string& text, bool force, bool backup, std::string& error);

} // namespace designer

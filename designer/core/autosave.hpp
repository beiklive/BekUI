#pragma once

// 自动保存草稿（DESIGN.md §4.8 / §10）：每份主题目录一个草稿文件，里面记着
// "哪个文件 + 内容 + 时间"。设计器异常退出后，下次打开能恢复未保存的改动。
//
// 草稿写在主题目录下的 `.designer-autosave.json`（不写 dist/，符合目录边界）。

#include <string>

namespace designer
{

struct AutosaveInfo
{
    std::string file;     // 相对主题根：widget/button.text
    std::string text;     // 当时的 JSON 原文
    long long   savedAt = 0; // 秒（time(nullptr)）
};

// 草稿路径：**用户可写目录**（不是主题目录 —— 从 dist 里跑时那里可能只读，
// 直接写主题目录会天天弹"写不了自动保存草稿"，实测踩到）。
// Linux/macOS: $HOME/.bklv-designer/autosave.json；Windows: %APPDATA%\bklv-designer\autosave.json
std::string autosavePath(const std::string& themeDir);

bool writeAutosave(const std::string& themeDir, const std::string& file, const std::string& text,
    std::string& error);

// 没有草稿返回 false 且 error 为空（区分"没有"与"读失败"）
bool readAutosave(const std::string& themeDir, AutosaveInfo& out, std::string& error);

bool clearAutosave(const std::string& themeDir);

} // namespace designer

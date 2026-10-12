#pragma once

// 主题 key 的反向引用：`color.accent.base` 被哪些外观文件用到。
//
// 改一个颜色之前得知道它影响谁 —— 这是主题表最该有的一条信息。
// 纯扫描 + 纯函数，无窗口可测。

#include <map>
#include <string>
#include <vector>

namespace designer
{

// key 用**点分全名**（"color.accent.base" / "metric.radius.md"，和主题表里的键一致）。
// 外观里写 `@theme/accent.base`（省了 color.）也会归一到 color.accent.base。
std::map<std::string, std::vector<std::string>> collectThemeKeyUsage(const std::string& themeDir,
    std::string& error);

// 从一段外观 JSON 文本里抽出所有 @theme 引用（归一后的点分 key，去重）
std::vector<std::string> collectThemeKeysInText(const std::string& text);

} // namespace designer

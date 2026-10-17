#pragma once

// 中文优先命名：把主题里的技术名翻成新手能看懂的说法。
// 设计原则：**按段翻译再拼**（`button.bordered` → 按钮 · 带边框），不维护 99 条长表；
// 未收录的段原样保留（不隐藏技术名，专业用户仍能对上文件）。

#include <string>

namespace designer
{

// 单个文件名段：box → 外框；未收录返回空（调用方回退原样）
std::string segmentName(const std::string& segment);

// 文件条目的中文标题：`widget/button.bordered` → 「按钮 · 带边框」
std::string fileTitle(const std::string& relPath);

// 一句话说明（可能为空）
std::string fileHint(const std::string& relPath);

// 层的 op 名：rect → 底色块
std::string opTitle(const std::string& op);

// "层"这个概念的说明（悬停提示用）
const char* layerConceptHint();

} // namespace designer

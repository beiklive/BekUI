#pragma once

// 令牌文件（tokens/colors.json、tokens/metrics.json）的文档模型 + 反向引用扫描。
//
// 文件里是**嵌套对象**（`spacing.xs`），而主题按点分键展平（`spacing.xs`）。这里做这层翻译，
// 并提供"哪些外观引用了这个键"的扫描（DESIGN.md §3 令牌编辑的验收依据）。

#include <string>
#include <vector>

#include "designer/core/json_document.hpp"

namespace designer
{

struct TokenEntry
{
    std::string key;   // 点分键：spacing.xs / accent.base
    std::string value; // 原样值（"#RRGGBBAA" / "12"），保留写法不转 float
};

struct TokenRef
{
    std::string file;    // 相对主题根：widget/button.text
    std::string pointer; // /layers/0/fill
    std::string raw;     // 原文：@theme/color.accent.base
};

class TokensDoc : public JsonDocument
{
  public:
    // 展平成点分键（只走对象；数组不展开）
    std::vector<TokenEntry> list() const;
    const TokenEntry* find(const std::string& key) const;
    bool write(const std::string& key, const std::string& value, std::string& error);
    bool removeKey(const std::string& key, std::string& error);
};

// 扫描主题目录下 base/widget 的外观 JSON，找出所有引用 `@theme/color.<key>` /
// `@theme/metric.<key>` 的位置。key 传点分名（不带前缀）。
std::vector<TokenRef> scanThemeReferences(
    const std::string& themeDir, const std::string& kind, const std::string& key);

} // namespace designer

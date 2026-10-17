// P6 验收（第一半）：令牌文件展平/读写 + 反向引用扫描（用真实主题目录）。

#include <cstdio>
#include <string>

#include "designer/core/tokens_doc.hpp"
#include "test_util.hpp"

using namespace designer;

int main()
{
    std::printf("== 1. 展平：嵌套对象 -> 点分键，保留原样写法 ==\n");
    TokensDoc colors;
    std::string error;
    check(colors.loadFile("resources/theme/default/tokens/colors.json", error),
        "装载 colors.json（" + error + "）");
    const std::vector<TokenEntry> entries = colors.list();
    check(entries.size() > 50, "token 数量合理（" + std::to_string(entries.size()) + "）");

    const TokenEntry* accent = colors.find("accent.base");
    check(accent != nullptr, "能找到 accent.base");
    if (accent != nullptr)
        check(accent->value.rfind("#", 0) == 0, "值是颜色原文：" + accent->value);

    std::printf("\n== 2. 写入/删除走路径补丁，键序与未知键保留 ==\n");
    {
        TokensDoc doc;
        doc.fromText(R"({"a":{"x":"#112233"},"zz":{"keep":true}})", error);
        check(doc.write("a.x", "#445566", error), "改 a.x（" + error + "）");
        const TokenEntry* updated = doc.find("a.x");
        check(updated != nullptr && updated->value == "#445566", "读回新值");
        check(doc.get("/zz/keep") != nullptr, "未知键保留");
        check(doc.removeKey("a.x", error), "删 a.x");
        check(doc.find("a.x") == nullptr, "删后不存在");
    }

    std::printf("\n== 3. 反向引用：真实主题里 accent.base 至少被一处引用 ==\n");
    {
        const std::vector<TokenRef> refs
            = scanThemeReferences("resources/theme/default", "color", "accent.base");
        check(!refs.empty(), "找到引用（" + std::to_string(refs.size()) + " 处）");
        bool hasFile = false;
        for (const TokenRef& ref : refs)
            hasFile = hasFile || ref.file.rfind("widget/", 0) == 0 || ref.file.rfind("base/", 0) == 0;
        check(hasFile, "引用里带相对路径");
        check(scanThemeReferences("resources/theme/default", "color", "__no_such_key__").empty(),
            "不存在的键 -> 空");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

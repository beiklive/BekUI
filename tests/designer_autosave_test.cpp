// 自动保存草稿：写/读/清 + 不丢内容（中文与转义都要原样回来）。

#include <cstdio>
#include <string>

#include "designer/core/autosave.hpp"
#include "test_util.hpp"

using namespace designer;

int main()
{
    const std::string dir = "dist/designer_autosave_work";
    std::system(("mkdir -p " + dir).c_str());
    clearAutosave(dir);

    AutosaveInfo info;
    std::string  error;
    check(!readAutosave(dir, info, error), "没有草稿时返回 false");
    check(error.empty(), "没有草稿不算错误");

    const std::string text = "{\n  \"layers\": [ { \"id\": \"bg\", \"text\": \"设置 \\u00e9\" } ]\n}\n";
    check(writeAutosave(dir, "widget/button.text", text, error), "写入草稿（" + error + "）");

    check(readAutosave(dir, info, error), "读回草稿（" + error + "）");
    checkEqual(info.file, std::string("widget/button.text"), "文件名一致");
    checkEqual(info.text, text, "内容逐字节一致");
    check(info.savedAt > 0, "带时间戳");

    // 覆盖写
    check(writeAutosave(dir, "base/box", "{\"v\":2}", error), "覆盖写");
    check(readAutosave(dir, info, error) && info.text == "{\"v\":2}", "读到最新内容");

    check(clearAutosave(dir), "清除草稿");
    check(!readAutosave(dir, info, error), "清除后没有草稿");

    // 损坏的草稿要报错而不是崩
    FILE* f = std::fopen(autosavePath(dir).c_str(), "wb");
    std::fputs("{ broken", f);
    std::fclose(f);
    check(!readAutosave(dir, info, error), "损坏草稿返回 false");
    check(!error.empty(), "损坏草稿有错误信息");
    clearAutosave(dir);

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

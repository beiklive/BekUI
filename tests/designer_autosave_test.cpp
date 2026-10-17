// 自动保存草稿：写/读/清 + 不丢内容（中文与转义都要原样回来）。

#include <cstdio>
#include <cstdlib>
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

    std::printf("\n== 2. 用户目录：写资源里的主题也不会失败（实测 bug）==\n");
    {
        // 主题目录在 dist/resources 下通常是只读的 —— 草稿必须落到用户目录
        const std::string path = autosavePath("resources/theme/default");
        const char* home = std::getenv("HOME");
        const char* appdata = std::getenv("APPDATA");
        const std::string expectedRoot = appdata != nullptr ? std::string(appdata)
                                                            : std::string(home != nullptr ? home : "");
        check(path.find("resources") == std::string::npos, "草稿路径不在主题目录里：" + path);
        if (!expectedRoot.empty())
            check(path.rfind(expectedRoot, 0) == 0, "草稿落在用户目录下：" + path);
        // 目录会被自动创建，写一次应成功
        std::string error;
        check(writeAutosave("resources/theme/default", "widget/button.text", "{}", error),
            "写资源主题的草稿能成功（" + error + "）");
        check(clearAutosave("resources/theme/default"), "清掉");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

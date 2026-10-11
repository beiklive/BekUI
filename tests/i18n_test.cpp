// 无窗口测试：文案系统（i18n）。
//
// 为什么不用图形界面测：这些断言全是数据层面的（装载、查找、回退、格式化、
// 缓存），开窗口只会让失败更难定位。图形相关的部分仍靠 demo 手测 + 截图。
//
// 运行前要求：可执行文件与 resources/ 同目录（构建已把 resources/ 复制到 dist/）。
// 语言用 BKLV_LOCALE 环境变量控制；测试自己会重新设成需要的值。

#include <cstdio>
#include <string>

#include "bklv/core/i18n.hpp"

static int g_failed = 0;
static int g_checks = 0;

static void check(bool ok, const std::string& what)
{
    g_checks++;
    if (!ok)
    {
        g_failed++;
        std::printf("  [失败] %s\n", what.c_str());
    }
    else
    {
        std::printf("  [通过] %s\n", what.c_str());
    }
}

static void checkEqual(const std::string& got, const std::string& want, const std::string& what)
{
    g_checks++;
    if (got != want)
    {
        g_failed++;
        std::printf("  [失败] %s\n         期望 \"%s\"\n         实际 \"%s\"\n", what.c_str(), want.c_str(),
            got.c_str());
    }
    else
    {
        std::printf("  [通过] %s  =>  \"%s\"\n", what.c_str(), got.c_str());
    }
}

int main()
{
    using namespace bklv;

    std::printf("\n== 0. initI18n 可独立调用（不崩，装载到某种语言）==\n");
    initI18n();
    check(!getLocale().empty(), "initI18n() 之后 getLocale() 非空");
    check(getI18nKeyCount() > 0, "initI18n() 之后有文案（跟随系统语言）");

    std::printf("\n== 1. canonicalLocale 规范化 ==\n");
    checkEqual(canonicalLocale("zh-CN"), "zh-Hans", "zh-CN -> zh-Hans（桌面 SDL 的格式）");
    checkEqual(canonicalLocale("zh-Hans"), "zh-Hans", "zh-Hans 保持");
    checkEqual(canonicalLocale("zh-TW"), "zh-Hant", "zh-TW -> zh-Hant");
    checkEqual(canonicalLocale("zh-HK"), "zh-Hant", "zh-HK -> zh-Hant");
    checkEqual(canonicalLocale("en-GB"), "en-GB", "en-GB 保持");
    checkEqual(canonicalLocale("ja"), "ja-JP", "ja -> ja-JP");
    checkEqual(canonicalLocale("ko-KR"), "ko-KR", "ko-KR 保持");
    checkEqual(canonicalLocale(""), "en-US", "空串 -> 默认 en-US");

    std::printf("\n== 2. 装载中文并取文案 ==\n");
    // 不依赖 initI18n：直接 setLocale 也要能工作（回退表必须懒加载）
    check(setLocale("zh-Hans"), "setLocale(zh-Hans) 成功");
    checkEqual(getLocale(), "zh-Hans", "getLocale()");
    checkEqual(getStr("demo/welcome"), "欢迎使用 bklv", "嵌套 key 查找");
    check(getI18nKeyCount() > 0, "装载到了若干 key");

    std::printf("\n== 3. 缺 key 的行为 ==\n");
    checkEqual(getStr("demo/__nope__"), "demo/__nope__", "缺 key 返回 key 本身");

    std::printf("\n== 4. 占位符格式化 ==\n");
    checkEqual(getStr("demo/list_count", 12), "共 12 项", "整数参数");
    checkEqual(getStr("demo/lang_current", "zh-Hans"), "当前语言：zh-Hans", "字符串参数");
    checkEqual(getStr("demo/list_count", 3.14f), "共 3.14 项", "浮点用 %g（不是 3.140000）");
    checkEqual(formatStr("a{}b{}c", {"1", "2"}), "a1b2c", "多个占位符按顺序");
    checkEqual(formatStr("a{}b", {}), "ab", "参数不足留空");
    checkEqual(formatStr("a{}b", {"1", "2"}), "a1b", "多余参数忽略");

    std::printf("\n== 5. 查找缓存（同一 key 重复取不应增长缓存）==\n");
    const int before = getI18nCacheSize();
    for (int i = 0; i < 100; i++)
        getStr("demo/welcome");
    check(getI18nCacheSize() == before, "重复取同一 key，缓存条目数不变");
    const int missBefore = getI18nCacheSize();
    for (int i = 0; i < 100; i++)
        getStr("demo/__nope__");
    check(getI18nCacheSize() == missBefore, "重复取缺失 key 也不增长（负缓存生效）");

    std::printf("\n== 6. 切语言 ==\n");
    check(setLocale("en-US"), "setLocale(en-US) 成功");
    checkEqual(getStr("demo/welcome"), "Welcome to bklv", "切换到英文后文案变化");
    checkEqual(getStr("demo/list_count", 12), "12 items", "格式化也走新语言");

    std::printf("\n== 7. 语言缺失时回退默认语言 ==\n");
    check(!setLocale("fr-FR"), "setLocale(fr-FR) 返回失败");
    checkEqual(getLocale(), getDefaultLocale(), "语言回退到默认语言");
    checkEqual(getStr("demo/welcome"), "Welcome to bklv", "文案来自默认语言表");

    std::printf("\n== 8. 空 key ==\n");
    checkEqual(getStr(""), "", "空 key 返回空串");

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

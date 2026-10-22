#pragma once

// 测试通用脚手架：计数、通过/失败打印、几种断言。
//
// 以前 8 个测试各自抄了一份（连输出格式都不太一样），集中到这里：
// 加新断言只改一处，输出也统一。测试只在 tests/ 里用，纯头文件、无窗口。

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "bklv/core/geometry.hpp"

inline int g_checks = 0;
inline int g_failed = 0;

inline void check(bool ok, const std::string& what)
{
    g_checks++;
    if (ok)
        std::printf("  [通过] %s\n", what.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s\n", what.c_str());
    }
}

inline void checkEqual(const std::string& got, const std::string& want, const std::string& what)
{
    g_checks++;
    if (got == want)
        std::printf("  [通过] %s  =>  \"%s\"\n", what.c_str(), got.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s\n         期望 \"%s\"\n         实际 \"%s\"\n",
            what.c_str(), want.c_str(), got.c_str());
    }
}

inline void checkEqualInt(int got, int want, const std::string& what)
{
    g_checks++;
    if (got == want)
        std::printf("  [通过] %s  =>  %d\n", what.c_str(), got);
    else
    {
        g_failed++;
        std::printf("  [失败] %s  期望 %d 实际 %d\n", what.c_str(), want, got);
    }
}

inline void checkEqualSize(std::size_t got, std::size_t want, const std::string& what)
{
    g_checks++;
    if (got == want)
        std::printf("  [通过] %s  =>  %zu\n", what.c_str(), got);
    else
    {
        g_failed++;
        std::printf("  [失败] %s  期望 %zu 实际 %zu\n", what.c_str(), want, got);
    }
}

inline void checkFloat(float got, float want, const std::string& what)
{
    g_checks++;
    if (got > want - 0.01f && got < want + 0.01f)
        std::printf("  [通过] %s  =>  %g\n", what.c_str(), got);
    else
    {
        g_failed++;
        std::printf("  [失败] %s  期望 %g 实际 %g\n", what.c_str(), want, got);
    }
}

inline void checkColor(ImU32 got, ImU32 want, const std::string& what)
{
    g_checks++;
    if (got == want)
        std::printf("  [通过] %s\n", what.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s  期望 0x%08X 实际 0x%08X\n", what.c_str(), want, got);
    }
}

// ---------------------------------------------------------------------------
// 环境变量：POSIX 用 setenv/unsetenv，MSVC 只有 _putenv_s（传空串即删除）。
// 有几个测试要验证"进程环境里的策略覆盖"，平台差异集中在这里。
// ---------------------------------------------------------------------------

inline void setEnv(const char* name, const char* value)
{
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

inline void unsetEnv(const char* name)
{
#if defined(_WIN32)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

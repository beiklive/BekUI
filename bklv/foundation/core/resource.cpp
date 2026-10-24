#include "bklv/foundation/core/resource.hpp"

#include "bklv/foundation/core/logger.hpp"
#include "bklv/foundation/core/services.hpp"

#if defined(BKLV_SWITCH)
#include <switch.h>
#elif !defined(BKLV_NO_SDL)
// SDL_GetBasePath 不要求先 SDL_Init，可以在 App 构造阶段安全调用。
#include <SDL3/SDL.h>
#else
#include <filesystem>
#endif

namespace bklv
{

namespace
{

// 资源根目录，含结尾分隔符；空表示"用工作目录相对路径"。
std::string g_base;

bool isAbsolute(const std::string& path)
{
    if (path.empty())
        return false;

    // 设备路径（romfs:/、sdmc:/、C:/）与 URL（https://）
    if (path.find(":/") != std::string::npos || path.find(":\\") != std::string::npos)
        return true;

    // POSIX 绝对路径
    return path[0] == '/';
}

} // namespace

void initResources()
{
#if defined(BKLV_SWITCH)
    // 资源由 build_romfs + elf2nro --romfs 打进 nro（不要用 --romfsdir，它会
    // 被静默忽略），romfsInit 之后才有 romfs:/ 这个设备。
    const Result rc = romfsInit();
    if (R_SUCCEEDED(rc))
    {
        g_base = "romfs:/";
        Logger::info("romfs 已挂载：romfs:/");
    }
    else
    {
        // 挂不上不影响启动，只是图片之类的资源读不到。
        BKLV_LOG_WARNING("romfsInit 失败（0x%x），资源将无法读取", (unsigned)rc);
    }
#elif !defined(BKLV_NO_SDL)
    // 桌面：资源在可执行文件旁边（CMake 的 POST_BUILD 会把 resources/ 复制过去）。
    const char* base = SDL_GetBasePath();
    if (base != nullptr)
    {
        g_base = base;
        Logger::info(std::string("资源目录：") + g_base);
    }
#else
    // 纯核心构建没有 SDL：用当前工作目录
    std::error_code ec;
    const std::string cwd = std::filesystem::current_path(ec).string();
    if (!ec)
    {
        g_base = cwd + "/";
        Logger::info(std::string("资源目录（无 SDL）：") + g_base);
    }
#endif
}

ResourceProvider* g_provider = nullptr;

void setResourceProvider(ResourceProvider* provider)
{
    g_provider = provider;
}

std::string defaultResourcePath(const std::string& relative)
{
    if (relative.empty() || isAbsolute(relative) || g_base.empty())
        return relative;

    return g_base + relative;
}

std::string resourcePath(const std::string& relative)
{
    if (g_provider != nullptr)
        return g_provider->resolve(relative);
    return defaultResourcePath(relative);
}

} // namespace bklv

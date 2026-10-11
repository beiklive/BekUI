// 平台注册表。
//
// 加平台时改这里：新写一个 Platform 子类，然后在 createPlatform 里加一个分支。
// Switch 走独立构建脚本，用编译期条件切换，不做运行时探测。
// 图形后端不在这里选：由各平台自己的 createBackend 决定（见 platforms/*.cpp）。

#include "bklv/core/platform.hpp"

#include "bklv/core/logger.hpp"

#if defined(BKLV_SWITCH)
#include "bklv/platforms/switch_platform.hpp"
#else
#include "bklv/platforms/sdl3_platform.hpp"
#endif

namespace bklv
{

void enablePlatformLog()
{
#if defined(BKLV_SWITCH)
    // sdmc 由 libnx 的默认 init（fsdevMountSdmc）挂好，不需要额外初始化。
    // 用 "w" 重开：文件里就是本次运行的完整日志。
    if (Logger::setFileOutput("sdmc:/bklv.log"))
        Logger::info("日志文件：sdmc:/bklv.log");
    else
        Logger::warning("无法写入 sdmc:/bklv.log（检查 SD 卡是否可写）");
#endif
}


const char* getPlatformName(PlatformType type)
{
    switch (type)
    {
        case PlatformType::DESKTOP_SDL3:
            return "SDL3";
        case PlatformType::SWITCH:
            return "Switch (libnx)";
    }
    return "unknown";
}

Platform* createPlatform(PlatformType type)
{
    switch (type)
    {
        case PlatformType::DESKTOP_SDL3:
#if defined(BKLV_SWITCH)
            Logger::error("Switch 构建不含 SDL3 平台");
            return nullptr;
#else
            return new PlatformSDL3();
#endif

        case PlatformType::SWITCH:
#if defined(BKLV_SWITCH)
            return new PlatformSwitch();
#else
            // Switch 平台只用 devkitA64 编译（见 scripts/build_switch.sh）。
            Logger::error("当前构建不含 Switch 平台支持，请用 Switch 工具链编译");
            return nullptr;
#endif
    }
    return nullptr;
}

} // namespace bklv

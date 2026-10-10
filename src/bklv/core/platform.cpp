// 平台注册表。
//
// 加平台时改这里：新写一个 Platform 子类，然后在 createPlatform 里加一个分支。
// Switch 走独立构建脚本，用 BOREALIS 式的条件编译切换，不在这里做运行时探测。

#include "bklv/core/platform.hpp"

#include "bklv/core/logger.hpp"
#include "bklv/platforms/sdl3_platform.hpp"

#ifdef __SWITCH__
#include "bklv/platforms/switch_platform.hpp"
#endif

namespace bklv
{

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

Platform* createPlatform(PlatformType type, GraphicsBackendType graphics)
{
    switch (type)
    {
        case PlatformType::DESKTOP_SDL3:
            return new PlatformSDL3(graphics);

        case PlatformType::SWITCH:
#ifdef __SWITCH__
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

#include "bklv/system/platform/switch_platform.hpp"

#if defined(BKLV_SWITCH)

#include <switch.h>

#include <arpa/inet.h> // struct in_addr：判断是不是 nxlink 启动的

#include <cstdio>
#include <memory>

#include "bklv/graphics/render/gles3/gles3_backend.hpp"
#include "bklv/graphics/render/libnx/imgui_impl_libnx.hpp"
#include "bklv/ui/input/gamepad.hpp"
#include "bklv/ui/input/input.hpp"
#include "bklv/foundation/core/logger.hpp"
#include "bklv/system/platform/switch_gamepad.hpp"

namespace bklv
{

// EGL 相关的东西都放在 cpp 里，头文件不需要包含 EGL。
struct PlatformSwitch::Impl
{
    std::unique_ptr<GLES3Backend> backend;
    std::unique_ptr<SwitchGamepadSource> gamepadSource;

    uint32_t width  = 1280;
    uint32_t height = 720;

    // nxlink 的输出连接；未连接时为 -1。
    int nxlinkSocket = -1;
};

PlatformSwitch::PlatformSwitch()
{
    this->impl = new Impl();

    // SD 卡日志已在 App::App() 里开了（那是最早能执行到的用户代码）。
    // 这里只负责 nxlink 的 stdout 重定向。

    // ---- nxlink 的 stdout/stderr 重定向 ----
    //
    // 关键：`nxlink -s` 只是开了服务端在等连接，**重定向要由 NRO 自己发起**。
    // 不调 nxlinkStdio() 的话，printf / BKLV_LOG_* 一个字节都到不了电脑，
    // 现场就是"没有任何日志"。
    //
    // __nxlink_host 由 libnx 在启动时从 argv[1] 解析（nxlink 会把主机 IP 传进来）。
    // 没通过 nxlink 启动时它是 0，此时不必碰 socket 服务，nxlinkStdio() 内部也会
    // 直接提前返回，所以这个判断既省事又安全。
    if (__nxlink_host.s_addr != 0)
    {
        socketInitializeDefault();
        this->impl->nxlinkSocket = nxlinkStdio();
        Logger::info(this->impl->nxlinkSocket >= 0 ? "nxlink: 日志已重定向到电脑"
                                                  : "nxlink: 连接主机失败，日志仍在设备上");
    }
}

void PlatformSwitch::createBackend()
{
    // Switch 上只有 EGL + OpenGL ES 3 这一条路（SDL3 没有 Switch 后端）。
    this->impl->backend = std::make_unique<GLES3Backend>();
    Logger::info(std::string("图形后端：") + this->impl->backend->getName());
}

PlatformSwitch::~PlatformSwitch()
{
    if (this->impl != nullptr)
    {
        // 关掉 nxlink 的 socket（重定向的 stdout 会随之失效，所以放在最后再关）。
        if (this->impl->nxlinkSocket >= 0)
            socketExit();

        // 先摘掉手柄源，避免 InputManager 持有已析构的对象。
        setGamepadSource(nullptr);
        this->impl->gamepadSource.reset();
        this->impl->backend.reset();

        delete this->impl;
        this->impl = nullptr;
    }

    // 息屏抑制是全局状态，退出前恢复。
    if (this->dimmingDisabled)
        appletSetLcdBacklightOffEnabled(true);
}

bool PlatformSwitch::createWindow(const std::string& /*title*/, uint32_t width, uint32_t height,
    float /*x*/, float /*y*/)
{
    // Switch 没有"创建窗口"：默认 NWindow 就是全屏。
    // 调用方传的是桌面窗口尺寸（demo 请求 1280x800），Switch 屏幕是 1280x720，
    // 所以这里**以 NWindow 的实际尺寸为准**，不要把请求值套上去。
    (void)width;
    (void)height;

    NWindow* window = nwindowGetDefault();
    u32 actualWidth = 0, actualHeight = 0;
    nwindowGetDimensions(window, &actualWidth, &actualHeight);
    this->impl->width  = actualWidth;
    this->impl->height = actualHeight;
    nwindowSetDimensions(window, (int)actualWidth, (int)actualHeight);

    // 图形后端：Switch 用 EGL + GLES3（编译期已选好）。
    this->createBackend();
    if (!this->impl->backend->initContext((void*)window))
    {
        Logger::error("Switch: 图形上下文初始化失败");
        return false;
    }

    // ImGui 平台后端（把按键与触摸喂进 ImGuiIO）。
    if (!ImGui_ImplLibnx_Init())
    {
        Logger::error("Switch: ImGui 平台后端初始化失败");
        return false;
    }

    // 手柄输入源。
    this->impl->gamepadSource = std::make_unique<SwitchGamepadSource>();
    setGamepadSource(this->impl->gamepadSource.get());

    // 玩游戏不能被息屏打断。
    this->setScreenDimmingDisabled(true, "bklv 应用运行中");

    Logger::info("Switch 平台初始化完成");
    return true;
}

void PlatformSwitch::restoreWindow()
{
    // 手持/底座切换后分辨率不变，但要把尺寸重新告诉 NWindow。
    if (this->impl->backend != nullptr)
    {
        NWindow* window = nwindowGetDefault();
        nwindowSetDimensions(window, (int)this->impl->width, (int)this->impl->height);
    }
}

void PlatformSwitch::setWindowSize(uint32_t width, uint32_t height)
{
    this->impl->width  = width;
    this->impl->height = height;
    this->restoreWindow();
}

bool PlatformSwitch::mainLoopIteration()
{
    // libnx 标准事件循环：HOME 键退出、applet 挂起/恢复都靠它。
    if (!appletMainLoop())
        return false;

    return true;
}

void PlatformSwitch::shutdownImGuiPlatformBackend()
{
    ImGui_ImplLibnx_Shutdown();
}

RenderBackend* PlatformSwitch::getGraphicsBackend()
{
    return this->impl->backend.get();
}

InputManager* PlatformSwitch::getInputManager()
{
    return &bklv::getInputManager();
}

// ---------------------------------------------------------------------------
// 系统能力
// ---------------------------------------------------------------------------

BatteryStatus PlatformSwitch::getBatteryStatus()
{
    BatteryStatus status;
    status.supported = true;

    if (R_SUCCEEDED(psmInitialize()))
    {
        u32 percentage = 0;
        if (R_SUCCEEDED(psmGetBatteryChargePercentage(&percentage)))
            status.level = (int)percentage;

        bool charging = false;
        if (R_SUCCEEDED(psmIsBatteryChargingEnabled(&charging)))
            status.charging = charging;

        psmExit();
    }

    return status;
}

NetworkStatus PlatformSwitch::getNetworkStatus()
{
    NetworkStatus status;

    // 普通应用用 User 权限就够（Admin 才能读 SSID 之类）。
    if (R_FAILED(nifmInitialize(NifmServiceType_User)))
        return status;

    NifmInternetConnectionType type = NifmInternetConnectionType_WiFi;
    u32 wifiStrength                = 0;
    NifmInternetConnectionStatus connectionStatus = NifmInternetConnectionStatus_ConnectingUnknown1;

    if (R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &wifiStrength, &connectionStatus)))
    {
        status.wireless      = type == NifmInternetConnectionType_WiFi;
        status.ethernet      = type == NifmInternetConnectionType_Ethernet;
        status.wirelessLevel = (int)wifiStrength;

        u32 ip = 0;
        if (R_SUCCEEDED(nifmGetCurrentIpAddress(&ip)))
        {
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", ip & 0xFF, (ip >> 8) & 0xFF,
                (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
            status.ipAddress = buffer;
        }
    }

    nifmExit();
    return status;
}

void PlatformSwitch::setScreenDimmingDisabled(bool disable, const std::string& reason)
{
    // appletSetLcdBacklightOffEnabled(false) 表示"不要自动关背光"。
    appletSetLcdBacklightOffEnabled(!disable);
    this->dimmingDisabled = disable;

    if (disable && !reason.empty())
        Logger::info("已禁止息屏：" + reason);
}

void PlatformSwitch::setBacklightBrightness(float brightness)
{
    // lbl 服务只在部分 AppletMode 下可用，拿不到就静默忽略，不影响其他功能。
    if (R_FAILED(lblInitialize()))
        return;

    lblSetCurrentBrightnessSetting(brightness);
    lblExit();
}

float PlatformSwitch::getBacklightBrightness()
{
    if (R_FAILED(lblInitialize()))
        return 1.0f;

    float value = 1.0f;
    lblGetCurrentBrightnessSetting(&value);
    lblExit();
    return value;
}

bool PlatformSwitch::isDarkTheme()
{
    return appletGetThemeColorType() == AppletThemeColorType_Default;
}

std::string PlatformSwitch::getLocale()
{
    if (R_FAILED(setInitialize()))
        return "en-US";

    u64 languageCode = 0;
    std::string locale = "en-US";

    if (R_SUCCEEDED(setGetSystemLanguage(&languageCode)))
    {
        SetLanguage language = SetLanguage_ENUS;
        if (R_SUCCEEDED(setMakeLanguage(languageCode, &language)))
        {
            switch (language)
            {
                case SetLanguage_JA:
                    locale = "ja-JP";
                    break;
                case SetLanguage_ENUS:
                    locale = "en-US";
                    break;
                case SetLanguage_ENGB:
                    locale = "en-GB";
                    break;
                case SetLanguage_FR:
                    locale = "fr-FR";
                    break;
                case SetLanguage_DE:
                    locale = "de-DE";
                    break;
                case SetLanguage_ES:
                    locale = "es-ES";
                    break;
                case SetLanguage_ZHCN:
                    locale = "zh-Hans";
                    break;
                case SetLanguage_ZHTW:
                    locale = "zh-Hant";
                    break;
                case SetLanguage_KO:
                    locale = "ko-KR";
                    break;
                default:
                    break;
            }
        }
    }

    setExit();
    return locale;
}

void PlatformSwitch::openBrowser(const std::string& url)
{
    // Switch 上要打开网页需要通过 LibraryApplet 或者启动浏览器应用（按标题 ID），
    // 这两种都强依赖具体系统版本与权限，这里先不做，只记一条日志。
    Logger::warning("Switch 暂不支持打开浏览器：" + url);
}

} // namespace bklv

#endif // BKLV_SWITCH

// Switch 的文本输入：弹系统软键盘（swkbd）。
//
// 桌面没有"弹软键盘"这回事（内联键盘输入就够了），所以这份只在 Switch 上编。
// libnx 的 swkbd 是**同步阻塞**接口：swkbdShow() 会一直等到用户输完/取消，
// 期间主循环不跑（画面会停住，这是系统键盘的固有行为，borealis 也一样）。

#include "bklv/ui/input/ime.hpp"

#ifdef BKLV_SWITCH

#include <switch.h>

#include <algorithm>
#include <memory>
#include <string>

#include "bklv/foundation/core/logger.hpp"

namespace bklv
{

namespace
{

class SwitchImeManager : public ImeManager
{
  public:
    bool open(const ImeRequest& request) override
    {
        if (this->isOpen())
            return true;

        SwkbdConfig config;
        if (R_FAILED(swkbdCreate(&config, 0)))
        {
            BKLV_LOG_ERROR("swkbdCreate 失败");
            return false;
        }

        this->active = true;

        // 这份 libnx 没有 NumPad preset，用 SetType 选键盘类型
        swkbdConfigMakePresetDefault(&config);
        swkbdConfigSetType(&config,
            request.numeric ? SwkbdType_NumPad : SwkbdType_QWERTY);

        if (!request.header.empty())
            swkbdConfigSetHeaderText(&config, request.header.c_str());
        if (!request.subText.empty())
            swkbdConfigSetSubText(&config, request.subText.c_str());
        if (!request.initialText.empty())
            swkbdConfigSetInitialText(&config, request.initialText.c_str());

        if (request.maxLength > 0)
            swkbdConfigSetStringLenMax(&config, (u32)request.maxLength);

        // 禁用键位：把本库的 bitmask 翻成 libnx 的
        u32 disabled = 0;
        if (request.disableBits & IME_DISABLE_SPACE)
            disabled |= SwkbdKeyDisableBitmask_Space;
        if (request.disableBits & IME_DISABLE_AT)
            disabled |= SwkbdKeyDisableBitmask_At;
        if (request.disableBits & IME_DISABLE_PERCENT)
            disabled |= SwkbdKeyDisableBitmask_Percent;
        if (request.disableBits & IME_DISABLE_FORWSLASH)
            disabled |= SwkbdKeyDisableBitmask_ForwardSlash;
        if (request.disableBits & IME_DISABLE_BACKSLASH)
            disabled |= SwkbdKeyDisableBitmask_Backslash;
        if (request.disableBits & IME_DISABLE_NUMBERS)
            disabled |= SwkbdKeyDisableBitmask_Numbers;
        if (disabled != 0)
            swkbdConfigSetKeySetDisableBitmask(&config, disabled);

        // 最大长度 + 1 个终止符
        const size_t capacity = (size_t)std::max(1, request.maxLength) + 1;
        std::unique_ptr<char[]> buffer(new char[capacity]());
        const Result rc = swkbdShow(&config, buffer.get(), capacity);
        swkbdClose(&config);

        this->active = false;

        const bool accepted = R_SUCCEEDED(rc);
        if (!accepted)
            BKLV_LOG_INFO("软键盘被取消（rc=0x%08X）", (unsigned int)rc);
        else
            BKLV_LOG_INFO("软键盘输入完成：%zu 字节", std::string(buffer.get()).size());

        if (request.callback)
            request.callback(accepted, accepted ? std::string(buffer.get()) : request.initialText);

        return true;
    }

    bool isOpen() const override { return this->active; }

  private:
    bool active = false;
};

SwitchImeManager g_switchIme;

// 静态初始化就登记：App 还没构造时就能用
struct SwitchImeInstaller
{
    SwitchImeInstaller() { setImeManager(&g_switchIme); }
};
SwitchImeInstaller g_switchImeInstaller;

} // namespace

} // namespace bklv

#endif // BKLV_SWITCH

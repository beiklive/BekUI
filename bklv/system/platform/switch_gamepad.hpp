#pragma once

// Switch 手柄输入源，基于 libnx 的 pad API。
//
// libnx 的按键是位掩码，Switch 上还有左手柄/右手柄分别横竖握的情况；
// padInitializeDefault 会把可用的输入源都算进来。

#include "bklv/ui/input/gamepad.hpp"

#if defined(BKLV_SWITCH)

#include <switch.h>

namespace bklv
{

class SwitchGamepadSource : public GamepadSource
{
  public:
    SwitchGamepadSource();
    ~SwitchGamepadSource() override;

    void update() override;
    bool isDirectionHeld(FocusDirection direction) const override;
    bool isConfirmPressed() const override;
    bool isBackPressed() const override;
    bool isConnected() const override;

  private:
    PadState pad {};

    // 上一帧的按键，用来算按下沿。
    u64 lastButtons = 0;
    bool confirmPressed = false;
    bool backPressed    = false;

    // 触摸屏也当作指针输入用（由平台后端写入 ImGuiIO）。
    bool initialized = false;
};

} // namespace bklv

#endif // BKLV_SWITCH

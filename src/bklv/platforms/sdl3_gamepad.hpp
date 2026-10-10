#pragma once

// SDL3 的手柄输入源。桌面平台（macOS / Linux / Windows）使用。

#include "bklv/core/gamepad.hpp"

#if !defined(BKLV_SWITCH)

#include <SDL3/SDL.h>

namespace bklv
{

class SDL3GamepadSource : public GamepadSource
{
  public:
    SDL3GamepadSource();
    ~SDL3GamepadSource() override;

    void update() override;
    bool isDirectionHeld(FocusDirection direction) const override;
    bool isConfirmPressed() const override;
    bool isBackPressed() const override;
    bool isConnected() const override;

    // 手柄热插拔时调用。
    void refresh();

  private:
    SDL_Gamepad* gamepad = nullptr;

    // 确认/返回用"按下沿"判断，所以要记住上一帧状态。
    bool confirmLast = false;
    bool backLast    = false;
};

} // namespace bklv

#endif // !BKLV_SWITCH

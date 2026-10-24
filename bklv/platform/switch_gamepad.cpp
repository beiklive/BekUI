#include "bklv/platform/switch_gamepad.hpp"

#if defined(BKLV_SWITCH)

#include "bklv/core/logger.hpp"

namespace bklv
{

SwitchGamepadSource::SwitchGamepadSource()
{
    // 标准手柄 + 左右单边 Joy-Con。
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&this->pad);
    this->initialized = true;

    Logger::info("Switch 手柄已初始化");
}

SwitchGamepadSource::~SwitchGamepadSource()
{
    // PadState 不需要显式销毁，这里留个位置方便以后加振动等清理。
}

void SwitchGamepadSource::update()
{
    if (!this->initialized)
        return;

    padUpdate(&this->pad);

    const u64 buttons = padGetButtons(&this->pad);

    // 按下沿：这一帧按下且上一帧没按。
    this->confirmPressed = (buttons & HidNpadButton_A) && !(this->lastButtons & HidNpadButton_A);
    this->backPressed    = (buttons & HidNpadButton_B) && !(this->lastButtons & HidNpadButton_B);

    this->lastButtons = buttons;
}

bool SwitchGamepadSource::isDirectionHeld(FocusDirection direction) const
{
    if (!this->initialized)
        return false;

    const u64 buttons = padGetButtons(&this->pad);

    switch (direction)
    {
        case FocusDirection::UP:
            // 十字键与左摇杆都算
            return (buttons & (HidNpadButton_Up | HidNpadButton_StickLUp)) != 0;
        case FocusDirection::DOWN:
            return (buttons & (HidNpadButton_Down | HidNpadButton_StickLDown)) != 0;
        case FocusDirection::LEFT:
            return (buttons & (HidNpadButton_Left | HidNpadButton_StickLLeft)) != 0;
        case FocusDirection::RIGHT:
            return (buttons & (HidNpadButton_Right | HidNpadButton_StickLRight)) != 0;
        case FocusDirection::NONE:
            return false;
    }
    return false;
}

bool SwitchGamepadSource::isConfirmPressed() const
{
    return this->confirmPressed;
}

bool SwitchGamepadSource::isBackPressed() const
{
    return this->backPressed;
}

bool SwitchGamepadSource::isConnected() const
{
    return this->initialized;
}

} // namespace bklv

#endif // BKLV_SWITCH

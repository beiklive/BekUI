#include "bklv/platforms/sdl3_gamepad.hpp"

#include "bklv/core/logger.hpp"

namespace bklv
{

SDL3GamepadSource::SDL3GamepadSource()
{
    this->refresh();
}

SDL3GamepadSource::~SDL3GamepadSource()
{
    if (this->gamepad != nullptr)
        SDL_CloseGamepad(this->gamepad);
}

void SDL3GamepadSource::refresh()
{
    if (this->gamepad != nullptr)
        return;

    int count            = 0;
    SDL_JoystickID* ids  = SDL_GetGamepads(&count);
    if (ids == nullptr)
        return;

    if (count > 0)
        this->gamepad = SDL_OpenGamepad(ids[0]);

    SDL_free(ids);
}

void SDL3GamepadSource::update()
{
    // 手柄可能是启动后才插上的。
    if (this->gamepad == nullptr)
    {
        this->refresh();
        return;
    }

    this->confirmLast = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    this->backLast    = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_EAST);
}

bool SDL3GamepadSource::isDirectionHeld(FocusDirection direction) const
{
    if (this->gamepad == nullptr)
        return false;

    switch (direction)
    {
        case FocusDirection::UP:
            return SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
        case FocusDirection::DOWN:
            return SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        case FocusDirection::LEFT:
            return SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        case FocusDirection::RIGHT:
            return SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        case FocusDirection::NONE:
            return false;
    }
    return false;
}

bool SDL3GamepadSource::isConfirmPressed() const
{
    if (this->gamepad == nullptr)
        return false;

    const bool now = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    return now && !this->confirmLast;
}

bool SDL3GamepadSource::isBackPressed() const
{
    if (this->gamepad == nullptr)
        return false;

    const bool now = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_EAST);
    return now && !this->backLast;
}

bool SDL3GamepadSource::isConnected() const
{
    return this->gamepad != nullptr;
}

} // namespace bklv

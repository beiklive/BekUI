#include "bklv/input/gamepad.hpp"

namespace bklv
{

namespace
{

// 没有手柄时的空实现：全部返回 false，上层就不显示手柄提示。
class NullGamepadSource : public GamepadSource
{
  public:
    void update() override { }
    bool isDirectionHeld(FocusDirection) const override { return false; }
    bool isConfirmPressed() const override { return false; }
    bool isBackPressed() const override { return false; }
    bool isConnected() const override { return false; }
};

GamepadSource*& currentSource()
{
    static GamepadSource* source = nullptr;
    return source;
}

NullGamepadSource& nullSource()
{
    static NullGamepadSource source;
    return source;
}

} // namespace

GamepadSource& getGamepadSource()
{
    GamepadSource* source = currentSource();
    return source != nullptr ? *source : nullSource();
}

void setGamepadSource(GamepadSource* source)
{
    currentSource() = source;
}

} // namespace bklv

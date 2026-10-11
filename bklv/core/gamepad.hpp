#pragma once

// 手柄输入源。
//
// 为什么单独抽一层：桌面用 SDL3，Switch 用 libnx 的 padUpdate，两套 API 完全不同。
// 抽象掉之后 core/input.cpp 不再依赖任何平台 API，键盘与鼠标继续走 ImGuiIO
// （Switch 上由 ImGui 的 libnx 平台后端把按键喂进 ImGuiIO）。

#include "bklv/core/input.hpp"

namespace bklv
{

class GamepadSource
{
  public:
    virtual ~GamepadSource() = default;

    // 每帧刷新一次按键状态。
    virtual void update() = 0;

    // 方向键与确认/返回。没有手柄时返回 false。
    virtual bool isDirectionHeld(FocusDirection direction) const = 0;
    virtual bool isConfirmPressed() const  = 0;
    virtual bool isBackPressed() const     = 0;

    // 手柄是否接上（用于决定要不要显示按键提示）。
    virtual bool isConnected() const = 0;
};

// 当前平台的手柄输入源。没有手柄支持时返回一个永远为假的实现。
GamepadSource& getGamepadSource();

// 由平台实现注册。传 nullptr 表示回到"无手柄"状态。
void setGamepadSource(GamepadSource* source);

} // namespace bklv

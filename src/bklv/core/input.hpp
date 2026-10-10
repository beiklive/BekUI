#pragma once

// 输入与焦点系统，对应 borealis 的 core/input.hpp + core/application.cpp 中的焦点部分。
//
// 设计要点（与 borealis 一致）：
//   - 焦点是一棵独立的"当前选中路径"，Application 持有 currentFocus，Box 负责在其中导航
//   - 每个 View 自己注册动作（registerAction），底部提示栏统一展示
//   - 键鼠事件直接读 ImGui 后端已经收好的 ImGuiIO；手柄另外读 SDL3，避免依赖 ImGui 的导航

#include <functional>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include "imgui.h"

#include "bklv/core/geometry.hpp"

namespace bklv
{

class View;

// 导航方向
enum class FocusDirection
{
    NONE,
    LEFT,
    RIGHT,
    UP,
    DOWN,
};

// 手柄/键盘抽象出来的按钮
enum class ControllerButton
{
    NONE,
    A, // 确认
    B, // 返回
    X,
    Y,
    LB,
    RB,
    START,
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

const char* getControllerButtonName(ControllerButton button);

using ActionIdentifier = size_t;
using ActionListener   = std::function<void(View*)>;

// 一个已注册的动作
struct InputAction
{
    ActionIdentifier identifier = 0;
    ControllerButton button     = ControllerButton::NONE;
    std::string hintText;
    ActionListener listener;
    bool hidden        = false;
    bool allowRepeating = false;
};

// 提示栏里的一项
struct HintItem
{
    ControllerButton button = ControllerButton::NONE;
    std::string hintText;
};

class InputManager
{
  public:
    // 每帧调用一次：处理导航、鼠标点击、动作派发。
    void update();

    // 手柄热插拔时调用；不接手柄也不影响键鼠。
    void setupGamepad();
    void shutdownGamepad();

    // 焦点管理
    void giveFocus(View* view);
    View* getCurrentFocus() const { return this->currentFocus; }

    // 底部提示栏内容（来自当前焦点视图注册的动作）
    std::vector<HintItem> getHints() const;

    // 鼠标/触摸位置，用于 hitTest
    Point getPointerPosition() const;

    // 某个 View 被销毁时通知输入层：如果它正在焦点路径上，重新选一个焦点。
    void notifyViewDeleted(View* view);

    // 导航树：焦点从 root 开始向下走
    void setRoot(View* root) { this->root = root; }

    View* getRoot() const { return this->root; }

    // 用鼠标点击切换焦点（点击空白不改变焦点）
    void focusFromPointer(View* hit);

    // 是否已经用鼠标点击过；点击后焦点会跟着鼠标走
    bool isPointerActive() const { return this->pointerActive; }

  private:
    void handleNavigation();
    void handlePointer();
    void handleActions();

    // 从 ImGuiIO 里读键盘边沿；keyIndex 用 ImGuiKey_*
    static bool isKeyPressedEdge(int imguiKey, bool* state);

    void navigate(FocusDirection direction);

    View* currentFocus = nullptr;
    View* root         = nullptr;

    bool pointerActive = false;
    Point pointerPosition;

    // 按键"按下沿"的上一帧状态
    bool stateA = false, stateB = false;

    SDL_Gamepad* gamepad = nullptr;
};

// 全局输入管理器
InputManager& getInputManager();

// 绘制底部提示栏（由 App 每帧调用）
void drawHints(ImDrawList* drawList, float screenWidth, float screenHeight);

} // namespace bklv

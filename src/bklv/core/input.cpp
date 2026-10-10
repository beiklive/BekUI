#include "bklv/core/input.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

// ---------------------------------------------------------------------------
// InputManager
// ---------------------------------------------------------------------------

InputManager& getInputManager()
{
    static InputManager manager;
    return manager;
}

const char* getControllerButtonName(ControllerButton button)
{
    switch (button)
    {
        case ControllerButton::A:
            return "A";
        case ControllerButton::B:
            return "B";
        case ControllerButton::X:
            return "X";
        case ControllerButton::Y:
            return "Y";
        case ControllerButton::LB:
            return "LB";
        case ControllerButton::RB:
            return "RB";
        case ControllerButton::START:
            return "START";
        case ControllerButton::UP:
            return "UP";
        case ControllerButton::DOWN:
            return "DOWN";
        case ControllerButton::LEFT:
            return "LEFT";
        case ControllerButton::RIGHT:
            return "RIGHT";
        case ControllerButton::NONE:
            break;
    }
    return "";
}

// 读取某个按键这一帧是否刚刚按下（边沿触发）。
bool InputManager::isKeyPressedEdge(int imguiKey, bool* state)
{
    const bool pressed = ImGui::IsKeyDown(static_cast<ImGuiKey>(imguiKey));
    const bool edge    = pressed && !*state;
    *state             = pressed;
    return edge;
}

void InputManager::update()
{
    // 指针位置每帧刷新。
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    this->pointerPosition = Point(mouse.x, mouse.y);

    this->handleNavigation();
    this->handlePointer();
    this->handleActions();
}

void InputManager::handleNavigation()
{
    bool dirUp = false, dirDown = false, dirLeft = false, dirRight = false;

    // ImGui 的键盘边沿：用轮询差分成"刚按下"。
    static bool kUp = false, kDown = false, kLeft = false, kRight = false;
    dirUp    = isKeyPressedEdge(ImGuiKey_UpArrow, &kUp) || isKeyPressedEdge(ImGuiKey_W, &kUp);
    dirDown  = isKeyPressedEdge(ImGuiKey_DownArrow, &kDown) || isKeyPressedEdge(ImGuiKey_S, &kDown);
    dirLeft  = isKeyPressedEdge(ImGuiKey_LeftArrow, &kLeft) || isKeyPressedEdge(ImGuiKey_A, &kLeft);
    dirRight = isKeyPressedEdge(ImGuiKey_RightArrow, &kRight) || isKeyPressedEdge(ImGuiKey_D, &kRight);

    // 手柄方向键（SDL3）
    if (this->gamepad != nullptr)
    {
        dirUp    = dirUp || SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
        dirDown  = dirDown || SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        dirLeft  = dirLeft || SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        dirRight = dirRight || SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    }

    if (dirUp)
        this->navigate(FocusDirection::UP);
    else if (dirDown)
        this->navigate(FocusDirection::DOWN);
    else if (dirLeft)
        this->navigate(FocusDirection::LEFT);
    else if (dirRight)
        this->navigate(FocusDirection::RIGHT);
}

void InputManager::navigate(FocusDirection direction)
{
    if (this->root == nullptr)
        return;

    // 还没有焦点：先选一个默认焦点。
    if (this->currentFocus == nullptr)
    {
        this->giveFocus(this->root->getDefaultFocus());
        return;
    }

    View* next = this->currentFocus->getNextFocus(direction, this->currentFocus);
    if (next == nullptr)
        next = this->root->getNextFocus(direction, this->currentFocus);

    if (next != nullptr && next != this->currentFocus)
        this->giveFocus(next);
}

void InputManager::giveFocus(View* view)
{
    if (view == this->currentFocus)
        return;

    if (view != nullptr && !view->isFocusable() && view->getDefaultFocus() == nullptr)
        return;

    // 换焦点：先通知旧的焦点路径，再通知新的。
    if (this->currentFocus != nullptr)
    {
        std::vector<View*> stack;
        for (View* v = this->currentFocus; v != nullptr; v = v->getParent())
            stack.push_back(v);
        for (auto it = stack.rbegin(); it != stack.rend(); ++it)
            (*it)->onFocusLost();
    }

    this->currentFocus = view;

    if (this->currentFocus != nullptr)
    {
        std::vector<View*> stack;
        for (View* v = this->currentFocus; v != nullptr; v = v->getParent())
            stack.push_back(v);
        for (auto it = stack.rbegin(); it != stack.rend(); ++it)
            (*it)->onFocusGained();
    }
}

void InputManager::focusFromPointer(View* hit)
{
    if (hit == nullptr || hit == this->currentFocus || !hit->isFocusable())
        return;

    this->giveFocus(hit);
}

void InputManager::notifyViewDeleted(View* view)
{
    if (view == nullptr)
        return;

    // 只处理"被删除的视图是当前焦点的祖先或自己"这一种情况。
    bool inPath = false;
    for (View* v = this->currentFocus; v != nullptr; v = v->getParent())
    {
        if (v == view)
        {
            inPath = true;
            break;
        }
    }

    if (!inPath)
        return;

    // 从被删除节点的下一层往上找第一个还可用的祖先作为新焦点的起点。
    View* candidate = nullptr;
    for (View* v = this->currentFocus; v != nullptr && v != view; v = v->getParent())
        candidate = v;

    this->currentFocus = nullptr;

    if (candidate != nullptr)
        this->giveFocus(candidate->getDefaultFocus());
    else if (this->root != nullptr)
        this->giveFocus(this->root->getDefaultFocus());
}

void InputManager::handlePointer()
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.MouseDown[0])
        return;

    this->pointerActive = true;

    if (this->root == nullptr)
        return;

    if (View* hit = this->root->hitTest(this->pointerPosition))
        this->focusFromPointer(hit);
}

void InputManager::handleActions()
{
    bool confirm = false;
    bool back    = false;

    confirm = confirm || isKeyPressedEdge(ImGuiKey_Enter, &this->stateA);
    confirm = confirm || isKeyPressedEdge(ImGuiKey_Space, &this->stateA);
    back    = back || isKeyPressedEdge(ImGuiKey_Escape, &this->stateB);

    if (this->gamepad != nullptr)
    {
        // 手柄按钮用"按下沿"判断，这里简化为直接读状态并在下一帧比较。
        static bool gpA = false, gpB = false, gpX = false, gpY = false;
        const bool nowA = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
        const bool nowB = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_EAST);
        const bool nowX = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_WEST);
        const bool nowY = SDL_GetGamepadButton(this->gamepad, SDL_GAMEPAD_BUTTON_NORTH);
        confirm = confirm || (nowA && !gpA);
        back    = back || (nowB && !gpB);
        gpA = nowA;
        gpB = nowB;
        gpX = nowX;
        gpY = nowY;
        (void)gpX;
        (void)gpY;
    }

    if (this->currentFocus == nullptr)
        return;

    if (confirm)
        this->currentFocus->triggerAction(ControllerButton::A);
    if (back)
        this->currentFocus->triggerAction(ControllerButton::B);
}

std::vector<HintItem> InputManager::getHints() const
{
    std::vector<HintItem> hints;

    if (this->currentFocus == nullptr)
        return hints;

    for (const InputAction& action : this->currentFocus->getActions())
    {
        if (action.hidden || action.button == ControllerButton::NONE)
            continue;

        hints.push_back({ action.button, action.hintText });
        if (hints.size() >= 6)
            break;
    }

    return hints;
}

Point InputManager::getPointerPosition() const
{
    return this->pointerPosition;
}

// ---------------------------------------------------------------------------
// 手柄
// ---------------------------------------------------------------------------

void InputManager::setupGamepad()
{
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids == nullptr)
        return;

    if (count > 0 && this->gamepad == nullptr)
        this->gamepad = SDL_OpenGamepad(ids[0]);

    SDL_free(ids);
}

void InputManager::shutdownGamepad()
{
    if (this->gamepad != nullptr)
    {
        SDL_CloseGamepad(this->gamepad);
        this->gamepad = nullptr;
    }
}

} // namespace bklv

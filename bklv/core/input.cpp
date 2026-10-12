#include "bklv/core/audio.hpp"
#include "bklv/core/input.hpp"

#include "bklv/core/time.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/gamepad.hpp"
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

// 码位取自 borealis 的 views/hint.cpp（Switch 图标字体）
const char* getControllerButtonIcon(ControllerButton button)
{
    switch (button)
    {
        case ControllerButton::A:     return "\uE0E0";
        case ControllerButton::B:     return "\uE0E1";
        case ControllerButton::X:     return "\uE0E2";
        case ControllerButton::Y:     return "\uE0E3";
        case ControllerButton::LSB:   return "\uE104";
        case ControllerButton::RSB:   return "\uE105";
        case ControllerButton::LT:    return "\uE0E6";
        case ControllerButton::RT:    return "\uE0E7";
        case ControllerButton::LB:    return "\uE0E4";
        case ControllerButton::RB:    return "\uE0E5";
        case ControllerButton::START: return "\uE0EF";
        case ControllerButton::BACK:  return "\uE0F0";
        case ControllerButton::LEFT:  return "\uE0ED";
        case ControllerButton::UP:    return "\uE0EB";
        case ControllerButton::RIGHT: return "\uE0EE";
        case ControllerButton::DOWN:  return "\uE0EC";
        default:                      return "\uE152"; // 未知键
    }
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
        case ControllerButton::LT:
            return "LT";
        case ControllerButton::RT:
            return "RT";
        case ControllerButton::LSB:
            return "LS";
        case ControllerButton::RSB:
            return "RS";
        case ControllerButton::BACK:
            return "BACK";
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
void InputManager::update()
{
    // 先让手柄刷新一次按键状态，后面的"按下沿"判断才准确。
    getGamepadSource().update();

    // 指针位置每帧刷新。
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    this->pointerPosition = Point(mouse.x, mouse.y);

    this->handleNavigation();
    this->handlePointer();
    this->handleActions();
}

bool InputManager::isKeyDownAny(int keyA, int keyB) const
{
    if (keyA >= 0 && ImGui::IsKeyDown(static_cast<ImGuiKey>(keyA)))
        return true;
    if (keyB >= 0 && ImGui::IsKeyDown(static_cast<ImGuiKey>(keyB)))
        return true;
    return false;
}

void InputManager::setRepeatDelay(int milliseconds)
{
    this->repeatDelayMs = milliseconds;
    this->repeatUp.setDelay(milliseconds);
    this->repeatDown.setDelay(milliseconds);
    this->repeatLeft.setDelay(milliseconds);
    this->repeatRight.setDelay(milliseconds);
    this->repeatConfirm.setDelay(milliseconds);
    this->repeatBack.setDelay(milliseconds);
}

void InputManager::setRepeatInterval(int milliseconds)
{
    this->repeatIntervalMs = milliseconds;
    this->repeatUp.setInterval(milliseconds);
    this->repeatDown.setInterval(milliseconds);
    this->repeatLeft.setInterval(milliseconds);
    this->repeatRight.setInterval(milliseconds);
    this->repeatConfirm.setInterval(milliseconds);
    this->repeatBack.setInterval(milliseconds);
}

void InputManager::handleNavigation()
{
    // 时间来自帧间隔累加：键盘、手柄、各平台共用同一套计时，不需要平台 API
    this->clock += (double)getFrameDeltaSeconds();
    const double now = this->clock;

    // 键盘：方向键与 WASD 都算"这个方向被按住"。
    // **不要**给两个键共用一个边沿状态：它们会互相清状态，
    // 结果按住一个键就变成每帧都"刚按下"（导航飙到帧率，踩过）。
    const bool keysUp    = this->isKeyDownAny(ImGuiKey_UpArrow, ImGuiKey_W);
    const bool keysDown  = this->isKeyDownAny(ImGuiKey_DownArrow, ImGuiKey_S);
    const bool keysLeft  = this->isKeyDownAny(ImGuiKey_LeftArrow, ImGuiKey_A);
    const bool keysRight = this->isKeyDownAny(ImGuiKey_RightArrow, ImGuiKey_D);

    // 手柄方向键：由平台提供的 GamepadSource 决定（桌面 SDL3 / Switch libnx）
    GamepadSource& gamepad = getGamepadSource();
    const bool heldUp    = keysUp || gamepad.isDirectionHeld(FocusDirection::UP);
    const bool heldDown  = keysDown || gamepad.isDirectionHeld(FocusDirection::DOWN);
    const bool heldLeft  = keysLeft || gamepad.isDirectionHeld(FocusDirection::LEFT);
    const bool heldRight = keysRight || gamepad.isDirectionHeld(FocusDirection::RIGHT);

    if (this->repeatUp.step(heldUp, now))
        this->navigate(FocusDirection::UP);
    else if (this->repeatDown.step(heldDown, now))
        this->navigate(FocusDirection::DOWN);
    else if (this->repeatLeft.step(heldLeft, now))
        this->navigate(FocusDirection::LEFT);
    else if (this->repeatRight.step(heldRight, now))
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

    // 有模态层时不许把焦点移到模态外面去
    if (this->hasModal() && next != nullptr && !isInsideModal(next))
        next = nullptr;

    if (next != nullptr && next != this->currentFocus)
        this->giveFocus(next);
}

void InputManager::resetFocusState()
{
    this->modals.clear();
    this->currentFocus = nullptr;
    this->gestureTarget = nullptr; // 同上：根视图一换就是野指针
}

bool InputManager::isInsideModal(View* view) const
{
    if (!this->hasModal())
        return true;

    View* modal = this->getTopModal();
    for (View* v = view; v != nullptr; v = v->getParent())
        if (v == modal)
            return true;
    return false;
}

void InputManager::pushModal(View* modal, bool closeOnBack)
{
    if (modal == nullptr)
        return;

    ModalEntry entry;
    entry.modal       = modal;
    entry.savedFocus  = this->currentFocus;
    entry.closeOnBack = closeOnBack;
    this->modals.push_back(entry);

    // 焦点先进模态里的默认焦点（否则第一次方向键会从外面开始算）
    if (View* inner = modal->getDefaultFocus())
        this->giveFocus(inner);
    else if (modal->isFocusable())
        this->giveFocus(modal);
}

void InputManager::popModal()
{
    if (this->modals.empty())
        return;

    ModalEntry entry = this->modals.back();
    this->modals.pop_back();

    // 恢复打开模态之前的焦点；那个视图可能已经不在了（被销毁），所以先确认它还在树上
    View* restore = entry.savedFocus;
    while (restore != nullptr && restore->getParent() == nullptr && restore != this->root)
        restore = nullptr;

    this->giveFocus(restore);
}

View* InputManager::getTopModal() const
{
    return this->modals.empty() ? nullptr : this->modals.back().modal;
}

void InputManager::giveFocus(View* view)
{
    if (view == this->currentFocus)
        return;

    if (view != nullptr && !view->isFocusable() && view->getDefaultFocus() == nullptr)
        return;

    // 焦点反馈音：侧栏条目单独一种（和 borealis 的 SOUND_FOCUS_SIDEBAR 一致）
    if (view != nullptr)
    {
        const bool sidebar = view->getClassString() == "bklv::TabCell";
        audio::play(sidebar ? Sound::FOCUS_SIDEBAR : Sound::FOCUS_CHANGE);
    }

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

    const Point position(io.MousePos.x, io.MousePos.y);
    this->pointerPosition = position;

    const bool down  = io.MouseDown[0];
    const Point delta(io.MouseDelta.x, io.MouseDelta.y);

    // 有模态层时，命中判定从最上层模态开始：点到模态外面等于点空白，
    // 不会穿透到底下的界面（这是弹窗/下拉能"拦住输入"的关键）。
    View* hitRoot = this->hasModal() ? this->getTopModal() : this->root;
    if (hitRoot == nullptr)
    {
        this->pointerDown   = down;
        this->gestureTarget = nullptr;
        return;
    }

    // ---- 手势：按下 / 按住移动 / 抬起 / 滚轮 ----
    if (down && !this->pointerDown)
    {
        View* hit           = hitRoot->hitTest(position);
        this->gestureTarget = hit;
        if (hit != nullptr)
            hit->dispatchGesture(GestureState::START, position, Point(0.0f, 0.0f));
    }
    else if (!down && this->pointerDown)
    {
        if (this->gestureTarget != nullptr)
        {
            this->gestureTarget->dispatchGesture(GestureState::END, position, delta);
            this->gestureTarget = nullptr;
        }
    }
    else if (down && this->gestureTarget != nullptr && (delta.x != 0.0f || delta.y != 0.0f))
    {
        this->gestureTarget->dispatchGesture(GestureState::STAY, position, delta);
    }

    if (down && this->gestureTarget == nullptr)
    {
        // 按在空白处：后面所有手势事件都忽略，直到抬起
        this->pointerActive = true;
    }

    if (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f)
    {
        const float step = 40.0f; // 一格滚轮的像素量（和常见桌面一致）
        const Point scroll(-io.MouseWheelH * step, -io.MouseWheel * step);
        if (View* hit = hitRoot->hitTest(position))
            hit->dispatchGesture(GestureState::SCROLL, position, scroll);
    }

    // ---- 点击：只处理按下那一刻 ----
    if (!down)
    {
        this->pointerDown = false;
        return;
    }

    this->pointerActive = true;

    View* hit = hitRoot->hitTest(position);
    if (hit == nullptr)
    {
        this->pointerDown = true;
        return;
    }

    // 只在按下的那一刻处理，按住不放不重复触发。
    const bool pressed = !this->pointerDown;
    this->pointerDown   = true;

    if (!pressed)
        return;

    // 手势已经吃掉这次按下（比如拖动/轻触识别器接管）就不再当成"点击"，
    // 否则拖一个按钮会顺手把它按下去。
    if (this->gestureTarget != nullptr && hit == this->gestureTarget
        && !this->gestureTarget->getGestures().empty())
        return;

    // 先聚焦，再执行动作。
    // 每次都尝试执行（不判断焦点是否变化）：否则第二次点同一个按钮就没反应了，
    // 而"是否响应指针"由 View::onPointerAction 决定，
    // "重复点击是否安全"由各控件自己的动作逻辑决定（比如 tab 同页切换会直接返回）。
    this->focusFromPointer(hit);
    audio::play(Sound::CLICK);
    hit->triggerAction(ControllerButton::A, true);
}

void InputManager::handleActions()
{
    // 确认/返回也走重复模型（borealis 的 A/B 一样会重复）：
    // 键盘的 Enter/Space、Esc，手柄的 A/B 合起来算"按住"
    const bool keysConfirm = this->isKeyDownAny(ImGuiKey_Enter, ImGuiKey_Space);
    const bool keysBack    = this->isKeyDownAny(ImGuiKey_Escape, -1);

    GamepadSource& gamepad = getGamepadSource();
    const bool heldConfirm = keysConfirm || gamepad.isConfirmPressed();
    const bool heldBack    = keysBack || gamepad.isBackPressed();

    const double now = this->clock;

    const bool confirm = this->repeatConfirm.step(heldConfirm, now);
    const bool back    = this->repeatBack.step(heldBack, now);

    if (this->currentFocus == nullptr)
        return;

    if (confirm)
    {
        audio::play(Sound::CLICK);
        this->currentFocus->triggerAction(ControllerButton::A);
    }
    if (back)
    {
        audio::play(Sound::BACK);
        this->currentFocus->triggerAction(ControllerButton::B);
    }
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

} // namespace bklv

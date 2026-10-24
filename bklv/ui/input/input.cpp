#include "bklv/system/media/audio.hpp"
#include "bklv/foundation/core/services.hpp"
#include "bklv/foundation/core/event.hpp"
#include "bklv/ui/input/input.hpp"

#include "bklv/foundation/anim/time.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "bklv/ui/view/box.hpp"
#include "bklv/ui/input/gamepad.hpp"
#include "bklv/foundation/core/globals.hpp"
#include "bklv/foundation/core/i18n.hpp"
#include "bklv/foundation/core/logger.hpp"
#include "bklv/ui/view/view.hpp"

namespace bklv
{

namespace
{

// 提示栏排序：START 最前（"菜单"类动作习惯靠边）、B 次之、A 最后（确认键永远压尾），
// 其余按键一律 1、保持沿链注册的顺序。顺序照抄 borealis 的 buttonToSortableVal。
int hintSortRank(ControllerButton button)
{
    switch (button)
    {
        case ControllerButton::START:
            return 0;
        case ControllerButton::B:
            return 2;
        case ControllerButton::A:
            return 3;
        default:
            return 1;
    }
}

// 事件音效：外观里的 states.<trigger>.sound 优先，否则用主题 sound.json / 合成音
void playUiSound(View* view, Sound sound, const char* trigger)
{
    if (view != nullptr)
    {
        const std::string override = view->getSoundOverride(trigger);
        if (!override.empty())
        {
            audio::playFile(override);
            return;
        }
    }
    audio::play(sound);
}

} // namespace

// ---------------------------------------------------------------------------
// InputManager
// ---------------------------------------------------------------------------

namespace
{
InputManager* g_contextInput = nullptr;
}

void setContextInputManager(InputManager* manager)
{
    g_contextInput = manager;
}

InputManager* getContextInputManager()
{
    return g_contextInput;
}

InputManager& getInputManager()
{
    if (g_contextInput != nullptr)
        return *g_contextInput;
    // 登记在全局表里（core/globals.hpp）：可列举、可随 resetGlobals() 一起重置。
    // 它握着焦点/模态这类指着视图的状态，重置必须发生在视图销毁之前（App 的收尾就是这么做的）。
    return *globals::getGlobalStore().obtain<InputManager>("inputManager");
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

    // 上一帧有视图被删除、焦点被迫清空：现在树是稳定的，补一次默认焦点。
    // 不能在 giveFocus 的删除路径里同步做：那时调用方往往正在 clearViews()，
    // 兄弟节点已经是野指针，同步走 getDefaultFocus() 会踩到它。
    if (this->focusRestorePending)
    {
        this->focusRestorePending = false;
        if (this->root != nullptr && this->currentFocus == nullptr)
            this->giveFocus(this->root->getDefaultFocus());
    }

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

namespace
{

// 系统键用的 ImGuiKey → 手柄按钮映射：只为了知道"这个系统键吃掉了哪个动作"，
// 好让同帧的 handleActions 不再吃第二次（v15 另两条）。映射不上的返回 NONE。
ControllerButton controllerButtonForKey(int key)
{
    if (key == ImGuiKey_Enter || key == ImGuiKey_Space || key == ImGuiKey_GamepadFaceDown)
        return ControllerButton::A;
    if (key == ImGuiKey_Escape || key == ImGuiKey_GamepadFaceRight)
        return ControllerButton::B;
    return ControllerButton::NONE;
}

} // namespace

void InputManager::handleNavigation()
{
    // 系统键最先处理：**优先级高于模态**（模态只挡导航与指针命中，不挡截图/Home/退出）。
    // 用 IsKeyPressed 的边缘语义，避免按住时每帧触发；无 ImGui 上下文（无窗口测试）时跳过。
    this->consumedActionsThisFrame.clear();
    if (!this->systemActions.empty() && ImGui::GetCurrentContext() != nullptr)
    {
        // 先收集本帧按下的键再逐个触发：回调里可能增删系统键注册表，
        // 边遍历边调会让迭代器失效。
        std::vector<int> pressed;
        for (const auto& entry : this->systemActions)
            if (entry.second && ImGui::IsKeyPressed((ImGuiKey)entry.first, false))
                pressed.push_back(entry.first);

        for (int key : pressed)
            this->dispatchSystemKey(key);
    }

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
    InputService* input = getServices().input;
    const bool heldUp = keysUp || gamepad.isDirectionHeld(FocusDirection::UP)
        || (input != nullptr && input->isDirectionHeld(0));
    const bool heldDown = keysDown || gamepad.isDirectionHeld(FocusDirection::DOWN)
        || (input != nullptr && input->isDirectionHeld(1));
    const bool heldLeft = keysLeft || gamepad.isDirectionHeld(FocusDirection::LEFT)
        || (input != nullptr && input->isDirectionHeld(2));
    const bool heldRight = keysRight || gamepad.isDirectionHeld(FocusDirection::RIGHT)
        || (input != nullptr && input->isDirectionHeld(3));

    // 焦点捕获：方向键直接转给捕获者（选择器设置态靠这条调值），
    // 同时**吃掉**所有方向导航 —— 焦点必须钉在它身上。
    // 走同一套 KeyRepeater，所以长按连发是现成的（首按立刻，之后 250ms/100ms）。
    if (this->focusCapture != nullptr)
    {
        if (this->repeatUp.step(heldUp, now))
            this->focusCapture->handleDirectionalInput(FocusDirection::UP);
        else if (this->repeatDown.step(heldDown, now))
            this->focusCapture->handleDirectionalInput(FocusDirection::DOWN);
        else if (this->repeatLeft.step(heldLeft, now))
            this->focusCapture->handleDirectionalInput(FocusDirection::LEFT);
        else if (this->repeatRight.step(heldRight, now))
            this->focusCapture->handleDirectionalInput(FocusDirection::RIGHT);
        return;
    }

    if (this->repeatUp.step(heldUp, now))
        this->navigate(FocusDirection::UP);
    else if (this->repeatDown.step(heldDown, now))
        this->navigate(FocusDirection::DOWN);
    else if (this->repeatLeft.step(heldLeft, now))
        this->navigate(FocusDirection::LEFT);
    else if (this->repeatRight.step(heldRight, now))
        this->navigate(FocusDirection::RIGHT);
}

bool InputManager::dispatchSystemKey(int key)
{
    auto it = this->systemActions.find(key);
    if (it == this->systemActions.end() || !it->second)
        return false;

    // 先拷再调：回调里可能注销/改写这个注册表（那是它的自由）
    std::function<void()> handler = it->second;
    handler();

    // 系统键也是某个动作键（A/B）时记一笔：同帧 handleActions 不许再吃一次（v15 另两条）
    const ControllerButton consumed = controllerButtonForKey(key);
    if (consumed != ControllerButton::NONE)
        this->consumedActionsThisFrame.push_back(consumed);

    return true;
}

bool InputManager::isActionConsumedThisFrame(ControllerButton button) const
{
    for (ControllerButton consumed : this->consumedActionsThisFrame)
        if (consumed == button)
            return true;
    return false;
}

void InputManager::markNavigationBlockedByPolicy()
{
    // 供 Box 的跨 box 策略调用：本次导航是被策略主动拦下的（静默，不触发撞边反馈）
    this->navigationBlockedByPolicy = true;
}

View* InputManager::navigationScope() const
{
    View* top = this->getTopModal();
    return top != nullptr ? top : this->root;
}

View* InputManager::scopeRootForLayer(int layer) const
{
    if (layer <= 0)
        return this->root;

    const size_t index = (size_t)layer - 1;
    if (index < this->modals.size())
        return this->modals[index].modal;

    return nullptr; // 那一层已经没了
}

void InputManager::navigate(FocusDirection direction)
{
    if (direction == FocusDirection::NONE)
        return;

    this->navigationBlockedByPolicy = false;

    // 还没有焦点：先取**当前范围**（模态优先）的默认焦点。
    // 焦点为空不触发撞边反馈（没有"当前焦点"可以反馈）。
    if (this->currentFocus == nullptr)
    {
        View* scope = this->navigationScope();
        if (View* defaultFocus = scope != nullptr ? scope->getDefaultFocus() : nullptr)
            this->giveFocus(defaultFocus, true);
        return;
    }

    // 焦点是根自己（没有父容器）就没有兄弟层可导航，和 borealis 一样直接算撞边
    View* next = nullptr;
    if (this->currentFocus->hasParent())
    {
        next = this->currentFocus->getNextFocus(direction, this->currentFocus);

        // 有模态层时不许把焦点移到模态外面去
        if (next != nullptr && !this->isInsideModal(next))
            next = nullptr;
    }

    if (next != nullptr && next != this->currentFocus)
    {
        // 容器"返回 this"表示焦点留在原地、由它自己把焦点滚进视野（滚动容器）
        if (!next->isFocusable() && next->isScrollable())
        {
            this->ensureFocusVisible();
            return;
        }

        if (this->canFocus(next))
        {
            this->giveFocus(next, true);
            return;
        }
        // 目标不可聚焦（被禁用 / 隐藏 / 模态挡住）→ 落到下面的"走不动"分支
    }

    // 确实无法移动：被 CrossBoxPolicy 主动拦住时静默，否则给一次撞边反馈
    if (this->navigationBlockedByPolicy)
        return;

    this->triggerFocusEdgeHit(direction);
}

void InputManager::triggerFocusEdgeHit(FocusDirection direction)
{
    if (this->currentFocus == nullptr)
        return;

    // 节流：长按方向键会连续导航，不节流就会狂响 / 狂抖
    if (this->lastEdgeHitTime >= 0.0
        && this->clock - this->lastEdgeHitTime < (double)this->edgeHitMinInterval)
        return;

    this->lastEdgeHitTime = this->clock;

    // 库自己的反馈只有"焦点框抖一下"（照附录 B3 的 shakeHighlight）；
    // 音效之类的额外反馈由上层在 onFocusEdgeHit 回调里挂。
    this->shakeHighlightOnEdge(direction);

    if (this->onFocusEdgeHit != nullptr)
        this->onFocusEdgeHit(direction, this->currentFocus);
}

void InputManager::shakeHighlightOnEdge(FocusDirection direction)
{
    if (this->currentFocus == nullptr)
        return;

    this->currentFocus->shakeHighlight(direction);
}

bool InputManager::canFocus(View* view) const
{
    if (view == nullptr)
        return false;

    if (!view->isFocusable())
        return false;
    if (view->isDisabled())
        return false;

    // 有意偏离 borealis 的两条（见 docs/shell-implementation-plan.md 附录 B）：
    // ① 沿父链可见：隐藏容器（GONE）里的子节点不该被选中
    if (!view->isVisibleInTree())
        return false;

    // ② 未被模态遮挡：bklv 的模态是标记而不是活动栈，必须显式判
    if (!this->isInsideModal(view))
        return false;

    return true;
}

void InputManager::ensureFocusVisible()
{
    if (this->currentFocus == nullptr)
        return;

    // 沿父链找到最近的滚动容器，让它把焦点滚进可视区
    for (View* v = this->currentFocus->getParent(); v != nullptr; v = v->getParent())
    {
        if (v->isScrollable())
        {
            v->scrollToView(this->currentFocus, true);
            return;
        }
    }
}

void InputManager::setNavigationWrap(View* box, bool wrap)
{
    if (box == nullptr || box->asBox() == nullptr)
    {
        BKLV_LOG_WARNING("setNavigationWrap: %s 不是容器，导航循环开关没生效",
            box != nullptr ? box->getClassString().c_str() : "nullptr");
        return;
    }

    box->asBox()->setNavigationWrap(wrap);
}

void InputManager::setCrossBoxPolicy(View* box, CrossBoxPolicy policy)
{
    if (box == nullptr || box->asBox() == nullptr)
    {
        BKLV_LOG_WARNING("setCrossBoxPolicy: %s 不是容器，跨 box 策略没生效",
            box != nullptr ? box->getClassString().c_str() : "nullptr");
        return;
    }

    box->asBox()->setCrossBoxPolicy(policy);
}

void InputManager::advanceClock(float seconds)
{
    this->clock += (double)seconds;
}

void InputManager::resetFocusState()
{
    this->modals.clear();
    this->currentFocus = nullptr;
    this->gestureTarget = nullptr; // 同上：根视图一换就是野指针
    this->focusRestorePending = false; // 换根会自己补默认焦点，不要再补一次

    // 跨层焦点记忆栈也一起清：它记的是旧树里的路径，留着只会得到无意义的一层
    // （按页记忆 keyedFocusRecords 不清：它是"页面 id → 路径"，解析失败会自然退默认焦点）。
    this->rememberedStack.clear();
    this->consumedActionsThisFrame.clear();

    // 撞边节流也一起复位：换根/重建视图后第一次撞边应该照常反馈，
    // 不能因为上一次的残留时间戳被静默吞掉。
    this->lastEdgeHitTime = -1.0;
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

void InputManager::pushModal(View* modal, bool closeOnBack, ModalUnhandledHandler onUnhandled)
{
    if (modal == nullptr)
        return;

    ModalEntry entry;
    entry.modal       = modal;
    entry.savedFocus  = this->currentFocus;
    entry.closeOnBack = closeOnBack;
    entry.onUnhandled = std::move(onUnhandled);
    this->modals.push_back(entry);

    // 焦点先进模态里的默认焦点（否则第一次方向键会从外面开始算）
    if (View* inner = modal->getDefaultFocus())
        this->giveFocus(inner);
    else if (modal->isFocusable())
        this->giveFocus(modal);
}

void InputManager::setModalEmptyClickHandler(std::function<void(const Point&)> handler)
{
    if (this->modals.empty())
        return;

    this->modals.back().onEmptyClick = std::move(handler);
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

void InputManager::setFocusCapture(View* view)
{
    this->focusCapture = view;
    if (view != nullptr)
        this->giveFocus(view);
}

void InputManager::giveFocus(View* view, bool playSound)
{
    if (view == this->currentFocus)
        return;

    if (view != nullptr && !view->isFocusable() && view->getDefaultFocus() == nullptr)
        return;

    // 焦点变化事件（全局 UI 可以订阅它做联动）
    {
        FocusChangedEvent event;
        event.previous = this->currentFocus;
        event.current  = view;
        getEventBus().publish(event);
    }

    // 焦点反馈音：只给用户驱动的导航播（见头文件里的说明）。
    // 换根 / 换页 / 弹层进出这些程序化赋值传 false，否则每加载一页都要响一声。
    if (playSound && view != nullptr)
    {
        const bool sidebar = view->getClassString() == "bklv::TabButton";
        playUiSound(view, sidebar ? Sound::FOCUS_SIDEBAR : Sound::FOCUS_CHANGE, "focus");
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
        {
            (*it)->onFocusGained();

            // 顺着焦点路径把"这一层的哪个直接子节点被聚焦"记下来，
            // Box 靠它做"上次焦点记忆"（默认焦点第 ② 级）。
            if (Box* parent = (*it)->getParent())
                parent->onChildFocusGained(*it, this->currentFocus);
        }
    }
}

// ---------------------------------------------------------------------------
// 焦点记忆（跨层 / 同层换页）
//
// 只存"怎么找到它"：层号 + 从该层根出发的子节点下标路径，绝不存裸 View*。
// 恢复时重新解析并校验，所以原控件销毁后不会悬空。
// ---------------------------------------------------------------------------

namespace
{

// 从 scope 到 view 的子节点下标路径（"0/2"）。view 必须挂在 scope 内。
bool buildFocusPath(View* scope, View* view, std::string& out)
{
    if (scope == nullptr || view == nullptr || view == scope)
        return false;

    std::vector<size_t> indices;
    for (View* v = view; v != scope; v = v->getParent())
    {
        Box* parent = v->getParent();
        if (parent == nullptr)
            return false; // 不在同一范围内

        const std::vector<View*>& children = parent->getChildren();
        size_t index = children.size();
        for (size_t i = 0; i < children.size(); i++)
        {
            if (children[i] == v)
            {
                index = i;
                break;
            }
        }
        if (index == children.size())
            return false;

        indices.push_back(index);
    }

    for (auto it = indices.rbegin(); it != indices.rend(); ++it)
    {
        if (!out.empty())
            out += '/';
        out += std::to_string(*it);
    }
    return true;
}

// 按路径解析回视图；任何一段越界 / 对不上都返回 nullptr。
View* resolveFocusPath(View* scope, const std::string& path)
{
    if (scope == nullptr || path.empty())
        return nullptr;

    View* current = scope;
    size_t start  = 0;

    while (start <= path.size())
    {
        const size_t slash = path.find('/', start);
        const std::string token =
            path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (token.empty())
            return nullptr;

        Box* box = current->asBox();
        if (box == nullptr)
            return nullptr;

        size_t index = 0;
        for (char c : token)
        {
            if (c < '0' || c > '9')
                return nullptr;
            index = index * 10 + (size_t)(c - '0');
        }

        const std::vector<View*>& children = box->getChildren();
        if (index >= children.size())
            return nullptr;

        current = children[index];

        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }

    return current;
}

} // namespace

FocusRecord InputManager::captureFocusRecord(const std::string& key) const
{
    FocusRecord record;
    record.layer = (int)this->modals.size();
    record.key   = key;

    if (this->currentFocus == nullptr)
        return record;

    buildFocusPath(this->navigationScope(), this->currentFocus, record.viewPath);

    // 上次下标 = 该控件在自己父容器里的下标
    if (Box* parent = this->currentFocus->getParent())
    {
        const std::vector<View*>& children = parent->getChildren();
        for (size_t i = 0; i < children.size(); i++)
        {
            if (children[i] == this->currentFocus)
            {
                record.hint = (int)i;
                break;
            }
        }
    }

    return record;
}

bool InputManager::applyFocusRecord(const FocusRecord& record)
{
    if (record.viewPath.empty())
        return false;

    View* scope = this->scopeRootForLayer(record.layer);
    if (scope == nullptr)
        scope = this->navigationScope(); // 那一层没了就退回当前范围
    if (scope == nullptr)
        return false;

    View* target = resolveFocusPath(scope, record.viewPath);

    // 路径可能落在不可聚焦的容器上；往下取它的默认焦点
    if (target != nullptr && !target->isFocusable())
        target = target->getDefaultFocus();

    if (target != nullptr && this->canFocus(target))
    {
        this->giveFocus(target);
        return true;
    }

    // 恢复失败 → 退**该范围**的默认焦点；连默认焦点都没有就保持当前焦点不动
    if (View* fallback = scope->getDefaultFocus())
    {
        if (this->canFocus(fallback))
            this->giveFocus(fallback);
    }

    return false;
}

void InputManager::remember()
{
    // 栈：弹窗套弹窗时每一层各压一条，逐级还原（单槽会被里层覆盖，见 v15 F5）
    this->rememberedStack.push_back(this->captureFocusRecord(""));
}

bool InputManager::loadFocus()
{
    if (this->rememberedStack.empty())
        return false;

    FocusRecord record = this->rememberedStack.back();
    this->rememberedStack.pop_back();

    return this->applyFocusRecord(record);
}

void InputManager::enterLayer(Box* layerRoot)
{
    this->remember();

    if (layerRoot == nullptr)
        return;

    // 取该层默认焦点；没有就保持当前焦点不动（v14）
    if (View* inner = layerRoot->getDefaultFocus())
    {
        if (this->canFocus(inner))
            this->giveFocus(inner);
    }
}

void InputManager::leaveLayer()
{
    // loadFocus() 内部已经含"失败退该范围默认焦点"，都没有时保持当前焦点不动。
    // 返回 void 是 check_naming.py 的硬约束（`leave` 不在动作前缀表里，见 v15 F4）。
    this->loadFocus();
}

void InputManager::rememberFor(const std::string& key)
{
    this->keyedFocusRecords[key] = this->captureFocusRecord(key);
}

bool InputManager::loadFocusFor(const std::string& key)
{
    auto it = this->keyedFocusRecords.find(key);
    if (it == this->keyedFocusRecords.end())
        return false;

    return this->applyFocusRecord(it->second);
}

bool InputManager::dispatchActionWithinScope(ControllerButton button)
{
    View* scope = this->navigationScope();
    if (this->currentFocus == nullptr || scope == nullptr)
        return false;

    for (View* v = this->currentFocus; v != nullptr; v = v->getParent())
    {
        // 只看**这一级自己**注册的动作（getActions 是公开的）：
        // View::triggerAction 会顺带沿父链一路冒到根视图，模态弹窗不处理按键时
        // 就会打到 Shell 的默认动作，违反"弹窗优先于外壳"（v15 F2）。
        for (const InputAction& action : v->getActions())
        {
            if (action.button != button || !action.listener)
                continue;

            // 先拷出来：listener 里可能 unregister 自己（会改 actions 这个 vector）
            ActionListener listener = action.listener;
            listener(v);
            return true;
        }

        if (v == scope)
            break;
    }

    return false;
}

bool InputManager::dispatchAction(ControllerButton button)
{
    if (button == ControllerButton::NONE || this->currentFocus == nullptr)
        return false;

    if (!this->hasModal())
        return this->currentFocus->triggerAction(button);

    // ① 模态子树内逐级派发（到模态根为止，不冒泡出模态）
    if (this->dispatchActionWithinScope(button))
        return true;

    // ② 没人处理：交给模态登记的兜底（Shell 用它把 closeOnBack 变成 popDialog，
    //    这样 Shell 的弹窗栈与遮罩栈不会跟输入层脱节）。
    //    先拷贝再调 —— 回调通常以 popModal() 收尾，会析构正在执行的那个 std::function。
    if (this->modals.back().onUnhandled != nullptr)
    {
        ModalUnhandledHandler handler = this->modals.back().onUnhandled;
        if (handler(button))
            return true;
    }

    // ③ 没有兜底时退回模态自己的 closeOnBack 语义（直接关这一层）
    if (button == ControllerButton::B && this->modals.back().closeOnBack
        && !this->modals.empty())
    {
        this->popModal();
        return true;
    }

    return false;
}

bool InputManager::dispatchPointerClick(const Point& position)
{
    // 有模态层时命中判定从最上层模态开始：点到模态外面等于点空白，不穿透到底下的界面
    View* hitRoot = this->hasModal() ? this->getTopModal() : this->root;
    if (hitRoot == nullptr)
        return false;

    this->pointerActive = true;

    View* hit = hitRoot->hitTest(position);
    if (hit == nullptr)
    {
        // 模态内的**空白**（弹窗外、或弹窗自己的非可聚焦区域）。
        // 这里不动 hitTest / focusFromPointer 的焦点语义（v15 的 F1(b)）：
        // 只把"点在哪儿"交给最上层模态登记的回调，由 Shell 判是否在弹窗矩形外。
        // 同样先拷贝回调再调（它内部会 popDialog → popModal → 析构原回调）。
        if (this->hasModal())
        {
            std::function<void(const Point&)> handler = this->modals.back().onEmptyClick;
            if (handler)
                handler(position);
        }
        return false;
    }

    // 手势已经吃掉这次按下（比如拖动/轻触识别器接管）就不再当成"点击"，
    // 否则拖一个按钮会顺手把它按下去。
    if (this->gestureTarget != nullptr && hit == this->gestureTarget
        && !this->gestureTarget->getGestures().empty())
        return true;

    // 先聚焦，再执行动作。
    // 每次都尝试执行（不判断焦点是否变化）：否则第二次点同一个按钮就没反应了。
    this->focusFromPointer(hit);
    playUiSound(hit, Sound::CLICK, "click");
    hit->triggerAction(ControllerButton::A, true);
    return true;
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

    // 手势目标也要清：它是"按下时命中的那个视图"，按住不放期间**切页会把它删掉**
    // （Pager 删旧页 / 下钻返回释放详情页），下一帧 handlePointer 再拿它
    // dispatchGesture 就是 use-after-free —— ASAN 实测踩过。
    // 被删的视图是手势目标的祖先时也要清（因为事件是沿父链往上派的）。
    // 注意**不要**在这里复位"按下"状态：那个状态现在只看物理鼠标（pointerHeld），
    // 由 handlePointer 每帧更新。以前在这里把消费标记清零，导致"点一下按钮、
    // 按钮所在页面被重建"时，鼠标一直按着就每帧重新触发一次点击。
    if (this->gestureTarget != nullptr)
    {
        for (View* v = this->gestureTarget; v != nullptr; v = v->getParent())
        {
            if (v == view)
            {
                this->gestureTarget = nullptr;
                break;
            }
        }
    }

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

    // 焦点清空，兜底的默认焦点推迟到下一帧（见 update() 里的 focusRestorePending）。
    // 这里**不能**同步找：调用方通常正在 clearViews() 删页面子树，此刻兄弟节点
    // 已经析构、父节点的 children 里还留着野指针 —— 同步 getDefaultFocus()
    // 会沿树走到已释放的节点上（实测崩溃栈：
    //   ~View → notifyViewDeleted → Box::getDefaultFocus → Box::getNextFocus）。
    this->currentFocus         = nullptr;
    this->focusRestorePending  = true;
}

void InputManager::handlePointer()
{
    ImGuiIO& io = ImGui::GetIO();

    const Point position(io.MousePos.x, io.MousePos.y);
    this->pointerPosition = position;

    const bool down  = io.MouseDown[0];
    const Point delta(io.MouseDelta.x, io.MouseDelta.y);

    // 边沿看**物理**状态（pointerHeld），不看"这一按有没有被消费"。
    // 踩过：换主题时页面被 clearViews() 重建，被点的按钮随之销毁 →
    // notifyViewDeleted() 会清掉消费标记；若边沿也用它判定，鼠标一直按着就会
    // 每帧重新派发一次点击（表现为点一下「切换主题」主题连续反复切换）。
    const bool pressEdge   = down && !this->pointerHeld;
    const bool releaseEdge = !down && this->pointerHeld;
    this->pointerHeld      = down;

    // 有模态层时，命中判定从最上层模态开始：点到模态外面等于点空白，
    // 不会穿透到底下的界面（这是弹窗/下拉能"拦住输入"的关键）。
    View* hitRoot = this->hasModal() ? this->getTopModal() : this->root;
    if (hitRoot == nullptr)
    {
        this->gestureTarget = nullptr;
        return;
    }

    // ---- 手势：按下 / 按住移动 / 抬起 / 滚轮 ----
    if (pressEdge)
    {
        View* hit           = hitRoot->hitTest(position);
        this->gestureTarget = hit;
        if (hit != nullptr)
            hit->dispatchGesture(GestureState::START, position, Point(0.0f, 0.0f));
    }
    else if (releaseEdge)
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

    // ---- 点击：只在按下的那一刻处理一次，按住不放不重复、抬起也不补一次 ----
    if (!pressEdge)
        return;

    // 点击逻辑全部在 dispatchPointerClick 里（设备无关入口，无窗口测试也走同一条路）
    this->dispatchPointerClick(position);
}

void InputManager::handleActions()
{
    // 确认/返回也走重复模型（borealis 的 A/B 一样会重复）：
    // 键盘的 Enter/Space、Esc，手柄的 A/B 合起来算"按住"
    const bool keysConfirm = this->isKeyDownAny(ImGuiKey_Enter, ImGuiKey_Space);
    const bool keysBack    = this->isKeyDownAny(ImGuiKey_Escape, -1);

    GamepadSource& gamepad = getGamepadSource();
    InputService* input = getServices().input;
    const bool heldConfirm = keysConfirm || gamepad.isConfirmPressed()
        || (input != nullptr && input->isConfirmDown());
    const bool heldBack = keysBack || gamepad.isBackPressed()
        || (input != nullptr && input->isBackDown());

    const double now = this->clock;

    bool confirm = this->repeatConfirm.step(heldConfirm, now);
    bool back    = this->repeatBack.step(heldBack, now);

    // 本帧已被系统键吃掉的动作不再响应（同一个 A/B 只算一次，见 v15 另两条）。
    // 仍然要调 step()（上面已经调过）以推进长按状态，否则按住时会一直判成"刚按下"。
    for (ControllerButton consumed : this->consumedActionsThisFrame)
    {
        if (consumed == ControllerButton::A)
            confirm = false;
        else if (consumed == ControllerButton::B)
            back = false;
    }

    if (this->currentFocus == nullptr)
        return;

    if (confirm)
    {
        playUiSound(this->currentFocus, Sound::CLICK, "click");
        this->dispatchAction(ControllerButton::A);
    }
    if (back)
    {
        playUiSound(this->currentFocus, Sound::BACK, "back");
        this->dispatchAction(ControllerButton::B);
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

std::vector<HintItem> InputManager::getActiveHints() const
{
    std::vector<HintItem> hints;

    if (this->currentFocus == nullptr)
        return hints;

    // 同一个按键只留离焦点最近的一条：外层（页面根 / 外壳）注册的"B 返回"
    // 不该压掉里层自己的同名按键。用"按键网格"当下标，顺便天然去重。
    constexpr size_t kButtonCount = (size_t)ControllerButton::RIGHT + 1;
    bool seen[kButtonCount] = {};

    for (View* view = this->currentFocus; view != nullptr; view = view->getParent())
    {
        for (const InputAction& action : view->getActions())
        {
            if (action.hidden || action.button == ControllerButton::NONE)
                continue;
            if (action.listener == nullptr || action.hintText.empty())
                continue;

            const size_t index = (size_t)action.button;
            if (index >= kButtonCount || seen[index])
                continue;

            seen[index] = true;

            HintItem hint;
            hint.button   = action.button;
            hint.hintText = action.hintText;
            hint.listener = action.listener;
            hint.owner    = view;
            hints.push_back(hint);

            if (hints.size() >= 6)
                return hints;
        }
    }

    // 一条 A 都没有时补"确认"：焦点控件可操作却没注册 A 的情况很少，但提示栏
    // 空着更奇怪（borealis 的 addUnableAButtonAction 就是这个意思）。
    // 这条没有 listener，点了不做事 —— 同样照 borealis：A 那条默认不可点。
    if (!seen[(size_t)ControllerButton::A] && hints.size() < 6)
    {
        HintItem hint;
        hint.button   = ControllerButton::A;
        hint.hintText = translate("hints/ok");
        hints.push_back(hint);
    }

    // START 最前、B 次之、A 最后（borealis 的 buttonToSortableVal 同序），
    // 其余按键保持沿链注册的顺序 —— 用 stable_sort 才保得住这一点。
    std::stable_sort(hints.begin(), hints.end(),
        [](const HintItem& left, const HintItem& right) {
            return hintSortRank(left.button) < hintSortRank(right.button);
        });

    return hints;
}

Point InputManager::getPointerPosition() const
{
    return this->pointerPosition;
}

} // namespace bklv

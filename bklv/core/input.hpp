#pragma once

// 输入与焦点系统，对应 borealis 的 core/input.hpp + core/application.cpp 中的焦点部分。
//
// 设计要点（与 borealis 一致）：
//   - 焦点是一棵独立的"当前选中路径"，Application 持有 currentFocus，Box 负责在其中导航
//   - 每个 View 自己注册动作（registerAction），底部提示栏统一展示
//   - 键鼠事件读 ImGui 后端已经收好的 ImGuiIO
//   - 手柄走 GamepadSource 抽象（见 core/gamepad.hpp）：
//     桌面是 SDL3，Switch 是 libnx，本文件因此不依赖任何平台 API

#include <functional>
#include <string>
#include <vector>

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
    LT, // Switch 上是 ZL
    RT, // Switch 上是 ZR
    LSB,
    RSB,
    START,
    BACK,
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

const char* getControllerButtonName(ControllerButton button);

// 按键字形：Switch 图标字体私有区里的码位（和 borealis 的 Hint::getKeyIcon 一致）。
// 返回 UTF-8 字符串，用 getIconFont() 画。
const char* getControllerButtonIcon(ControllerButton button);

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

// 长按重复：首次按下立刻响应，按住则等 delay 毫秒后每隔 interval 毫秒响应一次。
//
// 抽成独立小类是为了能无窗口测——这类"手感"逻辑以前只能靠人肉试，
// 结果"两个键共用一个边沿状态导致按住时每帧都触发"藏了很久。
class KeyRepeater
{
  public:
    KeyRepeater(int delayMs = 250, int intervalMs = 100)
        : delayMs(delayMs)
        , intervalMs(intervalMs)
    {
    }

    void setDelay(int milliseconds) { this->delayMs = milliseconds; }
    void setInterval(int milliseconds) { this->intervalMs = milliseconds; }

    // now 是累计秒数；held 是"这个方向/动作这一帧是否被按住"
    bool step(bool held, double now)
    {
        bool fire = false;

        if (held)
        {
            if (!this->held)
            {
                fire      = true;
                this->nextAt = now + (double)this->delayMs / 1000.0;
            }
            else if (now >= this->nextAt)
            {
                fire         = true;
                this->nextAt = now + (double)this->intervalMs / 1000.0;
            }
        }

        this->held = held;
        return fire;
    }

    void reset() { this->held = false; this->nextAt = 0.0; }

  private:
    int delayMs       = 250;
    int intervalMs    = 100;
    bool held         = false;
    double nextAt     = 0.0;
};

class InputManager
{
  public:
    // 每帧调用一次：处理导航、鼠标点击、动作派发。
    void update();

    // ---- 按键重复（长按连续移动）----
    //
    // 参考 borealis：**首次按下立刻响应一次**，按住不动的话等 repeatDelay 毫秒后
    // 开始重复，之后每 repeatInterval 毫秒一次（borealis 是 250ms / 100ms）。
    // 时间来自帧间隔，所以键盘、手柄、各平台共用同一套逻辑，不需要平台 API。
    void setRepeatDelay(int milliseconds);
    void setRepeatInterval(int milliseconds);
    int getRepeatDelay() const { return this->repeatDelayMs; }
    int getRepeatInterval() const { return this->repeatIntervalMs; }

    // 焦点管理
    void giveFocus(View* view);
    View* getCurrentFocus() const { return this->currentFocus; }

    // ---- 模态层（弹窗 / 下拉 / applet 面板）----
    //
    // pushModal() 会把当前焦点存起来，并把"指针命中"和"方向导航"限制在 modal 子树内
    // （点到外面等于点空白，不会穿透到下面的界面）；popModal() 恢复之前的焦点。
    // 传 closeOnBack=true 时按 B 会直接关闭这一层（modal 自己也可以注册 B 动作先处理）。
    void pushModal(View* modal, bool closeOnBack = true);
    void popModal();
    View* getTopModal() const;
    bool hasModal() const { return !this->modals.empty(); }
    // view 是否在最上层模态的子树里（模态生效时用来限制导航）
    bool isInsideModal(View* view) const;

    // 清空模态栈与焦点（App 销毁 / 换根视图时调）。
    // 不清的话模态栈里会留着已经被 delete 的视图，之后的指针命中与导航会踩野指针。
    void resetFocusState();

    // 底部提示栏内容（来自当前焦点视图注册的动作）
    std::vector<HintItem> getHints() const;

    // 鼠标/触摸位置，用于 hitTest
    Point getPointerPosition() const;

    // 某个 View 被销毁时通知输入层：如果它正在焦点路径上，重新选一个焦点。
    void notifyViewDeleted(View* view);

    // 导航树：焦点从 root 开始向下走
    void setRoot(View* root) { this->root = root; }

    View* getRoot() const { return this->root; }

    // 当前手势的目标（按下时命中的视图）；测试也可以直接喂手势事件
    View* getGestureTarget() const { return this->gestureTarget; }

    // 用鼠标点击切换焦点（点击空白不改变焦点）
    void focusFromPointer(View* hit);

    // 是否已经用鼠标点击过；点击后焦点会跟着鼠标走
    bool isPointerActive() const { return this->pointerActive; }

  private:
    void handleNavigation();
    void handlePointer();
    void handleActions();

    // 从 ImGuiIO 里读键盘边沿；keyIndex 用 ImGuiKey_*
    bool isKeyDownAny(int keyA, int keyB) const;

    void navigate(FocusDirection direction);

    View* currentFocus = nullptr;

    // 长按重复：borealis 的 250ms 触发 / 100ms 重复
    int repeatDelayMs    = 250;
    int repeatIntervalMs = 100;
    double clock         = 0.0; // 累计时间（秒），帧间隔累加
    KeyRepeater repeatUp, repeatDown, repeatLeft, repeatRight;
    KeyRepeater repeatConfirm, repeatBack;

    struct ModalEntry
    {
        View* modal         = nullptr;
        View* savedFocus    = nullptr;
        bool closeOnBack    = true;
    };
    std::vector<ModalEntry> modals;
    View* root         = nullptr;

    bool pointerDown = false;
    bool pointerActive = false;
    View* gestureTarget = nullptr;
    Point pointerPosition;

    // 键盘"按下沿"的上一帧状态
};

// 全局输入管理器
InputManager& getInputManager();

// 绘制底部提示栏（由 App 每帧调用）
// 提示栏不再由这里画：改用 widgets/hints_bar 控件（AppletFrame 的底栏）。
// 输入层只负责提供"当前焦点有哪些动作"（getHints()）。

} // namespace bklv

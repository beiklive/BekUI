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
#include <map>
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

// 跨 box 导航策略（按 box 局部生效；没设过的 box 是 ALLOW）。
//
// 每一档只在"焦点要离开这个 box 自己的子树"时起作用：
//   ALLOW             正常冒泡，焦点可以离开这棵子树
//   SAME_PARENT_ONLY  只允许移到**同一个父容器**里的兄弟节点；要再往上就停住
//   NONE              不许离开这棵子树，停在边界（静默，不是"撞边"）
//
// 策略**不沿父子链继承**：继承的话，外层设 NONE 会把整棵子树的内部导航一起冻住
// （内层每次扫不到候选都会被外层的 NONE 拦下）。要"内层可覆盖"就在内层显式设一个
// 更宽松的值。循环开关是另一回事，见 InputManager::setNavigationWrap。
enum class CrossBoxPolicy
{
    ALLOW,
    SAME_PARENT_ONLY,
    NONE,
};

// 焦点记忆：只存"怎么找到它"，**绝不存裸 View***。
//
//   layer     记下时的层号：0 = 内容层，1+ = 模态层（当时模态栈的深度）
//   key       页面 id（rememberFor/loadFocusFor 用；remember/loadFocus 时为空）
//   viewPath  从该层根节点出发的子节点下标路径，如 "0/2"
//   hint      上次下标：记下时该控件在自己父容器里的下标，没有就是 -1
//
// 恢复时按路径重新解析，所以原控件被销毁时不会悬空。代价是只按下标定位：
// 若只是删掉了它前面的兄弟，路径会落到"顶上来"的那个控件上；整页换掉
// （路径整体失效）时会正确地退回该范围的默认焦点。
struct FocusRecord
{
    int layer = 0;
    std::string key;
    std::string viewPath;
    int hint = -1;
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

    // ---- 方向导航 ----

    // 按方向移动焦点。键盘/手柄由 update() 每帧调用；
    // 程序化导航与测试也直接调它（无窗口、不依赖帧时间）。
    //
    // 算法照 borealis（方案附录 B3）：**顺序索引扫描 ±1 + 父链冒泡**，不是几何最近邻。
    // 确实走不动时给一次撞边反馈（节流）；被 CrossBoxPolicy::NONE 拦住则静默。
    void navigate(FocusDirection direction);

    // 这个视图现在能不能当焦点：可聚焦 + 未禁用 + 沿父链可见 + 未被模态遮挡。
    //
    // 后两条是**有意偏离 borealis** 的：它的模态是活动栈，天然隔离；
    // bklv 的模态是 isInsideModal() 标记，必须显式判。
    bool canFocus(View* view) const;

    // 把当前焦点滚进视野（滚动容器里用）。沿父链找最近的滚动容器，调它的 scrollToView。
    void ensureFocusVisible();

    // 撞边时让当前焦点抖一下（绘制期位移，不进布局）。
    // 撞边反馈的默认动作之一；单独暴露是为了让上层/测试能直接触发视觉反馈。
    void shakeHighlightOnEdge(FocusDirection direction);

    // ---- 导航策略（状态存在 Box 上，这里只是转发，避免两套真相）----

    // 这个 box（及其子树，内层可覆盖）内导航是否**循环**。默认不循环。
    // wrap 优先于跨 box 策略：能绕回本 box 内的候选时，就不算"要出去"。
    void setNavigationWrap(View* box, bool wrap);

    // 这个 box 的跨 box 策略（只在本 box 的边界上生效）。传非容器视图会被忽略。
    void setCrossBoxPolicy(View* box, CrossBoxPolicy policy);

    // ---- 撞边反馈 ----

    // 焦点**确实无法移动**时回调一次（带用户按的方向与当前焦点）。
    // 能移动时、焦点为空时、被 CrossBoxPolicy::NONE 主动拦住时都不会触发。
    // 库自己只做抖动（shakeHighlightOnEdge）；音效之类交上层在这个回调里挂
    // （borealis 是 SOUND_FOCUS_ERROR + 抖动，见方案附录 B3）。
    void setOnFocusEdgeHit(std::function<void(FocusDirection, View*)> callback)
    {
        this->onFocusEdgeHit = std::move(callback);
    }

    // 撞边反馈的最小间隔（秒）。长按方向键会连续导航，不节流就会狂响/狂抖。默认 0.12s。
    void setEdgeHitMinInterval(float seconds) { this->edgeHitMinInterval = seconds; }
    float getEdgeHitMinInterval() const { return this->edgeHitMinInterval; }

    // ---- 焦点记忆（跨层 / 同层换页）----

    // 记下当前焦点（跨层用：内容层 ↔ 弹窗）。
    void remember();

    // 恢复到 remember() 记下的控件。恢复成功返回 true；
    // 目标不在了就退该范围的默认焦点，连默认焦点都没有则**保持当前焦点不动**。
    //
    // 名字说明：原本叫 restore()，但 bool 返回值的动作方法要过
    // scripts/check_naming.py 的前缀白名单（is/has/can/get + 动词表），
    // `restore` 不在表里；按"不改白名单"的定稿改用 `load` 前缀。
    bool loadFocus();

    // 按页面 id 记忆（同层换页用：主页 → 游戏库 → 回主页回到原卡片）。
    void rememberFor(const std::string& key);
    bool loadFocusFor(const std::string& key);

    // 内部时钟推进（秒）。正常运行由 update() 用帧间隔累加；
    // 测试要确定性地验证撞边节流 / 长按重复时手动调。
    void advanceClock(float seconds);

    // 供 Box 的跨 box 策略调用：告诉本次导航"是被策略主动拦下的"，
    // 用来把"真的撞边了（要反馈）"和"策略不许出去（静默）"区分开。
    void markNavigationBlockedByPolicy();

  private:
    void handleNavigation();
    void handlePointer();
    void handleActions();

    // 从 ImGuiIO 里读键盘边沿；keyIndex 用 ImGuiKey_*
    bool isKeyDownAny(int keyA, int keyB) const;

    // 当前导航的"范围根"：有模态就是最上层模态，否则是根视图。
    View* navigationScope() const;
    // 第 layer 层的范围根（0 = 根视图，n = 第 n 层模态）；那一层没了返回 nullptr。
    View* scopeRootForLayer(int layer) const;

    FocusRecord captureFocusRecord(const std::string& key) const;
    bool applyFocusRecord(const FocusRecord& record);

    void triggerFocusEdgeHit(FocusDirection direction);

    View* currentFocus = nullptr;

    // 撞边反馈
    std::function<void(FocusDirection, View*)> onFocusEdgeHit;
    float edgeHitMinInterval = 0.12f; // 秒
    double lastEdgeHitTime   = -1.0;  // 内部时钟（秒），< 0 表示还没触发过
    bool navigationBlockedByPolicy = false; // 本次 navigate 是否被策略拦下

    // 焦点记忆（不存裸指针）
    FocusRecord rememberedFocus;
    bool hasRememberedFocus = false;
    std::map<std::string, FocusRecord> keyedFocusRecords;

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

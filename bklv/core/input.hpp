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
class Box;

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
    //
    // onUnhandled：模态子树里没人消费这个按键时的兜底（返回 true = 已处理）。
    // 壳层用它做 closeOnBack（`Shell::pushDialog` 传进来，内部再走 popDialog，
    // 这样 Shell 的弹窗栈/遮罩栈不会跟输入层脱节）。挂在 ModalEntry 上而不是全局槽位，
    // 生命周期随模态栈一起结束，不会留下指向已销毁 Shell 的回调。
    //
    // closeOnBack：**没有** onUnhandled 时的兜底语义 —— 直接 popModal()。
    // 壳层在场时永远走 onUnhandled（它传的 closeOnBack 与自己的 DialogOptions 一致）。
    using ModalUnhandledHandler = std::function<bool(ControllerButton)>;
    void pushModal(View* modal, bool closeOnBack = true, ModalUnhandledHandler onUnhandled = nullptr);
    void popModal();
    // 最上层模态登记的"空白处点击"回调（Shell 用来实现 closeOnDimClick）。
    // 注册点在 pushModal 之后；点模态内的空白（hitTest 返回 nullptr）时触发。
    void setModalEmptyClickHandler(std::function<void(const Point&)> handler);
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

    // ---- 设备无关的输入入口 ----
    //
    // update() 每帧从 ImGuiIO / 手柄读状态之后，内部也走这两个入口。
    // 单独暴露的理由（v15 审计）：handleActions/handlePointer 直接读 ImGui::GetIO()，
    // 无窗口测试根本驱动不了它们 —— 「B 路由 / 点遮罩关闭」这类行为就只能靠人肉试。
    // 平台层（Switch 等）也可以绕过 ImGui 直接喂事件。

    // 一次指针点击（按下那一刻）。命中 → 聚焦 + 派发 A 动作；
    // 没命中且在模态内 → 交给该模态登记的「空白处回调」（弹窗点外部关闭用）。
    // 返回是否命中了视图。
    bool dispatchPointerClick(const Point& position);

    // 把一个动作交给当前焦点。有模态时**只在模态子树内**逐级派发（不冒泡出模态），
    // 没人处理再问模态登记的 onUnhandled；没有模态时沿父链正常冒泡。
    // 返回是否被消费。
    bool dispatchAction(ControllerButton button);

    // ---- 方向导航 ----

    // 按方向移动焦点。键盘/手柄由 update() 每帧调用；
    // 程序化导航与测试也直接调它（无窗口、不依赖帧时间）。
    //
    // 算法照 borealis（方案附录 B3）：**顺序索引扫描 ±1 + 父链冒泡**，不是几何最近邻。
    // 确实走不动时给一次撞边反馈（节流）；被 CrossBoxPolicy::NONE 拦住则静默。
    // 系统键：截图 / Home / 退出这类**优先级高于模态**的按键。
    // 弹窗打开时模态只挡"导航与指针命中"，系统键照样触发（落点在 handleNavigation 最顶部）。
    // key 用 ImGuiKey 的整数值，与 handleNavigation 里的键盘处理同一套。
    void setSystemAction(int key, std::function<void()> handler)
    {
        this->systemActions[key] = std::move(handler);
    }
    void clearSystemAction(int key) { this->systemActions.erase(key); }
    bool hasSystemAction(int key) const { return this->systemActions.count(key) > 0; }

    // 触发一次系统键（按下沿语义）。有 ImGui 上下文时 update() 内部也走这里；
    // 平台层（Switch 的 HOME/CAPTURE 到不了 ImGui，见 v15）与无窗口测试可以直接喂。
    // 返回是否有注册的动作被触发。
    bool dispatchSystemKey(int key);

    // 本帧这个动作是否已经被系统键消费（handleActions 靠它避免同帧双响应：
    // 在 A/B 上注册系统键时，系统键与普通动作各吃一次是 bug）。
    // 暴露出来是为了让平台层与测试能检查同帧互斥是否生效。
    bool isActionConsumedThisFrame(ControllerButton button) const;

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
    //
    // 记忆是**栈**（v15 的 F5）：弹窗套弹窗时每一层各压一条，逐级还原；
    // 单槽的话里层弹窗一打开就把外层记的覆盖掉，关到外层时回不到原控件。
    void remember();

    // 恢复到最近一次 remember() 记下的控件，并**弹出**那条记录。
    // 恢复成功返回 true；目标不在了就退该范围的默认焦点，
    // 连默认焦点都没有则**保持当前焦点不动**。
    //
    // 名字说明：原本叫 restore()，但 bool 返回值的动作方法要过
    // scripts/check_naming.py 的前缀白名单（is/has/can/get + 动词表），
    // `restore` 不在表里；按"不改白名单"的定稿改用 `load` 前缀。
    bool loadFocus();

    // 按页面 id 记忆（同层换页用：主页 → 游戏库 → 回主页回到原卡片）。
    void rememberFor(const std::string& key);
    bool loadFocusFor(const std::string& key);

    // ---- 跨层进出（弹窗打开 / 关闭时由壳层调）----

    // 进层：先 remember()（压栈），再把焦点交给这一层的默认焦点。
    // 层没有默认焦点时保持当前焦点不动（v14：覆盖层无默认焦点时输入仍被模态吃住）。
    void enterLayer(Box* layerRoot);

    // 出层：loadFocus() 还原上一次记下的焦点（失败时它内部已退该范围默认焦点）。
    //
    // 返回 void 是硬约束（v15 的 F4）：`leave` 不在 check_naming.py 的动作前缀表里，
    // 写成 `bool leaveLayer()` 会被判违规；而"不许改白名单脚本"是定稿。
    void leaveLayer();

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

    // 从当前焦点往上走到"范围根"（含）为止，逐级只看**这一级自己**注册的动作。
    // 不用 View::triggerAction 的理由见 dispatchAction 的注释（它会冒到根视图）。
    bool dispatchActionWithinScope(ControllerButton button);

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

    // 系统键：优先级高于模态（见 setSystemAction 注释）
    std::map<int, std::function<void()>> systemActions;
    // 本帧已被系统键消费的动作（A/B）。handleNavigation 顶部清空并填充，
    // handleActions 用它避免"同一个 A/B 被系统键与普通动作各吃一次"（v15 另两条）。
    std::vector<ControllerButton> consumedActionsThisFrame;
    float edgeHitMinInterval = 0.12f; // 秒
    double lastEdgeHitTime   = -1.0;  // 内部时钟（秒），< 0 表示还没触发过
    bool navigationBlockedByPolicy = false; // 本次 navigate 是否被策略拦下

    // 焦点记忆（不存裸指针）。跨层记忆是**栈**：逐级压、逐级弹（v15 F5）。
    std::vector<FocusRecord> rememberedStack;
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
        // 模态子树里没人消费按键时的兜底（Shell 用它做 closeOnBack → popDialog）
        ModalUnhandledHandler onUnhandled;
        // 点在模态内空白处（hitTest 返回 nullptr）时的回调（Shell 用它做 closeOnDimClick）
        std::function<void(const Point&)> onEmptyClick;
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
// 提示栏不再由这里画：改用 widgets/hints_bar 控件（挂在外壳覆盖层的底栏里）。
// 输入层只负责提供"当前焦点有哪些动作"（getHints()）。

} // namespace bklv

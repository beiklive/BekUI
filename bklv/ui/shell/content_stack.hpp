#pragma once

// 内容层的层级导航：**往上叠一层 / 退回上一层**（列表 → 详情，B 逐层返回），
// 带"旧层先出场、播完才摘它，再挂新层、给焦点、新层入场"的顺序，以及每层的
// **焦点记忆**与**滚动位置记忆**。
//
// 同级换页（主页 ↔ 设置页这种"同一个槽里换内容"）用 replace()：出场播完直接删旧层。
//
// 三层关系：
//   Shell            拿着一个 ContentStack，把它转发成 pushContentView/popContentView 等
//   ContentStack     只管栈、转场顺序、记忆；只认识 Box / View / 转场 / 输入管理器
//   ViewTransition   真正播动画（外观 JSON 的 transitions 段优先，没写用预设）
//
// 记忆的两种东西：
//   · 焦点：走 `InputManager::rememberFor(key)` / `loadFocusFor(key)`（存的是路径，不是裸指针）
//   · 滚动：`View::hasRestorableState() / getRestorableState() / setRestorableState()`
//     （`ScrollBox` 覆盖成自己的滚动偏移；别的容器默认没有可存的状态）
// 记忆的 key 默认取 `View::getId()`，没设 id 时用指针地址兜底 —— 想要"跨进程/跨次启动"
// 都能认出来的记忆，请给层根视图 `setId()`。

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "bklv/foundation/anim/transition.hpp"

namespace bklv
{

class Box;
class View;

class ContentStack
{
  public:
    // 内容挂到哪一层（外壳构造时给一次）。
    void setHostLayer(Box* layer) { this->hostLayer = layer; }

    // ---- 转场配置（时间单位是秒；入场 / 出场分开）----
    void setEnter(ContentTransition value, float seconds = 0.0f);
    void setExit(ContentTransition value, float seconds = 0.0f);
    void setBoth(ContentTransition value, float seconds = 0.0f);
    ContentTransition getEnter() const { return this->transition.getEnter(); }
    ContentTransition getExit() const { return this->transition.getExit(); }
    float getEnterDuration() const { return this->transition.getEnterDuration(); }
    float getExitDuration() const { return this->transition.getExitDuration(); }
    // 上一次（或正在跑的）那段的实际时长：外观里写了 transitions 时是 spec 的总时长
    float getDuration() const { return this->activeDuration; }

    bool isEntering() const { return this->transition.isEntering(); }
    bool isExiting() const { return this->transition.isExiting(); }
    bool isActive() const { return this->transition.isActive(); }
    // 正在出场的那一个（入场期间是 nullptr；调试/测试用）
    View* getOutgoing() const { return this->transition.getOutgoing(); }

    // ---- 栈操作 ----
    // 往上叠一层（接管所有权）。memoryKey 空则用 view->getId()。
    void push(View* view, const std::string& memoryKey = std::string());
    // 同级换页：出场播完**删掉**旧层，再挂新层入场。
    void replace(View* view, const std::string& memoryKey = std::string());
    // 弹出栈顶并**释放**它；已经在最外层时什么都不做，只执行回调。
    void pop(std::function<void()> onFinished = nullptr);

    View* getTop() const { return this->stack.empty() ? nullptr : this->stack.back().view; }
    size_t size() const { return this->stack.size(); }

    // 直接对某个视图播一次入场/出场（宿主也能用；出场回调里可以接着摘/删它）
    void playEnter(View* view);
    void playExit(View* view, std::function<void()> onFinished = nullptr);

    void step(float seconds);
    // 立刻落位（把正在跑的那段收尾，含出场回调）
    void finishNow();
    // 析构前调用：释放"脱离树"的那几层（挂在树里的那个随宿主 Box 回收）
    void releaseDetached();

  private:
    struct Entry
    {
        View* view  = nullptr;
        std::string key;
    };

    struct Memory
    {
        bool hasScroll = false;
        float scroll   = 0.0f;
    };

    // 待执行的动作：出场播完之后做（push 挂新层 / pop 挂回上一层 / replace 挂新层）
    enum class Pending
    {
        NONE,
        PUSH,
        POP,
        REPLACE,
    };

    static std::string keyOf(const Entry& entry);
    static View* findRestorable(View* view);
    static float layerWidth(const Box* host, const View* view);

    void rememberOutgoing();
    void restoreMemory(const Entry& entry) const;
    void attach(Entry& entry);
    void detachCurrent(bool destroy);
    void runPending();
    void giveFocusTo(View* view);

    Box* hostLayer = nullptr;
    std::vector<Entry> stack;
    // 当前**挂在树里**的那一层（转场期间旧层还挂着，所以它可能 != stack.back()）
    View* attached = nullptr;

    ViewTransition transition;
    float activeDuration = 0.0f;

    std::map<std::string, Memory> memories;

    Pending pending = Pending::NONE;
    Entry pendingEntry;
    std::function<void()> pendingCallback;
};

} // namespace bklv

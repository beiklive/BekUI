#pragma once

// 内容层的层级下钻：**往上叠一层 / 退回上一层**（列表 → 详情，B 逐层返回）。
//
// 同级换页不归它（那是 `Pager::show(id)` 的事）。
//
// 从 `Shell` 抽出来的：这块逻辑有一串顺序敏感的操作 —— 旧层先出场（它还挂在树上，
// 所以看得到）→ 出场播完才摘它并挂新层 → 把焦点交给新层 → 新层入场；弹出的那层
// 等出场播完才释放、回调也等那时才跑。它和外壳的其余职责（图层、页头底栏、弹窗、
// 背景淡入）没有关系，所以能单独读、单独测。
//
// 它只认识三样东西：宿主层（一个 Box）、一个"把焦点交给谁"的回调、自己拥有的内容栈。
// 不认识的：App、平台、图形 API、主题。

#include <cstddef>
#include <functional>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/transition.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

class ContentStack
{
  public:
    // 内容挂到哪一层（外壳构造时给一次）
    void setHostLayer(Box* layer) { this->hostLayer = layer; }

    // 焦点接收者：外壳注入 `getInputManager().giveFocus`。不注入就不挪焦点。
    void setFocusSink(std::function<void(View*)> sink) { this->focusSink = std::move(sink); }

    // ---- 转场配置（时间单位是秒；入场 / 出场分开）----
    void setEnter(ContentTransition value);
    ContentTransition getEnter() const;
    void setExit(ContentTransition value);
    ContentTransition getExit() const;

    // 入场那一段的时长（秒）。给"旧的单开关写法" setContentTransition 用：
    // 它要把两边设成同一个值，同时保留入场那段已有的时长。
    float getEnterDuration() const;

    // 入场/出场各一段时长
    void setBoth(ContentTransition value, float seconds);
    void setDuration(float seconds);
    // 报**实际**时长：外观里写了 transitions 时是 spec 的总时长（出场播完才摘页靠它）
    float getDuration() const;

    bool isEntering() const;
    bool isExiting() const;
    bool isActive() const;
    // 正在出场的那个视图（入场期间是 nullptr；调试/测试用）
    View* getOutgoing() const;

    // 直接对某个视图播一次入场/出场（宿主也能用；出场回调里可以接着摘/删它）
    void playEnter(View* view);
    void playExit(View* view, std::function<void()> onFinished = nullptr);

    // 推进转场（外壳的 update 每帧调）；finishNow 立刻落位（含出场回调）
    void step(float deltaSeconds);
    void finishNow();

    // ---- 栈操作 ----
    // 往上叠一层：旧层先出场，出场播完再摘它、挂新层、交焦点、新层入场。
    void push(View* view);
    // 弹出栈顶并**释放**它（下钻出来的详情页是临时页）。转场播完才真正摘/删。
    // 已经在最外层时什么都不做，只执行回调。
    void pop(std::function<void()> callback = nullptr);

    // 栈顶那一层（没挂过返回 nullptr）
    View* getTop() const;
    size_t size() const { return this->stack.size(); }

    // 析构前调用：释放"脱离树"的那几层（挂在树里的那个随宿主 Box 回收）
    void releaseDetached();

  private:
    void attach(View* view);
    View* detach();
    void giveFocusToTop();

    Box* hostLayer = nullptr;
    std::function<void(View*)> focusSink;

    // 栈底是基础内容，栈顶是当前显示的层
    std::vector<View*> stack;
    // 当前**挂在树里**的那一层（== stack.back()；转场期间旧层已脱离）
    View* attached = nullptr;

    ViewTransition transition;
    std::function<void()> pendingPopCallback;
};

} // namespace bklv

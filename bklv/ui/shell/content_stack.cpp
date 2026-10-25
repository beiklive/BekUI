#include "bklv/ui/shell/content_stack.hpp"

#include <cstdio>
#include <utility>

#include "bklv/foundation/anim/time.hpp"
#include "bklv/foundation/core/logger.hpp"
#include "bklv/ui/input/input.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/view/view.hpp"

namespace bklv
{

namespace
{
// 转场宽度：先问宿主层，宿主层没量出尺寸时退到视图自己（都拿不到就 0 = 只淡入）。
float measureWidth(const Box* host, const View* view)
{
    if (host != nullptr && host->getWidth() > 0.0f)
        return host->getWidth();
    if (view != nullptr && view->getWidth() > 0.0f)
        return view->getWidth();
    return 0.0f;
}
} // namespace

void ContentStack::setEnter(ContentTransition value, float seconds)
{
    this->transition.setEnter(value, seconds);
}

void ContentStack::setExit(ContentTransition value, float seconds)
{
    this->transition.setExit(value, seconds);
}

void ContentStack::setBoth(ContentTransition value, float seconds)
{
    this->transition.setBoth(value, seconds);
}

std::string ContentStack::keyOf(const Entry& entry)
{
    if (!entry.key.empty())
        return entry.key;
    if (entry.view != nullptr && !entry.view->getId().empty())
        return entry.view->getId();
    // 兜底：用地址（同一次运行里稳定；跨次启动认不出来，所以推荐显式 setId）
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "@%llx",
        (unsigned long long)(uintptr_t)entry.view);
    return std::string(buffer);
}

View* ContentStack::findRestorable(View* view)
{
    if (view == nullptr)
        return nullptr;
    if (view->hasRestorableState())
        return view;

    Box* box = view->asBox();
    if (box == nullptr)
        return nullptr;
    for (View* child : box->getChildren())
    {
        if (View* found = ContentStack::findRestorable(child))
            return found;
    }
    return nullptr;
}

void ContentStack::giveFocusTo(View* view)
{
    if (view == nullptr)
        return;

    View* target = view->isFocusable() ? view : view->getDefaultFocus();
    if (target == nullptr)
        return;
    getInputManager().giveFocus(target);
}

void ContentStack::rememberOutgoing()
{
    if (this->attached == nullptr)
        return;

    Entry entry;
    entry.view = this->attached;
    entry.key  = this->stack.empty() ? std::string() : this->stack.back().key;
    if (this->stack.empty())
        entry.key = ContentStack::keyOf(entry);

    const std::string key = ContentStack::keyOf(entry);

    // 焦点：存路径（InputManager 自己负责"那一层没了就退回默认焦点"）
    getInputManager().rememberFor(key);

    // 滚动：只记第一个"可存状态"的视图（列表页通常只有一个滚动容器）
    Memory memory;
    if (View* restorable = ContentStack::findRestorable(this->attached))
    {
        memory.hasScroll = true;
        memory.scroll    = restorable->getRestorableState();
    }
    this->memories[key] = memory;
}

void ContentStack::restoreMemory(const Entry& entry) const
{
    const std::string key = ContentStack::keyOf(entry);

    auto it = this->memories.find(key);
    if (it != this->memories.end() && it->second.hasScroll)
    {
        if (View* restorable = ContentStack::findRestorable(entry.view))
            restorable->setRestorableState(it->second.scroll);
    }

    // 焦点记忆在给过默认焦点之后再套（没有记录时会退回该层默认焦点）
    getInputManager().loadFocusFor(key);
}

void ContentStack::attach(Entry& entry)
{
    if (entry.view == nullptr || this->hostLayer == nullptr)
        return;

    this->hostLayer->addView(entry.view);
    this->attached = entry.view;

    // 先给默认焦点，再用记忆覆盖（记忆里没有记录时 loadFocusFor 会退回默认焦点）
    this->giveFocusTo(entry.view);
    this->restoreMemory(entry);

    const float width = measureWidth(this->hostLayer, entry.view);
    this->transition.playEnter(entry.view, width);
    this->activeDuration = this->transition.getActiveDuration();
}

void ContentStack::detachCurrent(bool destroy)
{
    if (this->attached == nullptr)
        return;

    View* view = this->attached;
    this->attached = nullptr;

    if (Box* parent = view->getParent())
        parent->removeView(view, destroy);
    else if (destroy)
        delete view;
}

void ContentStack::playEnter(View* view)
{
    if (view == nullptr)
        return;
    const float width = measureWidth(this->hostLayer, view);
    this->transition.playEnter(view, width);
    this->activeDuration = this->transition.getActiveDuration();
}

void ContentStack::playExit(View* view, std::function<void()> onFinished)
{
    if (view == nullptr)
    {
        if (onFinished)
            onFinished();
        return;
    }
    const float width = measureWidth(this->hostLayer, view);
    this->transition.playExit(view, width, std::move(onFinished));
    this->activeDuration = this->transition.getActiveDuration();
}

void ContentStack::push(View* view, const std::string& memoryKey)
{
    if (view == nullptr)
        return;

    Entry entry;
    entry.view = view;
    entry.key  = memoryKey;

    if (this->attached == nullptr)
    {
        // 空栈：第一层直接挂上（hostLayer 可能还没建好，下一帧 push 前会再试一次）
        this->stack.push_back(entry);
        this->attach(this->stack.back());
        return;
    }

    // 旧层先出场（还挂在树上，所以看得到），播完才摘它、挂新层
    this->rememberOutgoing();

    this->stack.push_back(entry);
    this->pending        = Pending::PUSH;
    this->pendingEntry   = entry;
    this->pendingCallback = nullptr;

    View* outgoing = this->attached;
    this->playExit(outgoing, [this]() { this->runPending(); });
}

void ContentStack::replace(View* view, const std::string& memoryKey)
{
    if (view == nullptr)
        return;

    if (this->attached == nullptr)
    {
        this->push(view, memoryKey);
        return;
    }

    // 同级换页：旧层出场播完直接删，不做记忆（它不会再回来）
    this->rememberOutgoing();

    Entry entry;
    entry.view = view;
    entry.key  = memoryKey;

    this->pending         = Pending::REPLACE;
    this->pendingEntry    = entry;
    this->pendingCallback = nullptr;

    View* outgoing = this->attached;
    this->playExit(outgoing, [this]() { this->runPending(); });
}

void ContentStack::pop(std::function<void()> onFinished)
{
    if (this->stack.size() <= 1 || this->attached == nullptr)
    {
        if (onFinished)
            onFinished();
        return;
    }

    // 离开之前记下这一层的焦点与滚动（下次再 push 同一层时还能回来）
    this->rememberOutgoing();

    this->pending         = Pending::POP;
    this->pendingEntry    = Entry{};
    this->pendingCallback = std::move(onFinished);

    View* outgoing = this->attached;
    this->playExit(outgoing, [this]() { this->runPending(); });
}

void ContentStack::runPending()
{
    const Pending pending = this->pending;
    if (pending == Pending::NONE)
        return;

    this->pending = Pending::NONE;

    switch (pending)
    {
    case Pending::PUSH:
    {
        // 旧层脱离树但**不删**（还在栈里），新层挂上并入场
        View* outgoing = this->attached;
        if (outgoing != nullptr && this->hostLayer != nullptr)
            this->hostLayer->removeView(outgoing, false);
        this->attached = nullptr;

        if (!this->stack.empty())
            this->attach(this->stack.back());
        break;
    }
    case Pending::REPLACE:
    {
        // 旧层脱离树并释放
        this->detachCurrent(true);
        this->stack.clear();

        if (this->pendingEntry.view != nullptr)
        {
            this->stack.push_back(this->pendingEntry);
            this->attach(this->stack.back());
        }
        break;
    }
    case Pending::POP:
    {
        // 栈顶出场播完：摘掉并释放，然后回到上一层（带记忆）
        Entry top = this->stack.back();
        this->stack.pop_back();

        this->detachCurrent(true);

        if (!this->stack.empty())
            this->attach(this->stack.back());

        if (this->pendingCallback)
        {
            std::function<void()> callback = std::move(this->pendingCallback);
            this->pendingCallback          = nullptr;
            callback();
        }
        break;
    }
    case Pending::NONE:
    default:
        break;
    }
}

void ContentStack::step(float seconds)
{
    if (seconds <= 0.0f)
        return;

    this->transition.step(seconds);
    this->activeDuration = this->transition.getActiveDuration();
}

void ContentStack::finishNow()
{
    this->transition.finishNow();
}

void ContentStack::releaseDetached()
{
    // 栈里除了"挂在树里"的那一层，其余都脱离了树 —— 它们归本对象释放
    for (Entry& entry : this->stack)
    {
        if (entry.view == nullptr || entry.view == this->attached)
            continue;
        delete entry.view;
        entry.view = nullptr;
    }
    this->stack.clear();
    this->attached = nullptr;
}

} // namespace bklv

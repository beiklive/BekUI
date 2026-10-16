#include "bklv/widgets/content_stack.hpp"

namespace bklv
{

// ---- 转场配置 ----

void ContentStack::setEnter(ContentTransition value)
{
    this->transition.setEnter(value, this->transition.getEnterDuration());
}

ContentTransition ContentStack::getEnter() const
{
    return this->transition.getEnter();
}

void ContentStack::setExit(ContentTransition value)
{
    this->transition.setExit(value, this->transition.getExitDuration());
}

ContentTransition ContentStack::getExit() const
{
    return this->transition.getExit();
}

float ContentStack::getEnterDuration() const
{
    return this->transition.getEnterDuration();
}

void ContentStack::setBoth(ContentTransition value, float seconds)
{
    this->transition.setBoth(value, seconds);
}

void ContentStack::setDuration(float seconds)
{
    this->transition.setBoth(this->transition.getEnter(), seconds);
}

float ContentStack::getDuration() const
{
    return this->transition.getActiveDuration();
}

bool ContentStack::isEntering() const
{
    return this->transition.isEntering();
}

bool ContentStack::isExiting() const
{
    return this->transition.isExiting();
}

bool ContentStack::isActive() const
{
    return this->transition.isActive();
}

View* ContentStack::getOutgoing() const
{
    return this->transition.getOutgoing();
}

void ContentStack::playEnter(View* view)
{
    const float width = this->hostLayer != nullptr ? this->hostLayer->getWidth() : 0.0f;
    this->transition.playEnter(view, width);
}

void ContentStack::playExit(View* view, std::function<void()> onFinished)
{
    const float width = this->hostLayer != nullptr ? this->hostLayer->getWidth() : 0.0f;
    this->transition.playExit(view, width, std::move(onFinished));
}

void ContentStack::step(float deltaSeconds)
{
    this->transition.step(deltaSeconds);
}

void ContentStack::finishNow()
{
    this->transition.finishNow();
}

// ---- 栈操作 ----

void ContentStack::attach(View* view)
{
    if (this->hostLayer == nullptr || view == nullptr)
        return;

    // 上一任从树上摘下来（**不删**：它还在栈里）
    if (this->attached != nullptr && this->attached->getParent() == this->hostLayer)
        this->hostLayer->removeView(this->attached, false);

    this->attached = view;
    this->hostLayer->addView(view);
}

View* ContentStack::detach()
{
    View* leaving = this->attached;
    if (leaving != nullptr && this->hostLayer != nullptr && leaving->getParent() == this->hostLayer)
        this->hostLayer->removeView(leaving, false);

    this->attached = nullptr;
    return leaving;
}

void ContentStack::giveFocusToTop()
{
    if (this->hostLayer == nullptr || !this->focusSink)
        return;

    // 焦点落到新层里的第一个可聚焦项
    if (View* inner = this->hostLayer->getDefaultFocus())
        this->focusSink(inner);
}

void ContentStack::push(View* view)
{
    if (view == nullptr)
        return;

    // 上一段转场还没播完：先落位（否则栈与画面会对不上）
    this->transition.finishNow();

    View* outgoing = this->attached;
    this->stack.push_back(view);

    // 首屏：没有旧层可让位，直接挂上（也不做入场：刚起界面就滑一下很怪）
    if (outgoing == nullptr)
    {
        this->attach(view);
        return;
    }

    // **旧层先出场**（它还挂在树上，所以看得到），出场播完再摘它、挂新层、新层入场。
    // 旧层不删：下钻时它留在栈里，返回时还要用。
    this->playExit(outgoing, [this, view] {
        this->detach();
        this->attach(view);
        this->giveFocusToTop();
        this->playEnter(view);
    });
}

void ContentStack::pop(std::function<void()> callback)
{
    if (this->stack.size() <= 1)
    {
        if (callback)
            callback();
        return;
    }

    this->transition.finishNow();

    View* outgoing = this->stack.back();
    this->stack.pop_back();

    View* incoming = this->stack.back();
    this->pendingPopCallback = callback;

    // **被弹出的临时页先出场**，出场播完再释放它、挂回上一层、让上一层入场。
    // 注意顺序：先把它从内容层摘下来再 delete，不能让 attached 指着已释放的对象。
    this->playExit(outgoing, [this, incoming, outgoing] {
        if (this->attached == outgoing)
            this->detach();

        if (this->hostLayer != nullptr && outgoing->getParent() == this->hostLayer)
            this->hostLayer->removeView(outgoing, true);
        else
            delete outgoing;

        this->attach(incoming);
        this->giveFocusToTop();
        this->playEnter(incoming);

        // pop 的回调等**出场**播完才跑（此时被弹出的页已经可以释放了）
        std::function<void()> done = this->pendingPopCallback;
        this->pendingPopCallback   = nullptr;
        if (done)
            done();
    });
}

View* ContentStack::getTop() const
{
    return this->stack.empty() ? nullptr : this->stack.back();
}

void ContentStack::releaseDetached()
{
    for (View* view : this->stack)
        if (view != nullptr && view != this->attached && view->getParent() == nullptr)
            delete view;

    this->stack.clear();
    this->attached = nullptr;
}

} // namespace bklv

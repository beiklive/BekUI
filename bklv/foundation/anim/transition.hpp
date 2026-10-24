#pragma once

// 视图转场：**入场 / 出场**两个方向的绘制期动画播放器。
//
// 为什么单独抽一个类：这套逻辑原来散在 `Shell`（内容栈下钻）和 `Pager`（同级换页）里，
// 两边各写一份、还都只有"新页入场"没有"旧页出场"。抽出来之后两边共用同一份实现，
// 而且宿主可以直接拿它对任意视图播一次入场/出场。
//
// 契约（三条，都是踩过坑总结的）：
//   1. **只管画**：改的是 `setTranslateX` / `setAlpha`，不参与布局 → 过渡期间零重排。
//   2. **不碰视图树的生命周期**：出场播完要做什么（摘 / 删 / 挂新页 / 给焦点）
//      由调用方在 `playExit(view, width, onFinished)` 的回调里决定。
//   3. 出场回调在**清完状态之后**才调，所以回调里可以立刻 `playEnter()`（"旧页出场 →
//      新页入场"就是自然写法）；`NONE` 或时长 <= 0 时**同步**落位并同步跑回调。
//
// 方向约定：入场从**右侧**滑到 0（宽度是 `width`），出场从 0 滑到**左侧**（-width）。

#include <functional>
#include <utility>

#include "bklv/ui/theme/theme_data.hpp"
#include "bklv/foundation/anim/time.hpp"
#include "bklv/foundation/anim/transition_player.hpp"
#include "bklv/ui/view/view.hpp"

namespace bklv
{

// 转场方式。层级下钻（Shell 内容栈）与同级换页（Pager）共用这一套枚举。
enum class ContentTransition
{
    NONE,       // 不做转场，直接落位
    SLIDE,      // 只滑动
    FADE,       // 只淡入淡出
    SLIDE_FADE, // 滑动 + 淡入淡出（默认）
};

class ViewTransition
{
  public:
    // ---- 配置：入场 / 出场分别设 ----
    void setEnter(ContentTransition value, float seconds)
    {
        this->enterTransition = value;
        this->enterDuration   = seconds;
    }
    void setExit(ContentTransition value, float seconds)
    {
        this->exitTransition = value;
        this->exitDuration   = seconds;
    }
    // 两边都设成同一个（旧的"一个开关"语义）
    void setBoth(ContentTransition value, float seconds)
    {
        this->setEnter(value, seconds);
        this->setExit(value, seconds);
    }

    ContentTransition getEnter() const { return this->enterTransition; }
    ContentTransition getExit() const { return this->exitTransition; }
    float getEnterDuration() const { return this->enterDuration; }
    float getExitDuration() const { return this->exitDuration; }

    // ---- 播放 ----
    // width 是"滑多远"，一般传所在层的宽度；FADE 用不到它。
    // appearanceOverride：宿主/测试直接给一份外观（不给就从 view 取）。
    void playEnter(View* view, float width, const Appearance* appearanceOverride = nullptr)
    {
        const TransitionEventSpec* spec = this->resolveSpec(view, TransitionEvent::ENTER, appearanceOverride);
        if (spec != nullptr)
        {
            this->startSpec(Phase::ENTER, view, spec, TransitionEvent::ENTER, nullptr);
            return;
        }

        // 注意：有效时长要在 suspended 之前算出来 —— 配置值是 0 时 suspended 会当成"不动"，
        // 而 0 的语义是"用主题的动画时长"（想彻底不动请用 ContentTransition::NONE）
        const float duration = this->effectiveDuration(this->enterDuration);
        if (this->suspended(view, this->enterTransition, duration))
            return;

        this->start(Phase::ENTER, view, width, duration, nullptr);
    }

    // 显示 / 隐藏：只有"外观里写了"或"默认淡入淡出"两种（滑动对显隐没有意义）。
    // 隐藏播完的落位（比如真的设成 GONE / 摘掉）由调用方在回调里做 ——
    // 提前设 GONE 会让动画根本播不出来。
    void playShow(View* view, const Appearance* appearanceOverride = nullptr)
    {
        const TransitionEventSpec* spec = this->resolveSpec(view, TransitionEvent::SHOW, appearanceOverride);
        if (spec != nullptr)
        {
            this->startSpec(Phase::SHOW, view, spec, TransitionEvent::SHOW, nullptr);
            return;
        }
        this->startSpec(Phase::SHOW, view, nullptr, TransitionEvent::SHOW, nullptr);
    }

    void playHide(View* view, std::function<void()> onFinished = nullptr,
        const Appearance* appearanceOverride = nullptr)
    {
        const TransitionEventSpec* spec = this->resolveSpec(view, TransitionEvent::HIDE, appearanceOverride);
        this->startSpec(Phase::HIDE, view, spec, TransitionEvent::HIDE, std::move(onFinished));
    }

    // 出场。onFinished 在这段动画播完（状态已清干净）后调用；立刻落位的场景同步调用。
    void playExit(View* view, float width, std::function<void()> onFinished = nullptr,
        const Appearance* appearanceOverride = nullptr)
    {
        const TransitionEventSpec* spec = this->resolveSpec(view, TransitionEvent::EXIT, appearanceOverride);
        if (spec != nullptr)
        {
            this->startSpec(Phase::EXIT, view, spec, TransitionEvent::EXIT, std::move(onFinished));
            return;
        }

        const float duration = this->effectiveDuration(this->exitDuration);
        if (this->suspended(view, this->exitTransition, duration))
        {
            if (onFinished)
                onFinished();
            return;
        }

        this->start(Phase::EXIT, view, width, duration, std::move(onFinished));
    }

    // 推进（秒）。出场播完会调用回调。
    void step(float seconds)
    {
        if (this->phase == Phase::NONE)
            return;

        if (this->specDriven)
        {
            this->player.step(seconds);
            this->progress = this->activeDuration > 0.0f
                ? std::min(1.0f, this->progress + (seconds / this->activeDuration))
                : 1.0f;
            if (!this->player.isActive())
                this->finalize();
            return;
        }

        this->anim.update(seconds);

        const float progress = (float)this->anim;
        this->progress       = progress;
        this->apply(progress);

        if (progress >= 1.0f)
            this->finalize();
    }

    // 立刻落到终态：把正在跑的这段收尾（含跑掉出场回调）。
    // 回调里可能又起一段（"出场完接着入场"），所以循环到彻底空闲为止。
    void finishNow()
    {
        int guard = 0;
        while (this->phase != Phase::NONE && guard++ < 8)
        {
            if (this->specDriven)
                this->player.finishNow();
            else
                this->apply(1.0f);
            this->finalize();
        }
    }

    // ---- 查询 ----
    bool isEntering() const { return this->phase == Phase::ENTER; }
    bool isExiting() const { return this->phase == Phase::EXIT; }
    bool isActive() const { return this->phase != Phase::NONE; }
    View* getTarget() const { return this->target; }
    // 正在出场的那一个（入场期间是 nullptr）
    View* getOutgoing() const { return this->phase == Phase::EXIT ? this->target : nullptr; }
    // 1 = 没有转场在跑
    float getProgress() const { return this->progress; }

    // 这一次实际会播多久（秒）：外观里写了 transitions 就是 spec 的总时长，
    // 否则是配置的预设时长（0 = 用主题 metric.anim.normal）。
    // **Shell / Pager 的"出场播完才摘页"要靠它**，别只看配置值。
    float getActiveDuration() const { return this->activeDuration; }
    bool isSpecDriven() const { return this->specDriven; }

    // 直接播一次：给"不经过 Shell/Pager 的宿主"用
    // （先 setEnter/setExit 配好，再按需 playEnter/playExit）

  private:
    enum class Phase
    {
        NONE,
        ENTER,
        EXIT,
        SHOW,
        HIDE,
    };

    // 配置时长为 0 表示"用主题的动画时长"（想彻底不动请用 ContentTransition::NONE）
    float effectiveDuration(float configured) const
    {
        if (configured > 0.0f)
            return configured;
        return getTheme().number("metric.anim.normal", 150.0f) * 0.001f;
    }

    // 外观里有没有写这个事件；没写返回 nullptr（调用方走默认淡入淡出）
    const TransitionEventSpec* resolveSpec(
        View* view, TransitionEvent event, const Appearance* appearanceOverride) const
    {
        if (view == nullptr)
            return nullptr;

        const Appearance* appearance
            = appearanceOverride != nullptr ? appearanceOverride : view->getTransitionAppearance();
        if (appearance == nullptr)
            return nullptr;

        const TransitionEventSpec& spec = appearance->getTransition(event);
        return spec.defined ? &spec : nullptr;
    }

    // 用外观里的 spec 起一段（view 级通道 + 层内通道都由 TransitionPlayer 驱动）
    void startSpec(Phase phase, View* view, const TransitionEventSpec* spec, TransitionEvent event,
        std::function<void()> done)
    {
        this->phase        = phase;
        this->target       = view;
        this->done         = std::move(done);
        this->specDriven   = true;
        this->width        = 0.0f;
        this->progress     = 0.0f;

        AppearanceInstance* instance
            = view != nullptr ? view->getTransitionAppearanceInstance() : nullptr;
        const float fallbackMs = getTheme().number("metric.anim.normal", 150.0f);
        this->player.play(*view, instance, spec, event, fallbackMs);
        this->activeDuration = this->player.getTotalSeconds();

        if (!this->player.isActive())
        {
            // duration: 0 = 瞬变：落位后立刻收尾（回调照跑，调用方的"播完做什么"不受影响）
            this->finalize();
            return;
        }

        this->progress = 0.0f;
    }

    void resetTarget()
    {
        this->target->resetTransform();
        this->target->setAlpha(1.0f);
    }

    // 不需要动画（没目标 / NONE / 时长 <= 0）：立刻把视图摆正，返回 true 表示调用方别再起动画
    bool suspended(View* view, ContentTransition transition, float duration)
    {
        if (view == nullptr || transition == ContentTransition::NONE || duration <= 0.0f)
        {
            if (view != nullptr)
            {
                view->resetTransform();
                view->setAlpha(1.0f);
            }
            this->phase          = Phase::NONE;
            this->target         = nullptr;
            this->done           = nullptr;
            this->progress       = 1.0f;
            this->specDriven     = false;
            this->activeDuration = 0.0f;
            return true;
        }
        return false;
    }

    void start(Phase phase, View* view, float width, float duration, std::function<void()> done)
    {
        this->phase          = phase;
        this->target         = view;
        this->done           = std::move(done);
        this->width          = width;
        this->progress       = 0.0f;
        this->specDriven     = false;
        this->activeDuration = duration;

        this->anim.stop();
        this->anim.reset(0.0f);
        this->anim.addStep(1.0f, duration, EasingFunction::CUBIC_OUT);
        this->anim.start();

        this->apply(0.0f);
    }

    void apply(float progress)
    {
        if (this->target == nullptr)
            return;

        const ContentTransition mode = this->phase == Phase::EXIT ? this->exitTransition
                                                                 : this->enterTransition;
        const bool slide = mode == ContentTransition::SLIDE || mode == ContentTransition::SLIDE_FADE;
        const bool fade  = mode == ContentTransition::FADE || mode == ContentTransition::SLIDE_FADE;

        if (slide)
        {
            const float offset = (1.0f - progress) * this->width;
            this->target->setTranslateX(this->phase == Phase::EXIT ? -progress * this->width : offset);
        }
        else
        {
            this->target->setTranslateX(0.0f);
        }

        if (fade)
            this->target->setAlpha(this->phase == Phase::EXIT ? 1.0f - progress : progress);
        else
            this->target->setAlpha(1.0f);
    }

    void finalize()
    {
        // spec 路径不 reset：终值是外观里写的（比如出场结束停在 -width / alpha 0），
        // 调用方在回调里摘页/摘弹层；默认预设路径才需要把视图摆回原样。
        if (this->target != nullptr && !this->specDriven)
        {
            this->target->resetTransform();
            this->target->setAlpha(1.0f);
        }

        // **先清状态再跑回调**：回调里常常会接着 playEnter()，那时不能再看到旧状态
        std::function<void()> callback = std::move(this->done);
        this->done                     = nullptr;
        this->target                   = nullptr;
        this->phase                    = Phase::NONE;
        this->progress                 = 1.0f;
        this->specDriven               = false;
        this->activeDuration           = 0.0f;

        if (callback)
            callback();
    }

    Phase phase = Phase::NONE;
    View* target = nullptr;
    std::function<void()> done;

    // 默认淡入淡出（用户要的语义：外观和宿主都没说怎么动，就淡一下）
    ContentTransition enterTransition = ContentTransition::FADE;
    ContentTransition exitTransition  = ContentTransition::FADE;
    float enterDuration = 0.0f; // 秒；0 = 用主题 metric.anim.normal（想彻底不动用 NONE）
    float exitDuration  = 0.0f;
    float width         = 0.0f;

    Animatable anim{ 1.0f };
    float progress = 1.0f;

    TransitionPlayer player;      // 外观里写了 transitions 时用它播
    bool specDriven      = false;
    float activeDuration = 0.0f;  // 本次实际时长（秒）
};

} // namespace bklv

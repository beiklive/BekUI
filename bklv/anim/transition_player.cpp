#include "bklv/anim/transition_player.hpp"

#include <algorithm>
#include <cmath>

#include "bklv/theme/attributes.hpp" // enumMatches

namespace bklv
{

namespace
{

// 外观 JSON 里写的缓动名 → 枚举。拼错时退化成线性（解析期不拦这个 —— 它只是观感差异，
// 和"层 id 不存在"这类结构性错误不同）。
EasingFunction easingFromName(const std::string& name)
{
    if (enumMatches(name, "quadraticIn"))
        return EasingFunction::QUADRATIC_IN;
    if (enumMatches(name, "quadraticOut"))
        return EasingFunction::QUADRATIC_OUT;
    if (enumMatches(name, "quadraticInOut"))
        return EasingFunction::QUADRATIC_IN_OUT;
    if (enumMatches(name, "cubicIn"))
        return EasingFunction::CUBIC_IN;
    if (enumMatches(name, "cubicOut"))
        return EasingFunction::CUBIC_OUT;
    if (enumMatches(name, "cubicInOut"))
        return EasingFunction::CUBIC_IN_OUT;
    if (enumMatches(name, "quarticIn"))
        return EasingFunction::QUARTIC_IN;
    if (enumMatches(name, "quarticOut"))
        return EasingFunction::QUARTIC_OUT;
    if (enumMatches(name, "quarticInOut"))
        return EasingFunction::QUARTIC_IN_OUT;
    if (enumMatches(name, "quinticIn"))
        return EasingFunction::QUINTIC_IN;
    if (enumMatches(name, "quinticOut"))
        return EasingFunction::QUINTIC_OUT;
    if (enumMatches(name, "quinticInOut"))
        return EasingFunction::QUINTIC_IN_OUT;
    if (enumMatches(name, "sineIn"))
        return EasingFunction::SINE_IN;
    if (enumMatches(name, "sineOut"))
        return EasingFunction::SINE_OUT;
    if (enumMatches(name, "sineInOut"))
        return EasingFunction::SINE_IN_OUT;
    return EasingFunction::LINEAR;
}

// 一条通道在 elapsedMs 时刻的进度（0..1，已过缓动）
float progressOf(float elapsedMs, float delayMs, float durationMs, EasingFunction easing)
{
    if (durationMs <= 0.0f)
        return 1.0f;

    float t = (elapsedMs - delayMs) / durationMs;
    t       = std::min(1.0f, std::max(0.0f, t));
    return applyEasing(easing, t);
}

// 颜色在 AppearanceInstance 里按 **0..255** 存（和 splitColor / joinColor 同一套标度），
// 所以插值也在这个标度上做，别写成 0..1（写错了会在过渡开头跳一下）。
void splitColorTo255(ImU32 color, float out[4])
{
    out[0] = (float)(color & 0xFFu);
    out[1] = (float)((color >> 8) & 0xFFu);
    out[2] = (float)((color >> 16) & 0xFFu);
    out[3] = (float)((color >> 24) & 0xFFu);
}

} // namespace

float TransitionPlayer::currentOf(ViewChannel channel) const
{
    if (this->view == nullptr)
        return 0.0f;

    const Transform2D& transform = this->view->getTransform();
    switch (channel)
    {
        case ViewChannel::TRANSLATE_X: return transform.translateX;
        case ViewChannel::TRANSLATE_Y: return transform.translateY;
        case ViewChannel::SCALE_X: return transform.scaleX;
        case ViewChannel::SCALE_Y: return transform.scaleY;
        case ViewChannel::ALPHA: return this->view->getAlpha();
        default: return 0.0f;
    }
}

bool TransitionPlayer::play(View& view, AppearanceInstance* appearance,
    const TransitionEventSpec* spec, TransitionEvent event, float fallbackDurationMs)
{
    this->view        = &view;
    this->appearance  = appearance;
    this->channels.clear();
    this->layers.clear();
    this->elapsedMs = 0.0f;
    this->defaultFade = (spec == nullptr || !spec->defined);

    const bool appearing
        = (event == TransitionEvent::ENTER || event == TransitionEvent::SHOW);

    if (this->defaultFade)
    {
        // 没写 transitions：淡入淡出（入场 0→1，出场 1→0）
        ChannelClip clip;
        clip.channel    = ViewChannel::ALPHA;
        clip.from       = appearing ? 0.0f : 1.0f;
        clip.to         = appearing ? 1.0f : 0.0f;
        clip.durationMs = fallbackDurationMs;
        clip.easing     = EasingFunction::QUADRATIC_OUT;
        this->channels.push_back(clip);

        this->totalMs = fallbackDurationMs;

        // 默认路径下顺手把位移/缩放归零：避免上一次过渡留下的偏移粘住
        view.setTranslateX(0.0f);
        view.setTranslateY(0.0f);
    }
    else
    {
        for (const ViewChannelClip& clip : spec->viewClips)
        {
            ChannelClip state;
            state.channel    = clip.channel;
            state.from       = clip.hasSource ? clip.source : this->currentOf(clip.channel);
            state.to         = clip.target;
            state.delayMs    = clip.delayMs;
            state.durationMs = clip.durationMs;
            state.easing     = easingFromName(clip.easing);
            this->channels.push_back(state);
        }

        if (appearance != nullptr)
        {
            const Appearance& source = appearance->getAppearance();
            for (const AppearanceTransition& clip : spec->layerClips)
            {
                const int layerIndex = source.indexOfLayer(clip.layerId);
                if (layerIndex < 0)
                    continue;

                LayerClip state;
                state.layer      = layerIndex;
                state.prop       = clip.prop;
                state.isColor    = appearancePropIsColor(clip.prop);
                state.delayMs    = clip.delayMs;
                state.durationMs = clip.durationMs;
                state.easing     = easingFromName(clip.easing);

                if (clip.hasSource)
                {
                    for (int i = 0; i < 4; i++)
                        state.from[i] = clip.source[i];
                }
                else if (state.isColor)
                {
                    splitColorTo255(appearance->getColor(layerIndex, clip.prop), state.from);
                }
                else
                {
                    state.from[0] = appearance->getNumber(layerIndex, clip.prop);
                }

                for (int i = 0; i < 4; i++)
                    state.to[i] = clip.target[i];

                // 接管这条通道：状态动效和过渡不能同时对同一个属性下手
                appearance->cancelTween(layerIndex, clip.prop);
                this->layers.push_back(state);
            }
        }

        this->totalMs = spec->totalMs;
    }

    // `duration: 0`（或没有任何条目）= 瞬变：立刻落位，不进动画状态
    if (this->totalMs <= 0.0f)
    {
        this->active = false;
        this->applyAt(0.0f);
        return false;
    }

    this->active = true;
    this->applyAt(0.0f);
    return true;
}

void TransitionPlayer::step(float dtSeconds)
{
    if (!this->active)
        return;

    this->elapsedMs += dtSeconds * 1000.0f;
    if (this->elapsedMs >= this->totalMs)
    {
        this->applyAt(this->totalMs);
        this->active = false;
        return;
    }

    this->applyAt(this->elapsedMs);
}

void TransitionPlayer::finishNow()
{
    if (!this->active)
        return;

    this->applyAt(this->totalMs);
    this->active = false;
}

void TransitionPlayer::applyAt(float elapsedMs)
{
    if (this->view == nullptr)
        return;

    for (const ChannelClip& clip : this->channels)
    {
        const float t     = progressOf(elapsedMs, clip.delayMs, clip.durationMs, clip.easing);
        const float value = clip.from + (clip.to - clip.from) * t;

        switch (clip.channel)
        {
            case ViewChannel::TRANSLATE_X: this->view->setTranslateX(value); break;
            case ViewChannel::TRANSLATE_Y: this->view->setTranslateY(value); break;
            case ViewChannel::SCALE_X: this->view->setScaleX(value); break;
            case ViewChannel::SCALE_Y: this->view->setScaleY(value); break;
            case ViewChannel::ALPHA: this->view->setAlpha(value); break;
            default: break;
        }
    }

    if (this->appearance == nullptr)
        return;

    for (const LayerClip& clip : this->layers)
    {
        const float t = progressOf(elapsedMs, clip.delayMs, clip.durationMs, clip.easing);
        if (clip.isColor)
        {
            float rgba[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            for (int i = 0; i < 4; i++)
                rgba[i] = clip.from[i] + (clip.to[i] - clip.from[i]) * t;
            this->appearance->setColor(clip.layer, clip.prop, rgba);
        }
        else
        {
            this->appearance->setNumber(
                clip.layer, clip.prop, clip.from[0] + (clip.to[0] - clip.from[0]) * t);
        }
    }
}

} // namespace bklv

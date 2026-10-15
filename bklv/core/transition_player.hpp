#pragma once

// 视图过渡的**单时间轴播放器**。
//
// 外观 JSON 里的 `transitions` 解析成 `TransitionEventSpec`（`core/appearance.hpp`），
// 这个类负责把它播出来：
//   · **视图级通道**（`translateX/Y`、`scaleX/Y`、`alpha`）直接写 `View` 的绘制期变换
//     —— 整棵子树一起动，过渡期间零重排；
//   · **层内通道**写进 `AppearanceInstance`（和 `states` 同一批属性、同一份值存储），
//     开始前先 `cancelTween()` 接管这条通道，避免和状态动效互相覆盖。
//
// 一条时间轴：所有通道都从事件开始计时，各自 `delay + duration`，
// 事件总时长 = `spec.totalMs`（解析期算好的 max）—— 调用方拿 `getTotalSeconds()`
// 决定"这段动画播完了"，出场播完才摘页 / 摘弹层。
//
// 可以无窗口测：`play()` + `step()` 是纯逻辑，不碰 ImGui。

#include <vector>

#include "bklv/core/appearance.hpp"
#include "bklv/core/time.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

class TransitionPlayer
{
  public:
    // 开始播一个事件。
    //   · spec 为空或没写这个事件 → **默认淡入淡出**（入场/显示 0→1，出场/隐藏 1→0），
    //     时长用 fallbackDurationMs（调用方给主题的 metric.anim.normal）；
    //   · appearance 可以为空（那就只驱动视图级通道）。
    // 返回是否真的在播（`duration: 0` / 总时长为 0 时**立刻落位**并返回 false）。
    bool play(View& view, AppearanceInstance* appearance, const TransitionEventSpec* spec,
        TransitionEvent event, float fallbackDurationMs);

    void step(float dtSeconds);

    // 立刻落到终态（换页中途要立刻落位时用）
    void finishNow();

    bool isActive() const { return this->active; }
    float getTotalSeconds() const { return this->totalMs * 0.001f; }
    bool isDefaultFade() const { return this->defaultFade; }
    // 外观里写了这个事件（而不是走了默认淡入淡出）
    bool hasCustomSpec() const { return !this->defaultFade; }

  private:
    struct ChannelClip
    {
        ViewChannel channel = ViewChannel::ALPHA;
        float from          = 0.0f;
        float to            = 0.0f;
        float delayMs       = 0.0f;
        float durationMs    = 0.0f;
        EasingFunction easing = EasingFunction::LINEAR;
    };

    struct LayerClip
    {
        int layer         = -1;
        AppearanceProp prop = AppearanceProp::OPACITY;
        bool isColor      = false;
        float from[4]     = { 0.0f, 0.0f, 0.0f, 0.0f };
        float to[4]       = { 0.0f, 0.0f, 0.0f, 0.0f };
        float delayMs     = 0.0f;
        float durationMs  = 0.0f;
        EasingFunction easing = EasingFunction::LINEAR;
    };

    void applyAt(float elapsedMs);
    float currentOf(ViewChannel channel) const;

    View* view                     = nullptr;
    AppearanceInstance* appearance = nullptr;
    std::vector<ChannelClip> channels;
    std::vector<LayerClip> layers;

    float totalMs   = 0.0f;
    float elapsedMs = 0.0f;
    bool active     = false;
    bool defaultFade = false;
};

} // namespace bklv

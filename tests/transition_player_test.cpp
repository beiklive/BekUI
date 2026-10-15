// 无窗口测试：视图过渡的**单时间轴播放器**（core/transition_player.*）。
//
// 播放器只做"按时间给通道赋值"，不碰 ImGui，所以能确定性地逐步进：
// `play()` → `step(dt)` → 读 View 的变换/透明度或 AppearanceInstance 的当前值。

#include <cstdio>
#include <memory>
#include <string>

#include "bklv/core/appearance.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/transition_player.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

const char* kLayers = R"("layers": [
    { "op": "rect", "id": "bg", "fill": "#FF0000FF" },
    { "op": "text", "id": "title", "source": "text", "color": "#FFFFFFFF" }
])";

std::shared_ptr<const Appearance> compileAppearance(const std::string& extra, std::string& error)
{
    return Appearance::compile(std::string("{") + kLayers + "," + extra + "}", getTheme(), error);
}

// 推进到指定毫秒（按 60fps 的固定步长，确定性）
void advance(TransitionPlayer& player, float milliseconds, float stepMs = 16.0f)
{
    float left = milliseconds;
    while (left > 0.0f)
    {
        const float dt = left < stepMs ? left : stepMs;
        player.step(dt / 1000.0f);
        left -= dt;
    }
}

} // namespace

int main()
{
    std::printf("== 1. 外观没写 transitions：默认淡入淡出 ==\n");
    {
        Box view;
        TransitionPlayer player;
        const bool playing = player.play(view, nullptr, nullptr, TransitionEvent::ENTER, 150.0f);
        check(playing, "默认路径会播");
        check(player.isDefaultFade(), "标记成默认淡入淡出");
        checkFloat(player.getTotalSeconds(), 0.15f, "总时长用调用方给的兜底值");
        checkFloat(view.getAlpha(), 0.0f, "起点是全透明");

        advance(player, 75.0f);
        check(view.getAlpha() > 0.3f && view.getAlpha() < 0.9f, "中途已经开始显形");

        advance(player, 120.0f);
        check(!player.isActive(), "播完就停了");
        checkFloat(view.getAlpha(), 1.0f, "终点不透明");
    }

    std::printf("\n== 2. 自定义视图级通道（整树一起动）==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance = compileAppearance(R"(
            "transitions": { "enter": {
                "duration": 200,
                "translateX": { "from": -60, "to": 0 },
                "alpha":      { "from": 0, "to": 1 }
            } })", error);
        check(appearance != nullptr, "spec 能编译");

        Box view;
        TransitionPlayer player;
        player.play(view, nullptr, appearance != nullptr ? &appearance->getTransition(TransitionEvent::ENTER) : nullptr,
            TransitionEvent::ENTER, 150.0f);
        check(!player.isDefaultFade(), "用的是外观里的 spec");
        checkFloat(player.getTotalSeconds(), 0.2f, "总时长 = spec.totalMs");
        checkFloat(view.getTransform().translateX, -60.0f, "起点位移");
        checkFloat(view.getAlpha(), 0.0f, "起点透明");

        advance(player, 300.0f);
        checkFloat(view.getTransform().translateX, 0.0f, "终点位移归零");
        checkFloat(view.getAlpha(), 1.0f, "终点不透明");
    }

    std::printf("\n== 3. 层内通道 + when: after 的延迟 ==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance = compileAppearance(R"(
            "transitions": { "enter": {
                "duration": 200,
                "alpha": { "from": 0, "to": 1 },
                "layers": { "title": { "when": "after", "duration": 100,
                                       "opacity": { "from": 0, "to": 1 } } }
            } })", error);
        check(appearance != nullptr, "spec 能编译");
        if (appearance == nullptr)
            return 1;

        AppearanceInstance instance;
        instance.setAppearance(appearance);

        Box view;
        TransitionPlayer player;
        player.play(view, &instance, &appearance->getTransition(TransitionEvent::ENTER), TransitionEvent::ENTER, 150.0f);
        const int titleLayer = appearance->indexOfLayer("title");
        check(titleLayer >= 0, "找到 title 层");
        checkFloat(instance.getNumber(titleLayer, AppearanceProp::OPACITY), 0.0f, "起点透明");

        advance(player, 150.0f); // 视图级已播完、层内还没开始（after = 200ms 起）
        checkFloat(instance.getNumber(titleLayer, AppearanceProp::OPACITY), 0.0f, "after 段还没开始时不提前动");

        advance(player, 200.0f);
        checkFloat(instance.getNumber(titleLayer, AppearanceProp::OPACITY), 1.0f, "after 段播完到终点");
        checkFloat(player.getTotalSeconds(), 0.3f, "总时长 = 视图级 200 + 层内 100");
    }

    std::printf("\n== 4. 出场：没写 from 就用当前值 ==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance = compileAppearance(R"(
            "transitions": { "exit": { "duration": 100, "alpha": { "to": 0 } } })", error);
        check(appearance != nullptr, "spec 能编译");
        if (appearance == nullptr)
            return 1;

        Box view;
        view.setAlpha(0.5f);

        TransitionPlayer player;
        player.play(view, nullptr, &appearance->getTransition(TransitionEvent::EXIT), TransitionEvent::EXIT, 150.0f);
        checkFloat(view.getAlpha(), 0.5f, "起点取当前值（没写 from）");

        advance(player, 200.0f);
        checkFloat(view.getAlpha(), 0.0f, "终点是写的 to");
    }

    std::printf("\n== 5. duration: 0 = 瞬变 ==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance = compileAppearance(R"(
            "transitions": { "hide": { "duration": 0, "alpha": { "from": 1, "to": 0 } } })", error);
        check(appearance != nullptr, "spec 能编译");
        if (appearance == nullptr)
            return 1;

        Box view;
        view.setAlpha(1.0f);
        TransitionPlayer player;
        const bool playing = player.play(view, nullptr, &appearance->getTransition(TransitionEvent::HIDE),
            TransitionEvent::HIDE, 150.0f);
        check(!playing, "瞬变不进动画状态");
        check(!player.isActive(), "立刻就不活跃");
        checkFloat(view.getAlpha(), 0.0f, "值直接落到终点");
    }

    std::printf("\n== 6. 显隐（show / hide）走同一套 ==\n");
    {
        Box view;
        view.setAlpha(1.0f);
        TransitionPlayer player;
        player.play(view, nullptr, nullptr, TransitionEvent::HIDE, 150.0f);
        checkFloat(view.getAlpha(), 1.0f, "隐藏起点可见");
        advance(player, 200.0f);
        checkFloat(view.getAlpha(), 0.0f, "隐藏终点不可见");

        player.play(view, nullptr, nullptr, TransitionEvent::SHOW, 150.0f);
        checkFloat(view.getAlpha(), 0.0f, "显示从透明开始");
        advance(player, 200.0f);
        checkFloat(view.getAlpha(), 1.0f, "显示到可见");
    }

    std::printf("\n== 7. 颜色通道按 0..255 插值 ==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance = compileAppearance(R"(
            "transitions": { "show": { "duration": 100,
                "layers": { "bg": { "fill": { "from": "#00000000", "to": "#FFFFFFFF" } } } } })", error);
        check(appearance != nullptr, "spec 能编译");
        if (appearance == nullptr)
            return 1;

        AppearanceInstance instance;
        instance.setAppearance(appearance);
        const int bgLayer = appearance->indexOfLayer("bg");
        check(bgLayer >= 0, "找到 bg 层");

        Box view;
        TransitionPlayer player;
        player.play(view, &instance, &appearance->getTransition(TransitionEvent::SHOW), TransitionEvent::SHOW, 150.0f);
        checkEqualInt((int)((instance.getColor(bgLayer, AppearanceProp::FILL) >> 24) & 0xFFu), 0, "起点全透明");

        advance(player, 200.0f);
        checkEqualInt((int)((instance.getColor(bgLayer, AppearanceProp::FILL) >> 24) & 0xFFu), 255, "终点不透明");
    }

    std::printf("\n== 8. 过渡接管通道：状态补间被撤掉 ==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance = compileAppearance(R"(
            "states": { "focused": { "title.opacity": { "to": 0.2, "duration": 200 } } },
            "transitions": { "enter": { "duration": 100,
                "layers": { "title": { "opacity": { "from": 1, "to": 1 } } } } })", error);
        check(appearance != nullptr, "spec 能编译");
        if (appearance == nullptr)
            return 1;

        AppearanceInstance instance;
        instance.setAppearance(appearance);
        instance.setState("focused", true);
        instance.update(0.05f);
        check(instance.isAnimating(), "状态补间在跑");

        Box view;
        TransitionPlayer player;
        player.play(view, &instance, &appearance->getTransition(TransitionEvent::ENTER), TransitionEvent::ENTER, 150.0f);
        check(!instance.isAnimating(), "过渡开始时撤掉了同通道的状态补间");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

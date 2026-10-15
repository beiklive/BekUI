// 无窗口测试：视图过渡（appearance JSON 的 `transitions` 段）解析。
//
// 这里钉住的是**解析期就该算对的东西**：四个事件、视图级通道、层内通道、
// `when: "after"` 的延迟补偿、事件总时长、默认淡入淡出、以及五类写错要报错。
// 运行时（真的动起来）由 transition_* 的宿主测试覆盖。

#include <cstdio>
#include <string>

#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 包一层：把 layers 和 transitions 拼成一份完整外观 JSON
std::string appearanceJson(const std::string& transitions)
{
    return "{ \"layers\": ["
           "  { \"op\": \"rect\", \"id\": \"bg\", \"fill\": \"#101010FF\" },"
           "  { \"op\": \"text\", \"id\": \"title\", \"source\": \"text\", \"color\": \"#FFFFFFFF\" }"
           "], \"transitions\": " + transitions + " }";
}

// JSON 对象的 key 是按字母序迭代的（nlohmann 用 std::map），**不能按下标断言**，
// 所以测试一律按通道名 / 层 id 查找。
const ViewChannelClip* findViewClip(const TransitionEventSpec& spec, ViewChannel channel)
{
    for (const ViewChannelClip& clip : spec.viewClips)
        if (clip.channel == channel)
            return &clip;
    return nullptr;
}

const AppearanceTransition* findLayerClip(const TransitionEventSpec& spec, const std::string& layerId,
    AppearanceProp prop)
{
    for (const AppearanceTransition& clip : spec.layerClips)
        if (clip.layerId == layerId && clip.prop == prop)
            return &clip;
    return nullptr;
}

std::shared_ptr<const Appearance> compileOrNull(const std::string& transitions, std::string& error)
{
    return Appearance::compile(appearanceJson(transitions), getTheme(), error);
}

float defaultEventDuration()
{
    return getTheme().number("metric.anim.normal", 150.0f);
}

} // namespace

int main()
{
    std::printf("== 1. 没写 transitions：不算定义（向后兼容）==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance = Appearance::compile(
            R"({"layers": [{"op": "rect", "id": "bg", "fill": "#101010FF"}]})", getTheme(), error);
        check(appearance != nullptr, "只写 layers 的外观照旧能编译");
        if (appearance != nullptr)
        {
            check(!appearance->hasTransitions(), "没写 transitions 时 hasTransitions() 为 false");
            check(!appearance->getTransition(TransitionEvent::ENTER).defined, "enter 未定义");
            check(!appearance->getTransition(TransitionEvent::HIDE).defined, "hide 未定义");
        }
    }

    std::printf("\n== 2. 视图级通道 + 层内通道 + when: after ==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance = compileOrNull(R"({
            "enter": {
                "duration": 200, "easing": "quadraticOut",
                "translateX": { "from": -60, "to": 0 },
                "alpha":      { "from": 0, "to": 1 },
                "layers": {
                    "title": { "when": "after", "duration": 140,
                               "offsetX": { "from": -20, "to": 0 },
                               "opacity": { "from": 0, "to": 1 } }
                }
            }
        })", error);
        check(appearance != nullptr, "完整 enter 能编译");
        if (appearance != nullptr)
        {
            const TransitionEventSpec& enter = appearance->getTransition(TransitionEvent::ENTER);
            check(enter.defined, "enter 被记下来了");
            checkEqualInt((int)enter.viewClips.size(), 2, "两条视图级通道");
            checkEqualInt((int)enter.layerClips.size(), 2, "层内两条通道");
            checkFloat(enter.durationMs, 200.0f, "事件默认时长");
            check(enter.easing == "quadraticOut", "事件默认缓动");

            const ViewChannelClip* move = findViewClip(enter, ViewChannel::TRANSLATE_X);
            check(move != nullptr, "有 translateX 通道");
            if (move != nullptr)
            {
                check(move->hasSource, "写了 from 就有起点");
                checkFloat(move->source, -60.0f, "translateX 起点");
                checkFloat(move->target, 0.0f, "translateX 终点");
            }

            const ViewChannelClip* fade = findViewClip(enter, ViewChannel::ALPHA);
            check(fade != nullptr, "有 alpha 通道");
            if (fade != nullptr)
            {
                checkFloat(fade->source, 0.0f, "alpha 起点");
                checkFloat(fade->target, 1.0f, "alpha 终点");
            }

            checkFloat(enter.viewTotalMs, 200.0f, "视图级总时长 = 200");
            // when: after 的核心：层内条目的 delay 自动补上视图级总时长
            const AppearanceTransition* title
                = findLayerClip(enter, "title", AppearanceProp::OFFSET_X);
            check(title != nullptr, "有 title.offsetX 通道");
            if (title != nullptr)
                checkFloat(title->delayMs, 200.0f, "when:after 的层内条目延后 200ms");
            checkFloat(enter.totalMs, 340.0f, "事件总时长 = 200 + 140");
        }
    }

    std::printf("\n== 3. 默认时长来自主题、duration: 0 = 瞬变 ==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance
            = compileOrNull(R"({"enter": { "alpha": { "from": 0, "to": 1 } }})", error);
        check(appearance != nullptr, "只写一条 alpha 能编译");
        if (appearance != nullptr)
        {
            const TransitionEventSpec& enter = appearance->getTransition(TransitionEvent::ENTER);
            checkFloat(enter.durationMs, defaultEventDuration(), "没写 duration 时用主题的动画时长");
            checkFloat(enter.totalMs, defaultEventDuration(), "总时长跟着走");
        }

        error.clear();
        const std::shared_ptr<const Appearance> instant
            = compileOrNull(R"({"hide": { "duration": 0, "alpha": { "from": 1, "to": 0 } }})", error);
        check(instant != nullptr, "duration: 0 是合法写法");
        if (instant != nullptr)
        {
            const TransitionEventSpec& hide = instant->getTransition(TransitionEvent::HIDE);
            checkFloat(hide.durationMs, 0.0f, "duration: 0 记成 0");
            checkFloat(hide.totalMs, 0.0f, "总时长也是 0 = 瞬变");
        }
    }

    std::printf("\n== 4. delay 与 when 的时间账 ==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance = compileOrNull(R"({
            "enter": {
                "duration": 120,
                "alpha":   { "from": 0, "to": 1, "delay": 50, "duration": 200 },
                "layers": {
                    "title": { "offsetX": { "from": -10, "to": 0, "duration": 80 } },
                    "bg":    { "when": "after", "opacity": { "from": 0, "to": 1, "duration": 60 } }
                }
            }
        })", error);
        check(appearance != nullptr, "带 delay 的能编译");
        if (appearance != nullptr)
        {
            const TransitionEventSpec& enter = appearance->getTransition(TransitionEvent::ENTER);
            checkFloat(enter.viewTotalMs, 250.0f, "视图级 = delay 50 + 200");
            const AppearanceTransition* titleClip
                = findLayerClip(enter, "title", AppearanceProp::OFFSET_X);
            const AppearanceTransition* bgClip
                = findLayerClip(enter, "bg", AppearanceProp::OPACITY);
            check(titleClip != nullptr && bgClip != nullptr, "两条层内通道都在");
            if (titleClip != nullptr)
                checkFloat(titleClip->delayMs, 0.0f, "没写 when 的层内条目不延后");
            if (bgClip != nullptr)
                checkFloat(bgClip->delayMs, 250.0f, "when:after 补到视图级结束");
            // 通道对象里写 duration 优先于层条目级 / 事件默认
            if (bgClip != nullptr)
                checkFloat(bgClip->durationMs, 60.0f, "通道对象里的 duration 生效");
            checkFloat(enter.totalMs, 310.0f, "总时长取最大：250 + 60");
        }
    }

    std::printf("\n== 5. 出场/隐藏写 when: after 只警告、按 with 处理 ==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance = compileOrNull(R"({
            "exit": {
                "alpha": { "from": 1, "to": 0 },
                "layers": { "title": { "when": "after", "opacity": { "from": 1, "to": 0 } } }
            }
        })", error);
        check(appearance != nullptr, "出场写 after 不算错误");
        if (appearance != nullptr)
        {
            const TransitionEventSpec& exit = appearance->getTransition(TransitionEvent::EXIT);
            const AppearanceTransition* titleClip
                = findLayerClip(exit, "title", AppearanceProp::OPACITY);
            check(titleClip != nullptr, "层内通道在");
            if (titleClip != nullptr)
                checkFloat(titleClip->delayMs, 0.0f, "被按 with 处理：不额外延后");
        }
    }

    std::printf("\n== 6. 颜色通道与主题引用 ==\n");
    {
        std::string error;
        const std::shared_ptr<const Appearance> appearance = compileOrNull(R"({
            "show": {
                "layers": {
                    "title": { "color": { "from": "@theme/color.common.transparent",
                                          "to": "@theme/color.common.white" },
                               "fill":  { "from": "#00000000", "to": "#FFFFFFFF" } }
                }
            }
        })", error);
        check(appearance != nullptr, "颜色通道能编译（含 @theme 引用）");
        if (appearance != nullptr)
        {
            const TransitionEventSpec& show = appearance->getTransition(TransitionEvent::SHOW);
            checkEqualInt((int)show.layerClips.size(), 2, "两条颜色通道");
            const AppearanceTransition* textClip
                = findLayerClip(show, "title", AppearanceProp::TEXT_COLOR);
            check(textClip != nullptr, "color 通道在");
            if (textClip != nullptr)
            {
                check(textClip->hasSource, "写了 from 就有起点");
                check(textClip->target[3] > 0.5f, "终点是不透明的白");
                checkFloat(textClip->source[3], 0.0f, "起点是全透明");
            }
        }
    }

    std::printf("\n== 7. 写错必须报错（不静默降级）==\n");
    {
        std::string error;

        check(compileOrNull(R"({"slide": { "alpha": { "from": 0, "to": 1 } }})", error) == nullptr,
            "事件名不认识 -> 失败");
        check(error.find("enter") != std::string::npos, "错误里列出了可选事件");

        error.clear();
        check(compileOrNull(R"({"enter": { "type": "slideFade" }})", error) == nullptr,
            "预设写法（type）-> 失败");
        check(error.find("translateX") != std::string::npos, "错误里列出了可选通道");

        error.clear();
        check(compileOrNull(R"({"enter": { "alpha": { "from": 0, "to": 1 },
            "layers": { "title": { "when": "before", "opacity": { "from": 0, "to": 1 } } } }})",
                  error)
                == nullptr,
            "when: before -> 失败");
        check(error.find("delay") != std::string::npos, "错误里提示改用 delay");

        error.clear();
        check(compileOrNull(R"({"enter": { "layers": { "不存在": { "opacity": { "from": 0, "to": 1 } } } }})",
                  error)
                == nullptr,
            "层 id 不存在 -> 失败");
        check(error.find("不存在") != std::string::npos, "错误里带上那个层 id");

        error.clear();
        check(compileOrNull(R"({"enter": { "layers": { "title": { "不认识的属性": { "from": 0, "to": 1 } } } }})",
                  error)
                == nullptr,
            "属性名不认识 -> 失败");

        error.clear();
        check(compileOrNull(R"({"enter": { "alpha": { "from": 0 } }})", error) == nullptr,
            "缺 to -> 失败");
        check(error.find("to") != std::string::npos, "错误里说明了要写 to");

        error.clear();
        check(compileOrNull(R"({"enter": { "duration": -5, "alpha": { "from": 0, "to": 1 } }})", error)
                == nullptr,
            "负数时长 -> 失败");

        error.clear();
        check(compileOrNull(R"({"enter": { "when": "after" }})", error) == nullptr,
            "when 写在事件级（放错位置）-> 失败");
    }

    std::printf("\n== 8. 枚举名大小写与 _/- 都不敏感 ==\n");
    {
        TransitionEvent event = TransitionEvent::ENTER;
        check(transitionEventFromName("EXIT", event) && event == TransitionEvent::EXIT, "EXIT");
        check(transitionEventFromName("show", event) && event == TransitionEvent::SHOW, "show");
        check(!transitionEventFromName("before", event), "before 不是事件名");
        check(std::string(transitionEventName(TransitionEvent::HIDE)) == "hide", "名字反查");

        ViewChannel channel = ViewChannel::ALPHA;
        check(viewChannelFromName("TRANSLATE_X", channel)
                && channel == ViewChannel::TRANSLATE_X,
            "TRANSLATE_X");
        check(viewChannelFromName("translate-x", channel)
                && channel == ViewChannel::TRANSLATE_X,
            "translate-x");
        check(viewChannelFromName("scaleY", channel) && channel == ViewChannel::SCALE_Y, "scaleY");
        check(!viewChannelFromName("rotate", channel), "rotate 不支持（要报错）");
        check(viewChannelIsScaled(ViewChannel::TRANSLATE_X), "位移按 720p 缩放");
        check(!viewChannelIsScaled(ViewChannel::ALPHA), "透明度不缩放");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

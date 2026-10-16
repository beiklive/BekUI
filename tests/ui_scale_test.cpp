// 无窗口测试：UI 缩放（主题尺寸按 1280x720 写，运行时按画布宽度等比缩放）。
//
// 这一层以前没有测试（评审 §4.2 P1「测试覆盖有明显盲区」里点名过 ui_scale），
// 但它有个容易踩的点：**缩放只作用在主题值与外观 JSON 的像素量上**，
// 代码里写死的数字不缩放 —— 而 `ThemeData::number()` 的"兜底值"分支原来也不缩放，
// 于是"主题里有这个 key"和"没有"在非 720p 画布上表现不一致。这里把两类都钉住。

#include <cstdio>

#include "test_util.hpp"

#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/ui_scale.hpp"

using namespace bklv;

int main()
{
    initResources();

    std::printf("\n== 1. 缩放系数按画布宽度算（基准 1280）==\n");
    {
        setUiScale(1.0f);
        checkFloat(getUiScale(), 1.0f, "默认是 1.0");

        // 1280 宽就是基准：不变化，返回 false
        check(!updateUiScaleFromCanvas(1280.0f, 720.0f), "1280x720 不改变缩放（基准）");
        checkFloat(getUiScale(), 1.0f, "基准画布下缩放是 1.0");

        // 2560x1440：宽度翻倍
        check(updateUiScaleFromCanvas(2560.0f, 1440.0f), "2560 宽返回“变了”");
        checkFloat(getUiScale(), 2.0f, "2560 宽 → 缩放 2.0");

        // 再算一次同样的尺寸：应该报"没变"（避免每帧重新套用外观）
        check(!updateUiScaleFromCanvas(2560.0f, 1440.0f), "同一尺寸重复调用返回 false（不重复套用外观）");

        // 只认宽度：高度不同不影响
        check(!updateUiScaleFromCanvas(2560.0f, 100.0f), "只看宽度，高度不影响缩放");

        // 0 / 负数：不合法，保持原值
        check(!updateUiScaleFromCanvas(0.0f, 720.0f), "宽度 0 返回 false");
        checkFloat(getUiScale(), 2.0f, "宽度 0 不改动缩放");
    }

    std::printf("\n== 2. setUiScale 的非法值防御 ==\n");
    {
        setUiScale(1.5f);
        checkFloat(getUiScale(), 1.5f, "正常值生效");
        setUiScale(0.0f);
        checkFloat(getUiScale(), 1.5f, "0 被拒绝（否则整界面尺寸归零）");
        setUiScale(-2.0f);
        checkFloat(getUiScale(), 1.5f, "负数被拒绝");
        setUiScale(1.0f);
    }

    std::printf("\n== 3. 主题数值跟着缩放（metric.* 缩放，非像素量不缩放）==\n");
    {
        setUiScale(1.0f);
        const float baseSpacing = getTheme().number("metric.spacing.md", -1.0f);
        const float baseAnim    = getTheme().number("metric.anim.normal", -1.0f);
        const float baseLine    = getTheme().number("metric.font.lineHeight", -1.0f);
        check(baseSpacing > 0.0f && baseAnim > 0.0f && baseLine > 0.0f, "三个 key 都存在");

        setUiScale(2.0f);
        checkFloat(getTheme().number("metric.spacing.md"), baseSpacing * 2.0f,
            "metric.spacing.md 随缩放（像素量）");
        checkFloat(getTheme().number("metric.anim.normal"), baseAnim,
            "metric.anim.* 不缩放（毫秒不是像素）");
        checkFloat(getTheme().number("metric.font.lineHeight"), baseLine,
            "metric.font.lineHeight 不缩放（是行高系数）");

        // 兜底值也要缩放：这条是本次改的（原来 `return fallback` 不乘缩放）
        const float missingFallback = getTheme().number("metric.no.such.key", 10.0f);
        checkFloat(missingFallback, 20.0f, "主题里没有这个 key 时兜底值同样乘缩放");

        // 非 metric 前缀的兜底不缩放
        checkFloat(getTheme().number("no.such.metric.key", 10.0f), 10.0f,
            "不带 metric. 前缀的 key 兜底不缩放");
        setUiScale(1.0f);
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

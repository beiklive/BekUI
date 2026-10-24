#include "bklv/ui/widgets/debug/fps_overlay.hpp"

#include <cstdio>

#include "bklv/system/media/font.hpp"
#include "bklv/ui/theme/theme_data.hpp"
#include "bklv/foundation/anim/time.hpp"
#include "bklv/foundation/core/ui_scale.hpp"

using namespace bklv;

FpsOverlay::FpsOverlay()
{
    // 右上角浮层：绝对定位，不参与父容器的 flex 排布
    this->setPositionType(PositionType::ABSOLUTE);
    this->setPositionTop(getTheme().number("metric.spacing.sm", 8.0f));
    this->setPositionRight(getTheme().number("metric.spacing.lg", 20.0f));
    this->setWidth(230.0f);
    this->setHeight(56.0f);
}

void FpsOverlay::update()
{
    View::update();

    const float dt = getFrameDeltaSeconds();
    if (dt <= 0.0f)
        return;

    // 启动前 30 帧不采样：首帧/着色器/主题装载的毛刺不该算进 p99。
    // 按帧而不是按秒 —— 截图/压测模式帧间隔极小，按秒会长时间没数据。
    if (this->warmup < 30)
    {
        this->warmup++;
        return;
    }

    this->stats.record((double)dt);

    // 第一帧就给个即时值：不然窗口到期前一直是 0.0（截图/短会话很容易看到）
    if (this->fps <= 0.0f)
        this->fps = 1.0f / dt;
    this->window += dt;
    this->frames++;

    if (this->window < 0.5f)
        return;

    // 统计窗口到期：算一次并复位（窗口计数，vsync 抖动下也比 EMA 稳）
    this->fps     = (float)this->frames / this->window;
    const FrameTimeSummary summary = this->stats.getSummary();
    this->p99Ms   = (float)summary.p99Ms;
    this->worstMs = (float)summary.worstMs;
    this->window  = 0.0f;
    this->frames  = 0;
}

void FpsOverlay::draw(ImDrawList* drawList, float x, float y, float width, float, FrameContext*)
{
    const float fontSize = getTheme().number("metric.font.size.sm", 16.0f);
    const ImU32 color    = getTheme().color("color.text.dim");
    ImFont*     font     = getDefaultFont();
    if (font == nullptr || drawList == nullptr)
        return;

    const float padding = getUiScale() * 4.0f;
    const float lineGap = getUiScale() * 2.0f;

    char line1[64];
    std::snprintf(line1, sizeof(line1), "%.1f FPS / 目标 %d", (double)this->fps,
        (int)(1000.0f / FrameStats::kOneFrameMs + 0.5f));

    char line2[64];
    std::snprintf(line2, sizeof(line2), "p99 %.0fms · 最坏 %.0fms", (double)this->p99Ms,
        (double)this->worstMs);

    const ImVec2 size1 = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, line1);
    const ImVec2 size2 = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, line2);

    const float right = width - padding;
    drawList->AddText(font, fontSize, ImVec2(x + right - size1.x, y), color, line1);
    drawList->AddText(font, fontSize, ImVec2(x + right - size2.x, y + size1.y + lineGap),
        getTheme().color("color.text.disabled"), line2);
}

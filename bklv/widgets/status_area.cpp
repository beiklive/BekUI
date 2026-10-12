#include "bklv/widgets/status_area.hpp"

#include <ctime>

#include "bklv/core/theme_data.hpp"
#include "bklv/core/time.hpp"

#if defined(BKLV_SWITCH)
#include <switch.h>
#endif

namespace bklv
{

namespace
{

// Switch 上取一次电量百分比（拿不到就返回 -1）
int queryBatteryPercent()
{
#if defined(BKLV_SWITCH)
    static bool initialized = false;
    if (!initialized)
    {
        if (R_FAILED(psmInitialize()))
            return -1;
        initialized = true;
    }

    u32 percent = 0;
    if (R_FAILED(psmGetBatteryChargePercentage(&percent)))
        return -1;

    return (int)percent;
#else
    return -1; // 桌面没有电池，想接就自己替换这个函数
#endif
}

} // namespace

StatusArea::StatusArea()
    : Box(Axis::ROW)
{
    this->setHeight(AUTO);
    this->setAlignItems(AlignItems::CENTER);

    this->batteryLabel = new Label();
    this->batteryLabel->setSingleLine(true);
    this->batteryLabel->setVisibility(Visibility::GONE);
    this->addView(this->batteryLabel);

    this->clockLabel = new Label();
    this->clockLabel->setSingleLine(true);
    this->addView(this->clockLabel);

    this->applyTheme();
    this->refresh();
}

void StatusArea::setBatteryVisible(bool visible)
{
    this->batteryLabel->setVisibility(visible ? Visibility::VISIBLE : Visibility::GONE);
}

void StatusArea::applyTheme()
{
    const ThemeData& theme = getTheme();

    // 字号取 borealis 的 hints/time 那一档（21.5），颜色是次级文字色
    for (Label* label : { this->batteryLabel, this->clockLabel })
    {
        label->setFontSize(theme.number("metric.status.fontSize", 21.5f));
        label->setTextColorExplicit(false);
    }

    this->setGap(theme.number("metric.status.itemSpacing", 21.0f));
}

void StatusArea::refresh()
{
    // 时间：本地时间 HH:MM
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif

    char buffer[16] = { 0 };
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", local.tm_hour, local.tm_min);
    if (this->clockText != buffer)
    {
        this->clockText = buffer;
        this->clockLabel->setText(this->clockText);
    }

    // 电量：Switch 上有数据，其他平台一直拿不到就保持隐藏
    const int percent = queryBatteryPercent();
    if (percent != this->batteryPercent)
    {
        this->batteryPercent = percent;
        this->batteryText     = percent >= 0 ? (std::to_string(percent) + "%") : std::string();

        this->batteryLabel->setText(this->batteryText);
        this->batteryLabel->setVisibility(this->batteryText.empty() ? Visibility::GONE
                                                                   : Visibility::VISIBLE);
    }
}

void StatusArea::update()
{
    Box::update();

    // 半秒刷一次就够了（时间最小单位是分钟，电量变化也慢）
    static double accumulator = 0.0;
    accumulator += (double)getFrameDeltaSeconds();
    if (accumulator < 0.5)
        return;

    accumulator = 0.0;
    this->refresh();
}

} // namespace bklv

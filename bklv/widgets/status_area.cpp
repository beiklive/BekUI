#include "bklv/widgets/status_area.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>

#include "bklv/core/theme_data.hpp"
#include "bklv/core/time.hpp"
#include "bklv/core/ui_scale.hpp"

#if defined(BKLV_SWITCH)
#include <switch.h>
#endif

namespace bklv
{

// ---------------------------------------------------------------- 平台数据

StatusSnapshot queryPlatformStatus()
{
    StatusSnapshot snapshot;

#if defined(BKLV_SWITCH)
    // 电量与充电：psm 服务（borealis 走 Platform::getBatteryLevel，我们是显式查询）
    static bool psmReady = false;
    if (!psmReady)
        psmReady = R_SUCCEEDED(psmInitialize());

    if (psmReady)
    {
        u32 percent = 0;
        if (R_SUCCEEDED(psmGetBatteryChargePercentage(&percent)))
            snapshot.batteryPercent = (int)percent;

        PsmChargerType charger = PsmChargerType_Unconnected;
        if (R_SUCCEEDED(psmGetChargerType(&charger)))
            snapshot.charging = charger == PsmChargerType_EnoughPower
                || charger == PsmChargerType_LowPower;
    }

    // 网络：nifm 给"类型 + 信号强度(0~3)"
    static bool nifmReady = false;
    if (!nifmReady)
        nifmReady = R_SUCCEEDED(nifmInitialize(NifmServiceType_User));

    if (nifmReady)
    {
        NifmInternetConnectionType type = NifmInternetConnectionType_WiFi;
        u32 strength                    = 0;
        NifmInternetConnectionStatus status = NifmInternetConnectionStatus_ConnectingUnknown1;
        if (R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &strength, &status)))
        {
            snapshot.ethernet = type == NifmInternetConnectionType_Ethernet;
            if (type == NifmInternetConnectionType_WiFi)
                snapshot.wifiLevel = (int)std::min<u32>(strength, 3u);
            else if (snapshot.ethernet)
                snapshot.wifiLevel = 3;
        }
    }
#else
    // 桌面没有电池/无线硬件。想要数据就 setProvider(...) 注入
    // （也方便测试与演示）。
#endif

    return snapshot;
}

// ---------------------------------------------------------------- 电池图标

BatteryIcon::BatteryIcon()
    : AppearanceView("status_battery", false)
{
    this->enableContentMeasurement();
    this->applyTheme();
}

void BatteryIcon::setSnapshot(const StatusSnapshot& value)
{
    // 没数据就藏起来（桌面）。同上：显隐先定，别被值没变的短路跳过
    this->setVisibility(value.batteryPercent >= 0 ? Visibility::VISIBLE : Visibility::GONE);

    if (this->percent == value.batteryPercent && this->charging == value.charging)
        return;

    this->percent  = value.batteryPercent;
    this->charging = value.charging;
    this->invalidate();
}

void BatteryIcon::applyTheme()
{
    AppearanceView::applyTheme();

    const ThemeData& theme = getTheme();
    this->bodyColor    = theme.color("color.status.batteryBody", IM_COL32(200, 205, 212, 255));
    this->fillColor    = theme.color("color.status.batteryFill", IM_COL32(255, 255, 255, 255));
    this->lowColor     = theme.color("color.status.batteryLow", IM_COL32(255, 92, 92, 255));
    this->chargeColor  = theme.color("color.status.batteryCharging", IM_COL32(0, 255, 204, 255));
    this->lowThreshold = theme.number("metric.status.batteryLowThreshold", 0.2f);
}

void BatteryIcon::drawCustomLayer(ImDrawList* drawList, const AppearanceLayer&, float x, float y,
    float width, float height, const AppearanceInstance&)
{
    if (this->percent < 0 || width <= 0.0f || height <= 0.0f)
        return;

    // 电池：右侧凸起 + 外壳描边 + 内部电量条（全部矢量，任意缩放都清晰）
    const float capWidth  = std::max(2.0f, width * 0.08f);
    const float bodyWidth = width - capWidth;
    const float stroke    = std::max(1.0f, height * 0.08f);
    const float radius    = height * 0.25f;
    const float level     = std::clamp(this->percent / 100.0f, 0.0f, 1.0f);

    drawList->AddRect(ImVec2(x, y), ImVec2(x + bodyWidth, y + height), this->bodyColor, radius,
        ImDrawFlags_RoundCornersAll, stroke);

    // 电量条（留出描边与内边距）
    const float inset    = stroke + std::max(1.0f, height * 0.12f);
    const float innerW   = bodyWidth - inset * 2.0f;
    const float innerH   = height - inset * 2.0f;
    const ImU32 levelColor = this->charging ? this->chargeColor
        : (level <= this->lowThreshold ? this->lowColor : this->fillColor);

    if (innerW > 0.0f && innerH > 0.0f && level > 0.0f)
    {
        drawList->AddRectFilled(ImVec2(x + inset, y + inset),
            ImVec2(x + inset + innerW * level, y + height - inset), levelColor,
            std::max(1.0f, radius * 0.5f));
    }

    // 正极小凸起
    const float capHeight = height * 0.45f;
    drawList->AddRectFilled(ImVec2(x + bodyWidth + stroke * 0.5f, y + (height - capHeight) * 0.5f),
        ImVec2(x + width, y + (height + capHeight) * 0.5f), this->bodyColor, 1.0f);

    // 充电中：画一道闪电
    if (this->charging)
    {
        const float cx = x + bodyWidth * 0.5f;
        const float cy = y + height * 0.5f;
        const float h  = height * 0.32f;
        const float w  = h * 0.45f;
        drawList->AddTriangleFilled(ImVec2(cx - w * 0.5f, cy - h),
            ImVec2(cx + w * 0.5f, cy - h * 0.1f), ImVec2(cx - w * 0.15f, cy + h * 0.1f),
            this->chargeColor);
        drawList->AddTriangleFilled(ImVec2(cx + w * 0.5f, cy + h),
            ImVec2(cx - w * 0.5f, cy + h * 0.1f), ImVec2(cx + w * 0.15f, cy - h * 0.1f),
            this->chargeColor);
    }
}

// ---------------------------------------------------------------- 无线图标

WirelessIcon::WirelessIcon()
    : AppearanceView("status_wireless", false)
{
    this->enableContentMeasurement();
    this->applyTheme();
}

void WirelessIcon::setSnapshot(const StatusSnapshot& value)
{
    // 显隐先定：值和初值相同也要走这一步（否则第一次拿到无数据时会漏掉隐藏）
    const bool hasData = value.wifiLevel >= 0 || value.ethernet;
    this->setVisibility(hasData ? Visibility::VISIBLE : Visibility::GONE);

    if (this->level == value.wifiLevel && this->ethernet == value.ethernet)
        return;

    this->level    = value.wifiLevel;
    this->ethernet = value.ethernet;
    this->invalidate();
}

void WirelessIcon::applyTheme()
{
    AppearanceView::applyTheme();

    const ThemeData& theme = getTheme();
    this->onColor  = theme.color("color.status.wirelessOn", IM_COL32(255, 255, 255, 255));
    this->offColor = theme.color("color.status.wirelessOff", IM_COL32(255, 255, 255, 60));
}

void WirelessIcon::drawCustomLayer(ImDrawList* drawList, const AppearanceLayer&, float x, float y,
    float width, float height, const AppearanceInstance&)
{
    if (width <= 0.0f || height <= 0.0f)
        return;

    if (this->level < 0 && !this->ethernet)
        return;

    const float cx = x + width * 0.5f;

    if (this->ethernet)
    {
        // 以太网：三个小方块 + 顶部连接条
        const float cell = height * 0.26f;
        const float gap  = cell * 0.25f;
        const float baseY = y + height * 0.62f;
        for (int i = 0; i < 3; i++)
        {
            const float bx = x + (width - (cell * 3.0f + gap * 2.0f)) * 0.5f + i * (cell + gap);
            drawList->AddRectFilled(ImVec2(bx, baseY), ImVec2(bx + cell, baseY + cell * 0.8f),
                this->onColor, cell * 0.15f);
        }
        drawList->AddRectFilled(ImVec2(cx - cell * 0.5f, y + height * 0.26f),
            ImVec2(cx + cell * 0.5f, baseY), this->onColor, cell * 0.15f);
        return;
    }

    // WiFi：三段弧 + 底部圆点，按等级点亮（0 级也画，表示"连着但信号最弱"）
    const float baseY   = y + height * 0.78f;
    const float stroke  = std::max(1.5f, height * 0.11f);

    drawList->AddCircleFilled(ImVec2(cx, baseY), stroke * 0.8f, this->onColor, 16);

    for (int i = 0; i < 3; i++)
    {
        const float radius = height * (0.24f + 0.20f * (float)i);
        const ImU32 color  = i <= this->level ? this->onColor : this->offColor;
        const float top    = 3.14159265f * 1.15f;
        const float bottom = 3.14159265f * 1.85f;
        drawList->PathArcTo(ImVec2(cx, baseY), radius, top, bottom, 16);
        drawList->PathStroke(color, 0, stroke);
    }
}

// ---------------------------------------------------------------- 状态区

StatusArea::StatusArea()
    : Box(Axis::ROW)
{
    this->setHeight(AUTO);
    this->setAlignItems(AlignItems::CENTER);
    this->setJustifyContent(JustifyContent::CENTER);
    this->provider = [] { return queryPlatformStatus(); };

    // 分组：电量图标 + 百分比挨在一起，再是网络图标与时钟
    // （对标 borealis 的 brls/hints 里 icon+label 成对的排法）
    this->battery = new BatteryIcon();
    this->addView(this->battery);

    this->batteryLabel = new Label();
    this->batteryLabel->setSingleLine(true);
    this->batteryLabel->setMarginTop(8.f);
    this->addView(this->batteryLabel);

    this->wireless = new WirelessIcon();
    this->addView(this->wireless);

    this->clockLabel = new Label();
    this->clockLabel->setMarginTop(8.f);
    this->clockLabel->setSingleLine(true);
    this->addView(this->clockLabel);

    this->applyTheme();
    this->refresh();
}

void StatusArea::setProvider(std::function<StatusSnapshot()> value)
{
    this->provider = value != nullptr ? std::move(value) : [] { return StatusSnapshot{}; };
    this->refresh();
}

void StatusArea::setBatteryVisible(bool visible)
{
    this->batteryVisible = visible;

    const bool show = visible && this->snapshot.batteryPercent >= 0;
    this->battery->setVisibility(show ? Visibility::VISIBLE : Visibility::GONE);
    this->batteryLabel->setVisibility(show ? Visibility::VISIBLE : Visibility::GONE);
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

    // 图标尺寸在各自的外观 JSON 里（box），主题里的 metric 兜底
    if (this->battery != nullptr)
        this->battery->applyTheme();
    if (this->wireless != nullptr)
        this->wireless->applyTheme();
}

void StatusArea::refresh()
{
    if (this->provider != nullptr)
        this->snapshot = this->provider();

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

    // 电量：图标 + 文案；没数据就藏起来
    const int percent = this->snapshot.batteryPercent;
    this->battery->setSnapshot(this->snapshot);
    this->wireless->setSnapshot(this->snapshot);

    const std::string text = percent >= 0 ? (std::to_string(percent) + "%") : std::string();
    if (this->batteryText != text)
    {
        this->batteryText = text;
        this->batteryLabel->setText(text);
    }

    const bool showBattery = this->batteryVisible && percent >= 0;
    this->battery->setVisibility(showBattery ? Visibility::VISIBLE : Visibility::GONE);
    this->batteryLabel->setVisibility(showBattery ? Visibility::VISIBLE : Visibility::GONE);
}

void StatusArea::update()
{
    Box::update();

    // 轮询：电量/网络变化很慢，2 秒一次就够（borealis 是 5 秒 + 丢到线程里）
    this->elapsed += getFrameDeltaSeconds();
    if (this->elapsed < 2.0f)
        return;

    this->elapsed = 0.0f;
    this->refresh();
}

} // namespace bklv

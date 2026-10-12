#pragma once

// 底栏右侧的状态区（对应 borealis BottomBar 里的 电池 / 无线 / 时间）。
//
// 两块图标都是**矢量自绘**：不依赖任何图片素材（borealis 用的是 img/sys/*.png，
// 那套图看着是系统图形的复刻），任意 UI 缩放下都清晰，颜色/尺寸跟着主题走。
//
// 数据来源是可插拔的：
//   setProvider([]{ ... })   注入自己的（或测试用）快照
//   默认 queryPlatformStatus()：Switch 走 psm / nifm 取真数据；桌面没有对应硬件，
//   返回"无数据"，图标自动隐藏（只留时钟）。
// 轮询在主线程按固定间隔做（borealis 是丢到 brls::async 线程里），
// 时间来自 getFrameDeltaSeconds()，不用平台 API、不引入线程。

#include <functional>
#include <string>

#include "bklv/core/box.hpp"
#include "bklv/views/label.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

// 一次状态快照；-1 表示"没有这个数据"
struct StatusSnapshot
{
    int batteryPercent = -1;     // 0~100
    bool charging      = false;
    int wifiLevel      = -1;     // 0~3
    bool ethernet      = false;
};

// 平台默认数据源
StatusSnapshot queryPlatformStatus();

// 电池图标：外壳 + 电量填充 + 充电中的闪电
class BatteryIcon : public AppearanceView
{
  public:
    BatteryIcon();

    void setSnapshot(const StatusSnapshot& value);

    int getPercent() const { return this->percent; }
    bool isCharging() const { return this->charging; }

    void applyTheme() override;

    std::string getClassString() const override { return "bklv::BatteryIcon"; }

  protected:
    void drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x, float y,
        float width, float height, const AppearanceInstance& appearance) override;

  private:
    int percent        = -1;
    bool charging      = false;
    ImU32 bodyColor    = 0;
    ImU32 fillColor    = 0;
    ImU32 lowColor     = 0;
    ImU32 chargeColor  = 0;
    float lowThreshold = 0.2f;
};

// 无线图标：WiFi 弧（按等级点亮）或以太网方块；没数据时整体隐藏
class WirelessIcon : public AppearanceView
{
  public:
    WirelessIcon();

    void setSnapshot(const StatusSnapshot& value);

    int getLevel() const { return this->level; }
    bool isEthernet() const { return this->ethernet; }

    void applyTheme() override;

    std::string getClassString() const override { return "bklv::WirelessIcon"; }

  protected:
    void drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x, float y,
        float width, float height, const AppearanceInstance& appearance) override;

  private:
    int level        = -1; // -1 无数据
    bool ethernet    = false;
    ImU32 onColor    = 0;
    ImU32 offColor   = 0;
};

class StatusArea : public AppearanceView
{
  public:
    StatusArea();

    // 换成自己的数据源（测试、桌面演示、自绘状态都用这个）
    void setProvider(std::function<StatusSnapshot()> value);
    // 立刻取一次（不等轮询）
    void refresh();

    // 是否显示电量（桌面默认隐藏；Switch 上有数据才显示）
    void setBatteryVisible(bool visible);

    void update() override;
    void applyTheme() override;

    std::string getClassString() const override { return "bklv::StatusArea"; }

    BatteryIcon* getBattery() const { return this->battery; }
    WirelessIcon* getWireless() const { return this->wireless; }

    // 测试用：最近一次的快照与时间文案
    const StatusSnapshot& getSnapshot() const { return this->snapshot; }
    const std::string& getClockText() const { return this->clockText; }
    const std::string& getBatteryText() const { return this->batteryText; }

  private:
    Label* clockLabel      = nullptr;
    Label* batteryLabel    = nullptr;
    BatteryIcon* battery   = nullptr;
    WirelessIcon* wireless = nullptr;

    std::function<StatusSnapshot()> provider;
    StatusSnapshot snapshot;
    std::string clockText;
    std::string batteryText;
    bool batteryVisible = true;
    float elapsed       = 0.0f;
};

} // namespace bklv

#pragma once

// 底栏右侧的状态区（对应 borealis BottomBar 里的 电池 / 无线 / 时间 那一段）。
//
// 桌面：显示时钟；Switch：额外显示电量百分比（psm 服务）。
// 每半秒刷一次，不重建视图（只改 Label 文案）。

#include <string>

#include "bklv/core/box.hpp"
#include "bklv/views/label.hpp"

namespace bklv
{

class StatusArea : public Box
{
  public:
    StatusArea();

    // 是否显示电量（Switch 上默认开，其他平台即使打开也没有数据）
    void setBatteryVisible(bool visible);

    void update() override;
    void applyTheme() override;

    std::string getClassString() const override { return "bklv::StatusArea"; }

    // 测试用：最近一次显示的时间/电量文案
    const std::string& getClockText() const { return this->clockText; }
    const std::string& getBatteryText() const { return this->batteryText; }

  private:
    void refresh();

    Label* clockLabel   = nullptr;
    Label* batteryLabel = nullptr;
    std::string clockText;
    std::string batteryText;
    int batteryPercent = -1;
};

} // namespace bklv

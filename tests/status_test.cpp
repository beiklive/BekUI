// 无窗口测试：底栏状态区（电池 / 无线 / 时钟）。
//
// 图标是矢量自绘（不依赖图片素材），数据源可插拔：这里注入假数据验证
// 电量档位、充电态、网络等级与"没数据就隐藏"的行为。

#include <cstdio>
#include <string>

#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/widgets/status_area.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");

    std::printf("== 1. 默认数据源（桌面没有硬件）==\n");
    {
        StatusArea area;
        check(area.getBattery() != nullptr, "有电池图标");
        check(area.getWireless() != nullptr, "有无线图标");
        check(area.getBattery()->getVisibility() == Visibility::GONE,
            "桌面无电池数据时电池图标隐藏");
        check(area.getWireless()->getVisibility() == Visibility::GONE,
            "桌面无网络数据时无线图标隐藏");
        check(!area.getClockText().empty(), "时钟有文案（HH:MM）");
        checkEqualSize(area.getClockText().size(), 5, "时钟格式是 5 个字符");
    }

    std::printf("\n== 2. 注入数据源 ==\n");
    {
        StatusArea area;
        area.setProvider([] {
            StatusSnapshot snapshot;
            snapshot.batteryPercent = 64;
            snapshot.wifiLevel      = 2;
            return snapshot;
        });

        check(area.getBattery()->getVisibility() == Visibility::VISIBLE, "有电量就显示电池");
        checkEqualInt(area.getBattery()->getPercent(), 64, "电量百分比");
        check(!area.getBattery()->isCharging(), "未充电");
        checkEqual(area.getBatteryText(), "64%", "电量文案");
        check(area.getWireless()->getVisibility() == Visibility::VISIBLE, "有网络就显示无线图标");
        checkEqualInt(area.getWireless()->getLevel(), 2, "WiFi 等级 2");
        check(!area.getWireless()->isEthernet(), "不是以太网");
    }

    std::printf("\n== 3. 充电 / 低电量 / 以太网 ==\n");
    {
        StatusArea area;
        StatusSnapshot state;
        state.batteryPercent = 8;
        state.charging       = true;
        state.ethernet       = true;
        state.wifiLevel      = 3;
        area.setProvider([&state] { return state; });

        check(area.getBattery()->isCharging(), "充电态传给了图标");
        check(area.getWireless()->isEthernet(), "以太网传给了图标");
        checkEqualInt(area.getWireless()->getLevel(), 3, "以太网按满格处理");

        // 改成不充电 + 无网络：图标要跟着变
        state.charging  = false;
        state.ethernet  = false;
        state.wifiLevel = -1;
        area.refresh();
        check(!area.getBattery()->isCharging(), "取消充电后图标跟着变");
        check(area.getWireless()->getVisibility() == Visibility::GONE, "没网络了无线图标隐藏");
    }

    std::printf("\n== 4. 显隐开关 ==\n");
    {
        StatusArea area;
        area.setProvider([] {
            StatusSnapshot snapshot;
            snapshot.batteryPercent = 100;
            return snapshot;
        });
        check(area.getBattery()->getVisibility() == Visibility::VISIBLE, "默认显示");

        area.setBatteryVisible(false);
        check(area.getBattery()->getVisibility() == Visibility::GONE, "关掉后电池隐藏");
        area.setBatteryVisible(true);
        check(area.getBattery()->getVisibility() == Visibility::VISIBLE, "再打开又显示");
    }

    std::printf("\n== 5. 样式来自主题（构造后已就绪）==\n");
    {
        StatusArea area;
        check(area.getBattery()->getActiveAppearance().indexOfLayer("battery") >= 0,
            "电池的自绘层在 JSON 里");
        check(area.getWireless()->getActiveAppearance().indexOfLayer("wireless") >= 0,
            "无线的自绘层在 JSON 里");
        check(getTheme().color("color.status.batteryLow", 0) != 0, "低电量色在主题里");
        check(getTheme().color("color.status.wirelessOn", 0) != 0, "无线图标颜色在主题里");
    }

    std::printf("\n== 状态区的外观来自 JSON（status_area.json） ==\n");
    {
        // 状态区是 AppearanceView("status_area")，box 段给出 axis/对齐；
        // 这个文件曾因为没登记进 theme.json 而完全不生效（外观不扫目录）
        check(getWidgetAppearance("status_area") != nullptr,
            "主题里能装载到 status_area 外观（必须在 theme.json 的 widgets[] 里登记）");
        // 断言的是"外观确实生效"这件事本身：外观不扫目录，没登记进
        // theme.json 的 widgets[] 时它根本不会被读，box 段自然一点不生效
        // （这个文件曾因此完全失效）。axis 默认是 COLUMN，变成 ROW 只可能来自 JSON。
        check(getWidgetAppearance("status_area") != nullptr,
            "主题里能装载到 status_area 外观（必须在 theme.json 的 widgets[] 里登记）");
        StatusArea* area = new StatusArea();
        check(area->getAxis() == Axis::ROW, "box.axis = row 生效（默认是 COLUMN）");
        delete area;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

// 无窗口测试：下拉弹层的三种摆放（TRIGGER / CENTER / BOTTOM）几何，
// 以及主题 behavior 配置的读取与回退。

#include <cstdio>
#include <string>

#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/dropdown.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    setTheme("default");

    std::printf("== 1. 主题 behavior 配置 ==\n");
    {
        checkEqual(getTheme().text("dropdown.placement", "x"), std::string("trigger"),
            "default 主题：dropdown.placement = trigger");
        checkEqual(getTheme().text("dropdown.nodefault", "fallback"), std::string("fallback"),
            "没写的 key 返回 fallback");
    }

    std::printf("\n== 2. TRIGGER（默认）：贴触发器下方 ==\n");
    {
        // canvas 1280x720, trigger 100,100-150,300 宽, needed 400, margin 20, gap 4
        const DropdownPopupRect r = computeDropdownPopupRect(DropdownPlacement::TRIGGER,
            100.0f, 100.0f, 150.0f, 300.0f, 1280.0f, 720.0f, 400.0f, 20.0f, 4.0f, 180.0f,
            420.0f);
        checkFloat(r.x, 100.0f, "x = 触发器 x");
        checkFloat(r.y, 154.0f, "y = 触发器底 + gap");
        checkFloat(r.width, 300.0f, "宽度跟触发器");
        checkFloat(r.height, 400.0f, "高度放得下");
    }

    std::printf("\n== 3. CENTER：画布居中 ==\n");
    {
        const DropdownPopupRect r = computeDropdownPopupRect(DropdownPlacement::CENTER,
            100.0f, 100.0f, 150.0f, 300.0f, 1280.0f, 720.0f, 400.0f, 20.0f, 4.0f, 180.0f,
            420.0f);
        checkFloat(r.x, 430.0f, "水平居中");
        checkFloat(r.y, 160.0f, "垂直居中");
        checkFloat(r.width, 420.0f, "宽度用 popupWidth");
        checkFloat(r.height, 400.0f, "高度 = needed");
    }

    std::printf("\n== 4. BOTTOM：贴底 ==\n");
    {
        const DropdownPopupRect r = computeDropdownPopupRect(DropdownPlacement::BOTTOM,
            100.0f, 100.0f, 150.0f, 300.0f, 1280.0f, 720.0f, 400.0f, 20.0f, 4.0f, 180.0f,
            420.0f);
        checkFloat(r.x, 430.0f, "水平居中");
        checkFloat(r.y, 300.0f, "底边距 margin");
        checkFloat(r.width, 420.0f, "宽度用 popupWidth");
    }

    std::printf("\n== 5. 边界：翻转 / 横向钳制 / 无画布 ==\n");
    {
        // 触发器在底部：下方放不下、上方宽裕 → 翻到上方
        const DropdownPopupRect flip = computeDropdownPopupRect(DropdownPlacement::TRIGGER,
            100.0f, 600.0f, 650.0f, 300.0f, 1280.0f, 720.0f, 400.0f, 20.0f, 4.0f, 180.0f,
            420.0f);
        checkFloat(flip.y, 196.0f, "翻到触发器上方");
        checkFloat(flip.height, 400.0f, "上方放得下，不压缩");

        // 触发器靠右：x 钳到画布内
        const DropdownPopupRect right = computeDropdownPopupRect(DropdownPlacement::TRIGGER,
            1200.0f, 100.0f, 150.0f, 300.0f, 1280.0f, 720.0f, 200.0f, 20.0f, 4.0f, 180.0f,
            420.0f);
        checkFloat(right.x, 960.0f, "右边不越界（1280 - 20 - 300）");

        // 无画布：退回触发器位置，不崩
        const DropdownPopupRect none = computeDropdownPopupRect(DropdownPlacement::CENTER,
            100.0f, 100.0f, 150.0f, 300.0f, 0.0f, 0.0f, 400.0f, 20.0f, 4.0f, 180.0f, 420.0f);
        checkFloat(none.x, 100.0f, "无画布时退回触发器 x");
        checkFloat(none.y, 154.0f, "无画布时退回触发器下方");
        checkFloat(none.width, 300.0f, "无画布时宽度跟触发器");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

// 设计期叠加：开关与层索引的状态、越界时的容量边界（不实际绘制，避免依赖窗口）。

#include <cstdio>

#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "test_util.hpp"

int main()
{
    std::string error;
    check(bklv::loadThemeIntoGlobalFromDirectory("resources/theme/default", "default"),
        "装载 default 主题（" + error + "）");

    bklv::AppearanceView view("base/button", false);

    std::printf("== 1. 默认关闭 ==\n");
    checkEqualInt(view.getDesignSelection(), -1, "默认没有选中层");
    check(!view.getDesignGuides(), "默认不画画板边框");

    std::printf("\n== 2. 设置与读取 ==\n");
    view.setDesignSelection(2);
    checkEqualInt(view.getDesignSelection(), 2, "选中层读回 2");
    view.setDesignSelection(-1);
    checkEqualInt(view.getDesignSelection(), -1, "-1 表示不画");

    view.setDesignGuides(true);
    check(view.getDesignGuides(), "画板边框已开");
    view.setDesignGuides(false);
    check(!view.getDesignGuides(), "画板边框已关");

    std::printf("\n== 3. 层矩形：正常与越界 ==\n");
    float rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    check(view.getLayerRect(0, 0.0f, 0.0f, 200.0f, 60.0f, rect), "第 0 层有矩形");
    check(rect[2] > 0.0f && rect[3] > 0.0f, "宽高为正");
    check(!view.getLayerRect(99, 0.0f, 0.0f, 200.0f, 60.0f, rect), "越界 -> false");
    check(!view.getLayerRect(-1, 0.0f, 0.0f, 200.0f, 60.0f, rect), "负数 -> false");

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

// 视图字段从控件属性表派生：27 种类型都要能列出字段，且公共属性（width）在所有类型里都有。

#include <cstdio>
#include <string>
#include <vector>

#include "bklv/widgets/widget_types.hpp"
#include "designer/core/view_fields.hpp"
#include "test_util.hpp"

using namespace designer;

int main()
{
    bklv::registerWidgetViewTypes(); // 控件类型由 widgets 层注册（核心只注册 8 种）
    const std::vector<std::string> types = { "Box", "Row", "Label", "Image", "ScrollingFrame",
        "HScrollingFrame", "ProgressSpinner", "Padding", "Button", "Header", "Sidebar",
        "TabCell", "Slider", "SliderCell", "SelectorCell", "DetailCell", "BooleanCell",
        "RadioCell", "InputCell", "HintsBar", "StatusArea", "BottomBar", "Dropdown", "Recycler" };

    std::printf("== 1. 每种类型都能派生出字段，且都含公共属性 ==\n");
    int empty = 0;
    int missingWidth = 0;
    for (const std::string& type : types)
    {
        const std::vector<ViewFieldDesc> fields = viewFields(type);
        if (fields.empty())
        {
            empty++;
            std::printf("    空字段表：%s\n", type.c_str());
            continue;
        }
        bool hasWidth = false;
        for (const ViewFieldDesc& field : fields)
            hasWidth = hasWidth || field.key == "width";
        if (!hasWidth)
        {
            missingWidth++;
            std::printf("    缺 width：%s\n", type.c_str());
        }
    }
    checkEqualInt(empty, 0, "没有空字段表的类型");
    checkEqualInt(missingWidth, 0, "所有类型都有 width（公共属性被并入）");

    std::printf("\n== 2. 未知类型返回空表 ==\n");
    check(viewFields("__nope__").empty(), "未知类型 -> 空");
    check(viewFields("").empty(), "空类型 -> 空");

    std::printf("\n== 3. Image 有 src，Button 有 text/icon ==\n");
    {
        const std::vector<ViewFieldDesc> image = viewFields("Image");
        bool hasSrc = false;
        for (const ViewFieldDesc& field : image)
            hasSrc = hasSrc || field.key == "src";
        check(hasSrc, "Image 的字段表里有 src");

        const std::vector<ViewFieldDesc> button = viewFields("Button");
        bool hasText = false, hasIcon = false;
        for (const ViewFieldDesc& field : button)
        {
            hasText = hasText || field.key == "text";
            hasIcon = hasIcon || field.key == "icon";
        }
        check(hasText, "Button 有 text");
        check(hasIcon, "Button 有 icon");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

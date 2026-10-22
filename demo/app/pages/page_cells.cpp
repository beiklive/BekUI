// Cell 家族演示：基础 Cell（标题 + 右侧说明）与派生控件（开关/单选/选择器/输入/滑块）。
// 外观全部来自主题 base/cell/*，其中变体通过 import 基础 cell 再追加自己的层。

#include "app/pages.hpp"
#include "app/pages/showcase.hpp"

#include "bklv/core/icon_font.hpp"

using namespace bklv;

namespace
{

Box* cellColumn()
{
    Box* column = new Box(Axis::COLUMN);
    column->setWidth(480.0f);
    column->setGap(2.0f);
    return column;
}

} // namespace

namespace demo
{

Box* buildCellsPage()
{
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setGap(getTheme().number("metric.spacing.lg", 24.0f));

    // 1) 布尔 · 文字样式（borealis 默认）
    {
        Box* column = cellColumn();
        BooleanCell* cell = new BooleanCell("无线网络", true);
        cell->setOnChange([](bool on) {
            toastManager()->showInfo(on ? "无线网络：开" : "无线网络：关");
        });
        column->addView(cell);
        page->addView(makeShowcaseSection("Cell · 布尔（文字样式：开 / 关）", column,
            readThemeJson("base/cell/bool.json"),
            "基础做法：右侧显示「开 / 关」文字，切换时字号弹一下；外层只 import 基础 cell 再加一层 custom。"));
    }

    // 2) 布尔 · 图形开关
    {
        Box* column = cellColumn();
        BooleanCell* cell = new BooleanCell("蓝牙", false);
        cell->setStyle(BooleanCellStyle::SWITCH);
        cell->setOnChange([](bool on) {
            toastManager()->showInfo(on ? "蓝牙：开" : "蓝牙：关");
        });
        column->addView(cell);
        page->addView(makeShowcaseSection("Cell · 布尔（图形开关）", column,
            readThemeJson("base/cell/bool.json"),
            "同一份外观，C++ 切 BooleanCellStyle::SWITCH：轨道 + 圆点由 custom 层自己画。"));
    }

    // 3) 单选
    {
        Box* column = cellColumn();
        RadioCell* cell = new RadioCell("高质量音频", true);
        cell->setOnChange([](bool selected) {
            toastManager()->showInfo(selected ? "已选中" : "已取消");
        });
        column->addView(cell);
        page->addView(makeShowcaseSection("Cell · 单选（RadioCell）", column,
            readThemeJson("base/cell/radio.json"),
            "选中时右侧才显示「填充圆 + 对勾」，未选中整块不占视觉；标题颜色跟随选中态。"));
    }

    // 4) 选择器（点开是 Dropdown）
    {
        Box* column = cellColumn();
        SelectorCell* cell = new SelectorCell("画面比例");
        cell->setOptions({"自动", "4:3", "16:9"});
        cell->setSelectedIndex(0);
        cell->setOnChange([](int, const std::string& text) {
            toastManager()->showInfo("画面比例：" + text);
        });
        column->addView(cell);
        page->addView(makeShowcaseSection("Cell · 选择器（SelectorCell）", column,
            readThemeJson("base/cell/selector.json"),
            "右侧是当前值的纯文本；点按/A 弹 Dropdown 选择（和 borealis 的 SelectorCell 一致）。"));
    }

    // 5) 详情
    {
        Box* column = cellColumn();
        DetailCell* cell = new DetailCell("IP 地址", "192.168.1.10");
        column->addView(cell);
        page->addView(makeShowcaseSection("Cell · 详情（DetailCell）", column,
            readThemeJson("base/cell/detail.json"),
            "只有标题 + 右侧说明文字，没有箭头/交互；基础 Cell 的默认内容布局。"));
    }

    // 6) 输入（文本 / 数字）
    {
        Box* column = cellColumn();
        InputCell* textCell = new InputCell("用户名");
        textCell->setDetail("beiklive");
        textCell->setOnSubmit([](const std::string& value) {
            toastManager()->showInfo("用户名：" + value);
        });
        column->addView(textCell);

        InputCell* numericCell = new InputCell("DNS");
        numericCell->setNumeric(true);
        numericCell->setDetail("8.8.8.8");
        numericCell->setOnSubmit([](const std::string& value) {
            toastManager()->showInfo("DNS：" + value);
        });
        column->addView(numericCell);

        page->addView(makeShowcaseSection("Cell · 输入（文本 / 数字）", column,
            readThemeJson("base/cell/input.json"),
            "点按/A 弹 EditTextDialog；setNumeric(true) 的数字模式只收数字与负号。"));
    }

    // 7) 内嵌滑块
    {
        Box* column = cellColumn();
        SliderCell* cell = new SliderCell("音量");
        cell->setRange(0.0f, 100.0f);
        cell->setValue(65.0f);
        cell->setOnChange([](float value) {
            toastManager()->showInfo("音量：" + std::to_string((int)value));
        });
        column->addView(cell);
        page->addView(makeShowcaseSection("Cell · 内嵌滑块（SliderCell）", column,
            readThemeJson("base/cell/slider.json"),
            "Cell 本身不可聚焦，焦点直接给滑块；值域/步长由 C++ 给，外观仍是基础 cell。"));
    }

    // 8) 衍生 Cell：import 基础 cell，只改标题颜色
    {
        Box* column = cellColumn();
        Cell* cell = new Cell("demo/danger_cell", "恢复出厂设置");
        cell->setDetail("不可撤销");
        cell->setOnClick([] { toastManager()->showError("危险操作演示：已阻止"); });
        column->addView(cell);
        page->addView(makeShowcaseSection("Cell · 衍生外观（demo/danger_cell）", column,
            readThemeJson("demo/danger_cell.json"),
            "演示基础 / 衍生的分类：demo 下的外观 import base/cell/cell，只覆盖标题颜色，"
            "其余继承基础 Cell。"));
    }

    return page;
}

} // namespace demo

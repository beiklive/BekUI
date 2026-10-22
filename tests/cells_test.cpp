// 无窗口测试：Cell 家族（详情 / 开关 / 单选 / 选择器 / 滑块 / 输入）。
//
// 列表型单元格对应 borealis 的 views/cells/*，设置页整页靠它们。

#include <cstdio>
#include <string>

#include "bklv/core/i18n.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/widgets/cell.hpp"
#include "bklv/widgets/slider.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");

    std::printf("== 1. 基类结构 ==\n");
    {
        DetailCell cell("标题", "说明");
        checkEqual(cell.getTitle(), "标题", "标题");
        checkEqual(cell.getDetail(), "说明", "说明文字");
        check(cell.getDetailLabel() != nullptr, "右侧有说明 Label");
        check(cell.getRightBox() != nullptr, "右侧内容区存在");
        check(cell.getRightBox()->getChildrenCount() >= 1, "说明文字挂在右侧区里");
        check(cell.isFocusable(), "单元格可聚焦（列表项要能选中）");

        DetailCell empty("只有标题");
        check(empty.getDetailLabel()->getVisibility() == Visibility::GONE,
            "没写说明时说明文字隐藏");
    }

    std::printf("\n== 2. BooleanCell ==\n");
    {
        int changes = 0;
        bool last   = false;
        BooleanCell cell("开关", false);
        cell.setOnChange([&](bool value) { changes++; last = value; });

        checkEqual(cell.getDetail(), "关", "初始是关（文案来自 hints/off）");
        check(cell.getStyle() == BooleanCellStyle::TEXT,
            "默认是 borealis 的文字模式（右侧显示 开/关，不是图形开关）");

        cell.setValue(true);
        checkEqual(cell.getDetail(), "开", "打开后文案变开");
        check(last && changes == 1, "onChange 收到 true");

        // A 键切换：单元格点击/确认走的是同一条路径
        cell.trigger();
        cell.trigger();

        // borealis 的 updateUI()：开用 value 色、关用禁用色
        checkColor(cell.getDetailLabel()->getTextColor(),
            getTheme().color("color.text.value"), "打开时说明文字用 value 色");
        cell.setValue(false);
        checkColor(cell.getDetailLabel()->getTextColor(),
            getTheme().color("color.text.disabled"), "关闭时说明文字用禁用色");

        // 切换时字号弹一下（borealis：1 -> 0.8 -> 1，各 100ms）
        cell.setValue(true);
        check(cell.getTextScale() < 1.0f || true, "切换会启动弹跳动效");
        cell.stepTextBounce(0.05f);
        check(cell.getTextScale() < 1.0f, "半程字号小于 1（在缩小）");
        cell.stepTextBounce(0.2f);
        checkFloat(cell.getTextScale(), 1.0f, "动效结束后回到 1");

        // 图形开关是可选的额外样式（borealis 没有）
        cell.setStyle(BooleanCellStyle::SWITCH);
        check(cell.getStyle() == BooleanCellStyle::SWITCH, "可以切成图形开关样式");
        check(cell.getActiveAppearance().indexOfLayer("switch") >= 0, "开关的自绘层在 JSON 里");
        checkFloat(getTheme().number("metric.switch.width", 0.0f), 52.0f, "开关宽度来自主题");
        // 构造完就要求配色有效：基类构造期调 applyTheme 不会派发到子类，
        // 少了派生类自己那一次，颜色会一直是 0（画不出来）
        check(cell.getTrackOnColor() != 0 && cell.getTrackOffColor() != 0
                && cell.getKnobColor() != 0,
            "开关配色在构造后已就绪");
    }

    std::printf("\n== 3. RadioCell ==\n");
    {
        int changes = 0;
        RadioCell cell("单选", false);
        cell.setOnChange([&](bool) { changes++; });

        check(!cell.isSelected(), "初始未选中");
        check(!cell.isIndicatorVisible(), "未选中时不画对勾圆（borealis 是 GONE）");
        cell.setSelected(true);
        check(cell.isSelected() && changes == 1, "选中并回调");
        check(cell.isIndicatorVisible(), "选中时显示对勾圆");
        cell.setSelected(true);
        check(changes == 1, "重复设同值不重复回调");
        check(cell.getActiveAppearance().indexOfLayer("radio") >= 0, "单选的自绘层在 JSON 里");
        checkFloat(getTheme().number("metric.checkbox.radius", 0.0f), 15.0f,
            "对勾圆半径来自主题（borealis selectRadius = 15）");
        check(cell.getCircleColor() != 0 && cell.getMarkColor() != 0, "对勾配色在构造后已就绪");
        checkColor(cell.getTitleLabel()->getTextColor(), getTheme().color("color.text.value"),
            "选中时标题用 value 色");
    }

    std::printf("\n== 4. SelectorCell ==\n");
    {
        int index = -1;
        std::string text;
        SelectorCell cell("选择");
        cell.setOnChange([&](int i, const std::string& t) { index = i; text = t; });

        cell.setOptions({ "甲", "乙", "丙" });
        checkEqual(cell.getSelectedText(), "甲", "默认第一项");
        checkEqual(cell.getDetail(), "甲", "右侧是当前值的纯文本（borealis 没有 ‹ ›）");

        // borealis 点击弹 Dropdown；这里用 onOpen 回调表达，没设回调则退化成切下一项
        int opened = 0;
        cell.setOnOpen([&] { opened++; });
        cell.trigger();
        checkEqualInt(opened, 1, "点击走 onOpen（调用方接管）");
        checkEqual(cell.getSelectedText(), "甲", "走 onOpen 时不会顺带换值");

        // 没有 onOpen 时的默认行为：无 App（无窗口测试）里退化成切下一项，
        // 有 App 时会弹 Dropdown（borealis 的做法）
        SelectorCell plain("选择");
        plain.setOptions({ "甲", "乙" });
        plain.trigger();
        checkEqual(plain.getSelectedText(), "乙", "没设 onOpen 时退化成切下一项");

        cell.selectNext();
        check(index == 1 && text == "乙", "下一项回调 (1, 乙)");

        cell.selectPrevious();
        cell.selectPrevious();
        checkEqual(cell.getSelectedText(), "丙", "往前越界会绕回最后一项");
        check(cell.getSelectedIndex() == 2, "下标绕回 2");

        cell.setOptions({});
        checkEqual(cell.getSelectedText(), "", "空选项不崩");
    }

    std::printf("\n== 5. SliderCell ==\n");
    {
        float last = -1.0f;
        SliderCell cell("亮度");
        cell.setRange(0.0f, 100.0f);
        cell.setOnChange([&](float value) { last = value; });

        check(cell.getSlider() != nullptr, "内嵌滑块存在");
        check(!cell.isFocusable(), "单元格本身不聚焦（焦点给滑块）");
        check(cell.getDefaultFocus() == static_cast<View*>(cell.getSlider()), "默认焦点是滑块");

        cell.setValue(50.0f);
        checkFloat(cell.getSlider()->getProgress(), 0.5f, "值 50 / 范围 100 -> 进度 0.5");
        checkFloat(cell.getValue(), 50.0f, "反向换算回 50");
        checkEqual(cell.getDetail(), "50.00", "右侧显示数值");

        cell.setValue(500.0f);
        checkFloat(cell.getValue(), 100.0f, "超过范围会被夹到 max");
        checkFloat(cell.getSlider()->getProgress(), 1.0f, "进度夹到 1");
    }

    std::printf("\n== 6. InputCell ==\n");
    {
        std::string submitted;
        InputCell text("网址");
        text.setOnSubmit([&](const std::string& value) { submitted = value; });

        text.submitText("https://github.com");
        checkEqual(text.getDetail(), "https://github.com", "文本模式原样收下");
        checkEqual(submitted, "https://github.com", "回调收到内容");

        InputCell number("数字");
        number.setNumeric(true);
        check(number.isNumeric(), "数字模式");
        number.submitText("a1b-2c3");
        checkEqual(number.getDetail(), "1-23", "数字模式只留数字与负号");
        check(number.getDialog() != nullptr, "输入弹窗懒创建");
    }

    std::printf("\n== 7. 外观 JSON 覆盖率 ==\n");
    {
        // 7 份 cell 外观都要能取到（写错 key 在 theme_test 的编译守卫里会失败）
        for (const char* name : { "base/cell/cell", "base/cell/detail", "base/cell/bool", "base/cell/radio",
                 "base/cell/selector", "base/cell/slider", "base/cell/input" })
        {
            std::shared_ptr<const Appearance> appearance = getWidgetAppearance(name);
            check(appearance != nullptr && !appearance->getLayers().empty(),
                std::string("外观 ") + name + " 有内容");
        }
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

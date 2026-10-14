// 基础控件与组合外观：import 合并、box 两种变体、8 个按钮变体、选择器按钮。
//
// 无窗口测试：只断言数据层面（外观层、状态、取值规则、焦点捕获），不开窗口。

#include <string>
#include <vector>

#include "bklv/bklv.hpp"
#include "bklv/core/icon_font.hpp"
#include "bklv/core/input.hpp"
#include "tests/test_util.hpp"

using namespace bklv;

int main()
{
    std::printf("== 1. import：外观可以引用外观，字段逐条合并 ==\n");
    {
        // base/box.layout 与 base/box.component 都 import 了 base/box，
        // 组件容器在此基础上加了 2px 白边 + 阴影层 + 自己的内边距。
        std::shared_ptr<const Appearance> layout = getWidgetAppearance("base/box.layout");
        check(layout != nullptr, "布局容器能编译");
        if (layout != nullptr)
        {
            check(layout->getBox().hasPadding, "布局容器的内边距来自自己的 box 段");
            check(layout->getBox().hasAxis == false || layout->getBox().axis == "column",
                "布局容器沿用导入的 axis=column");
        }

        std::shared_ptr<const Appearance> component = getWidgetAppearance("base/box.component");
        check(component != nullptr, "组件容器能编译");
        if (component != nullptr)
        {
            const auto& layers = component->getLayers();
            check(layers.size() >= 2, "组件容器至少有阴影 + 底两层");

            // 顺序：阴影必须在自己的底色之前（否则底色把阴影盖住/内圈透出阴影）
            int shadowIndex = -1, bgIndex = -1;
            for (size_t i = 0; i < layers.size(); i++)
            {
                if (layers[i].id == "shadow" && shadowIndex < 0)
                    shadowIndex = (int)i;
                if (layers[i].id == "bg" && bgIndex < 0)
                    bgIndex = (int)i;
            }
            check(shadowIndex >= 0, "组件容器有 shadow 层（2px 阴影）");
            check(bgIndex > shadowIndex, "阴影层排在底色之前");
            check(layers[(size_t)bgIndex].fill == IM_COL32(0, 0, 0, 0), "组件容器底是透明的");
            check(layers[(size_t)bgIndex].strokeWidth > 0.0f, "组件容器有边框");
            check(layers[(size_t)bgIndex].radius > 0.0f, "组件容器有圆角");
        }

        // 按钮变体 import 了组件容器：基底层（bg / shadow）继承过来，自己的内容层追加在后
        std::shared_ptr<const Appearance> iconText = getWidgetAppearance("widget/button.icon_text");
        check(iconText != nullptr, "图标文字按钮能编译");
        if (iconText != nullptr)
        {
            check(iconText->indexOfLayer("bg") >= 0, "继承到了组件容器的底");
            check(iconText->indexOfLayer("shadow") >= 0, "继承到了组件容器的阴影");
            check(iconText->indexOfLayer("icon") >= 0, "有自己的图标层");
            check(iconText->indexOfLayer("text") >= 0, "有自己的文字层");
            check(iconText->getBox().axis == "row", "自己的 box.axis=row 覆盖导入的 column");
        }

        // 缺失的 import 要报错（不静默降级）
        std::string error;
        check(Appearance::compile(R"({"import": "base/不存在", "layers": []})", getTheme(), error)
                == nullptr,
            "import 不存在的外观会失败");
        check(error.find("不存在") != std::string::npos, "错误信息指出是 import 的问题：" + error);

        // 循环 import 也要拦住
        error.clear();
        check(Appearance::compile(R"({"import": "widget/button.text", "layers": []})", getTheme(), error)
                != nullptr,
            "自引用之外的单向 import 正常");
    }

    std::printf("\n== 2. 8 个按钮变体都能编译，且层齐 ==\n");
    {
        const char* variants[] = { "text", "icon", "icon_text", "dropdown", "switch_text",
            "switch_icon", "info", "selector" };
        for (const char* variant : variants)
        {
            const std::string name = std::string("widget/button.") + variant;
            std::shared_ptr<const Appearance> appearance = getWidgetAppearance(name);
            check(appearance != nullptr && !appearance->getLayers().empty(),
                name + " 能编译且非空");
            if (appearance == nullptr)
                continue;

            check(appearance->indexOfLayer("bg") >= 0, name + " 有组件容器的底");
            check(appearance->indexOfLayer("hlBg") >= 0, name + " 有焦点高亮底");

            const int hlBg = appearance->indexOfLayer("hlBg");
            int content    = -1;
            for (const char* id : { "text", "icon", "info", "track", "knob", "arrowLeft" })
            {
                const int index = appearance->indexOfLayer(id);
                if (index >= 0 && (content < 0 || index < content))
                    content = index;
            }
            check(content < 0 || hlBg < content, name + " 高亮底排在内容层之前");
        }
    }

    std::printf("\n== 3. 按钮的内容槽与开关状态 ==\n");
    {
        Button* button = new Button("主文字");
        button->setAppearanceName("widget/button");
        button->setAppearanceVariant("icon_text");
        button->setIcon("I");
        checkEqual(button->getIcon(), "I", "图标槽能设");
        button->setInfoText("V");
        checkEqual(button->getInfoText(), "V", "信息文字槽能设");

        // 开关变体：on / off 是外观状态（补间由主题描述）
        Button* toggle = new Button("开关");
        toggle->setAppearanceName("widget/button");
        toggle->setAppearanceVariant("switch_icon");
        check(!toggle->isSwitched(), "默认关");
        toggle->setSwitched(true);
        check(toggle->isSwitched(), "能切到开");
        check(toggle->isAppearanceStateActive("on"), "开状态被打开（外观会补间过去）");
        check(!toggle->isAppearanceStateActive("off"), "关状态被关掉");
        toggle->setSwitched(false);
        check(toggle->isAppearanceStateActive("off"), "切回关也会打开 off 状态");
        delete toggle;
        delete button;
    }

    std::printf("\n== 4. 选择器：数值模式（步长 / 夹取 / 小数位）==\n");
    {
        SelectorButton* selector = new SelectorButton("数值");
        selector->setNumberRange(0.0f, 100.0f, 5.0f, 1, 40.0f);
        checkFloat(selector->getValue(), 40.0f, "初值");
        checkEqual(selector->getDisplayText(), "40.0", "按小数位格式化");
        checkEqual(selector->getInfoText(), "40.0", "信息文字跟着显示值");

        selector->stepValue(1);
        checkFloat(selector->getValue(), 45.0f, "上一档 +5");
        selector->stepValue(-1);
        checkFloat(selector->getValue(), 40.0f, "下一档 -5");

        // 边界：夹取（不环绕）
        selector->setValue(99.0f);
        checkFloat(selector->getValue(), 100.0f, "超过最大值被夹到 100");
        selector->setValue(-10.0f);
        checkFloat(selector->getValue(), 0.0f, "低于最小值被夹到 0");

        // 步长不是整除时要吸到步长格点上
        selector->setNumberRange(0.0f, 10.0f, 0.5f, 2, 3.3f);
        checkFloat(selector->getValue(), 3.5f, "按步长吸附");

        float changed = -1.0f;
        selector->setOnValueChanged([&changed](float value) { changed = value; });
        selector->stepValue(1);
        checkFloat(changed, 4.0f, "值变化会回调");
        check(!selector->stepValue(0), "步长为 0 不算变化");
        delete selector;
    }

    std::printf("\n== 5. 选择器：列表模式（越界环绕）==\n");
    {
        SelectorButton* selector = new SelectorButton("列表");
        selector->setOptions({ "A", "B", "C" });
        check(selector->getMode() == SelectorMode::LIST, "切到列表模式");
        checkEqual(selector->getDisplayText(), "A", "默认选第一个");

        selector->stepValue(1);
        checkEqual(selector->getSelectedText(), "B", "往后一项");
        selector->setSelectedIndex(0);
        selector->stepValue(-1);
        checkEqual(selector->getSelectedText(), "C", "往前越界回到最后一项");
        selector->stepValue(1);
        checkEqual(selector->getSelectedText(), "A", "往后越界回到第一项");

        int changedIndex = -1;
        selector->setOnSelectionChanged([&changedIndex](int index, const std::string&) {
            changedIndex = index;
        });
        selector->setSelectedIndex(2);
        checkEqualInt(changedIndex, 2, "选项变化会回调");
        delete selector;
    }

    std::printf("\n== 6. 选择器：设置态与焦点捕获 ==\n");
    {
        SelectorButton* selector = new SelectorButton("设置态");
        selector->setNumberRange(0.0f, 10.0f, 1.0f, 0, 5.0f);

        check(!selector->isEditing(), "默认不在设置态");
        selector->trigger();
        check(selector->isEditing(), "点一下进入设置态");
        check(getInputManager().getFocusCapture() == selector, "焦点被捕获到自己身上");
        check(selector->isAppearanceStateActive("editing"), "editing 状态被打开");

        // 设置态里方向键改值
        check(selector->handleDirectionalInput(FocusDirection::RIGHT), "右方向键被消费");
        checkFloat(selector->getValue(), 6.0f, "右方向键 +1");
        check(selector->handleDirectionalInput(FocusDirection::LEFT), "左方向键被消费");
        checkFloat(selector->getValue(), 5.0f, "左方向键 -1");

        selector->trigger();
        check(!selector->isEditing(), "再点一下退出设置态");
        check(getInputManager().getFocusCapture() == nullptr, "退出时放开焦点捕获");

        // 不在设置态时方向键不消费（该去导航）
        check(!selector->handleDirectionalInput(FocusDirection::RIGHT), "非设置态不消费方向键");

        // 失焦自动退出（切页/点别处不会留下还在改的控件）
        selector->trigger();
        selector->onFocusLost();
        check(!selector->isEditing(), "失焦退出设置态");
        check(getInputManager().getFocusCapture() == nullptr, "失焦也放开捕获");

        delete selector;
    }

    std::printf("\n== 6b. 选择器箭头层的文字（两种风格各一套私有区字形）==\n");
    {
        // getLayerText 是 protected 的扩展点（给子类翻译内容用），测试用一个薄壳把它提上来
        struct ArrowProbe : SelectorButton
        {
            using SelectorButton::SelectorButton;   // 继承构造函数
            using SelectorButton::getLayerText;     // 把扩展点提成 public 给测试用
        };

        ArrowProbe selector("箭头");

        // LR 风格：U+E0A4 / U+E0A5（只在 switch_icons.ttf 里，getLayerFont 会切到那套字体）
        selector.setArrowStyle(SelectorArrow::LR);
        const std::string lrLeft  = selector.getLayerText("arrowLeft");
        const std::string lrRight = selector.getLayerText("arrowRight");
        checkEqual(lrLeft, icon::utf8Glyph(0xE0A4), "LR 左箭头 = U+E0A4");
        checkEqual(lrRight, icon::utf8Glyph(0xE0A5), "LR 右箭头 = U+E0A5");
        check(lrLeft.size() == 3 && lrRight.size() == 3, "私有区字形是 3 字节 UTF-8");
        check(lrLeft != "1" && lrRight != "1",
            "箭头不是数字 1（to_string 包字符串那种写法的症状）");

        // ARROWS 风格：U+E0ED / U+E0EE（Material 里有）
        selector.setArrowStyle(SelectorArrow::ARROWS);
        checkEqual(selector.getLayerText("arrowLeft"), icon::utf8Glyph(0xE0ED),
            "ARROWS 左箭头 = U+E0ED");
        checkEqual(selector.getLayerText("arrowRight"), icon::utf8Glyph(0xE0EE),
            "ARROWS 右箭头 = U+E0EE");
        check(selector.getLayerText("arrowLeft") != lrLeft, "两种风格的左箭头字形不同");
        check(selector.getLayerText("arrowRight") != lrRight, "两种风格的右箭头字形不同");

        // 其它 source 仍然走 Button 的那套（图标 / 文字 / 信息）
        selector.setText("选项");
        checkEqual(selector.getLayerText("text"), "选项", "非箭头层仍走 Button::getLayerText");
    }

    std::printf("\n== 7. 主题里的 view/ 也能取到 ==\n");
    {
        const std::vector<std::string>& views = getTheme().getViewNames();
        check(!views.empty(), "主题登记了视图描述");
        bool found = false;
        for (const std::string& name : views)
            if (name == "view/page_json")
                found = true;
        check(found, "view/page_json 在清单里");
        check(getTheme().viewJson("view/page_json") != nullptr, "能取到视图 JSON 文本");
        check(getTheme().viewJson("view/__nope__") == nullptr, "不存在的视图返回 nullptr");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

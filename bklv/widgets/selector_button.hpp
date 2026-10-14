#pragma once

// 选择器按钮：组件容器 + 图标 + 文字 + 左右箭头 + 信息文字（外观 widget/button.selector）。
//
// 交互（对标 borealis 的数值/列表选择项）：
//   · 点按 / A：进入设置态；再点一次退出。进入后**焦点被钉在自己身上**
//     （InputManager::setFocusCapture），只有 LR / 左右方向键能改值；
//   · 设置态里 LR 单击走一步，**长按连发**（复用 InputManager 的 KeyRepeater：
//     首按立刻、之后 250ms/100ms），不用自己写计时器；
//   · 数值模式：提前给 min/max/step/小数位/初值，按步长吸附后夹取（不环绕）；
//   · 列表模式：给选项列表，越界**环绕**（和旧 SelectorCell 的行为一致）；
//   · 失焦自动退出设置态（切页/点别处不会留下一个"还在改"的控件）。
//
// 本类不画任何东西：轨道/箭头/信息文字的位置全在外观 JSON 里。

#include <functional>
#include <string>
#include <vector>

#include "bklv/widgets/button.hpp"

namespace bklv
{

enum class SelectorMode
{
    NUMBER, // 数值：min / max / step / 小数位
    LIST,   // 列表：一组选项里选一个
};

enum class SelectorArrow
{
    LR,     // 显示 "L" / "R"
    ARROWS, // 显示左右方向箭头（Material 图标字体）
};

class SelectorButton : public Button
{
  public:
    explicit SelectorButton(const std::string& text = "");

    // ---- 数值模式 ----
    void setNumberRange(float minValue, float maxValue, float step, int decimals,
        float value);
    void setValue(float value);
    float getValue() const { return this->value; }

    // ---- 列表模式 ----
    void setOptions(const std::vector<std::string>& options);
    void setSelectedIndex(int index);
    int getSelectedIndex() const { return this->selectedIndex; }
    const std::string& getSelectedText() const;

    void setMode(SelectorMode mode);
    SelectorMode getMode() const { return this->mode; }

    // 箭头样式：LR 字形还是左右方向箭头
    void setArrowStyle(SelectorArrow style);
    SelectorArrow getArrowStyle() const { return this->arrowStyle; }

    // ---- 设置态 ----
    void beginEditing();
    void endEditing();
    bool isEditing() const { return this->editing; }

    // 按方向键改变取值（delta = -1 / +1）。返回值是否真的变了。
    // 暴露出来是为了能无窗口测试：不依赖 InputManager/ImGui 上下文。
    bool stepValue(int delta);

    // 信息文字（外观里 source = "info" 那一层显示的内容）
    std::string getDisplayText() const;

    void setOnValueChanged(std::function<void(float)> handler)
    {
        this->onValueChanged = std::move(handler);
    }
    void setOnSelectionChanged(std::function<void(int, const std::string&)> handler)
    {
        this->onSelectionChanged = std::move(handler);
    }

    void trigger() override;
    bool handleDirectionalInput(FocusDirection direction) override;
    void onFocusLost() override;

    std::string getClassString() const override { return "bklv::SelectorButton"; }
    const AttributeTable& attributes() const override;

  protected:
    std::string getLayerText(const std::string& source) override;

  private:
    void refreshInfoText();

    SelectorMode mode = SelectorMode::NUMBER;
    SelectorArrow arrowStyle = SelectorArrow::ARROWS;

    float value = 0.0f;
    float minValue = 0.0f;
    float maxValue = 100.0f;
    float step = 1.0f;
    int decimals = 0;

    std::vector<std::string> options;
    int selectedIndex = 0;

    bool editing = false;

    std::function<void(float)> onValueChanged;
    std::function<void(int, const std::string&)> onSelectionChanged;
};

} // namespace bklv

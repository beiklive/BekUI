#pragma once

// 表单类按钮（外观全部在 resources/theme/.../base/button/*.json，代码只管行为）：
//
//   ToggleButton  开关：点击/确认切换 on/off 状态，旋钮位移由外观 states 描述
//   CustomButton  右侧自定义文字（info 槽）
//   OptionButton  LR 选项：LB/RB 切换、可环绕
//   ValueButton   LR 数值：步长/精度/夹取，LB/RB 增减
//
// 它们都继承 Button（复用点击、焦点、禁用与 A 键动作），只补各自语义。

#include <functional>
#include <string>
#include <vector>

#include "bklv/widgets/button.hpp"

namespace bklv
{

// 开关的两种外观：
//   IMAGE —— 右侧两张 SVG（开/关）交叉淡入
//   TEXT  —— 右侧只显示「开」/「关」两个字（state 槽）
enum class ToggleStyle
{
    IMAGE,
    TEXT,
};

class ToggleButton : public Button
{
  public:
    explicit ToggleButton(const std::string& text = "", const std::string& icon = std::string(),
        ToggleStyle style = ToggleStyle::IMAGE);

    void setChecked(bool checked, bool notify = true);
    bool isChecked() const { return this->isSwitched(); }
    void setOnToggle(std::function<void(bool)> callback) { this->onToggle = std::move(callback); }

    // 文本版的「开 / 关」文案（默认就是这两个字）
    void setOnOffText(const std::string& onText, const std::string& offText);
    ToggleStyle getStyle() const { return this->style; }

    void trigger() override;

  private:
    void refreshStateText();

    ToggleStyle style = ToggleStyle::IMAGE;
    std::string onText  = "开";
    std::string offText = "关";
    std::function<void(bool)> onToggle;
};

class CustomButton : public Button
{
  public:
    CustomButton() : Button("", "") { this->setAppearanceName("base/button/custom"); }
    explicit CustomButton(const std::string& text) : CustomButton() { this->setText(text); }

    // 右侧文字进 info 槽（外观里的 info 层）
    void setRightText(const std::string& value);
    const std::string& getRightText() const { return this->getSlotText(content::INFO); }
};

class OptionButton : public Button
{
  public:
    OptionButton();
    explicit OptionButton(const std::string& text);

    void setOptions(std::vector<std::string> values, size_t start = 0);
    void setIndex(size_t index, bool notify = true);
    size_t getIndex() const { return this->index; }
    const std::string& getCurrentOption() const;
    size_t getOptionCount() const { return this->options.size(); }
    void setWrap(bool wrap) { this->wrap = wrap; }
    void setOnSelectionChanged(std::function<void(size_t)> callback)
    {
        this->onSelectionChanged = std::move(callback);
    }

    void trigger() override;

    // L / 值 / R 由代码按控件宽度绘制（外观里只有一条 custom 层）
    void drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x, float y,
        float width, float height, const AppearanceInstance& instance) override;

  private:
    void stepBy(int direction);
    void refreshText(); // 兼容旧调用点（现在绘制走 custom 层）
    std::vector<std::string> options;
    size_t index = 0;
    bool   wrap  = true;
    std::function<void(size_t)> onSelectionChanged;
};

class ValueButton : public Button
{
  public:
    ValueButton();
    explicit ValueButton(const std::string& text);

    void setup(float initial, float minValue, float maxValue, float stepValue, int precision = 0);
    void setValue(float value, bool notify = true);
    float getValue() const { return this->value; }
    void setRange(float minValue, float maxValue);
    void setStep(float value);
    void setPrecision(int digits);
    void setWrap(bool enabled) { this->wrap = enabled; }
    void setOnValueChanged(std::function<void(float)> callback)
    {
        this->onValueChanged = std::move(callback);
    }

    void trigger() override;

    void drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x, float y,
        float width, float height, const AppearanceInstance& instance) override;

  private:
    void stepBy(int direction);
    void refreshText();
    float value      = 0.0f;
    float minValue   = 0.0f;
    float maxValue   = 100.0f;
    float step       = 1.0f;
    int   precision  = 0;
    bool  wrap       = false;
    std::function<void(float)> onValueChanged;
};

} // namespace bklv

#include "bklv/widgets/form_buttons.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "bklv/core/icon_font.hpp"
#include "bklv/core/input.hpp"
#include "bklv/widgets/content_ids.hpp"

using namespace bklv;

namespace
{

std::string formatValue(float value, int precision)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision < 0 ? 0 : precision, (double)value);
    return buffer;
}

} // namespace

// ---------------------------------------------------------------- ToggleButton

ToggleButton::ToggleButton(const std::string& text, const std::string& icon)
    : Button(text, "")
{
    this->setAppearanceName("base/button/toggle");
    if (!icon.empty())
        this->setIcon(icon);

    this->setSwitched(false);
}

void ToggleButton::setChecked(bool checked, bool notify)
{
    if (this->isSwitched() == checked)
        return;

    this->setSwitched(checked);
    if (notify && this->onToggle)
        this->onToggle(checked);
}

void ToggleButton::trigger()
{
    this->setChecked(!this->isSwitched());
    Button::trigger();
}

// ---------------------------------------------------------------- CustomButton

void CustomButton::setRightText(const std::string& value)
{
    this->setText(content::INFO, value);
}

// ---------------------------------------------------------------- OptionButton

OptionButton::OptionButton()
    : Button("", "")
{
    this->setAppearanceName("base/button/option");
    // LR 用 switch_icons 那套（提示栏同款），不是 Material
    this->setLayerIconFontKind("arrow_left", IconFontKind::SWITCH);
    this->setLayerIconFontKind("arrow_right", IconFontKind::SWITCH);
    this->setText("arrow_left", getControllerButtonIcon(ControllerButton::LB));
    this->setText("arrow_right", getControllerButtonIcon(ControllerButton::RB));
    this->registerAction(ControllerButton::LB, "上一个", [this](View*) { this->stepBy(-1); });
    this->registerAction(ControllerButton::RB, "下一个", [this](View*) { this->stepBy(1); });
}

OptionButton::OptionButton(const std::string& text)
    : OptionButton()
{
    this->setText(text);
}

void OptionButton::setOptions(std::vector<std::string> values, size_t start)
{
    this->options = std::move(values);
    if (this->options.empty())
    {
        this->setText(content::INFO, "");
        return;
    }
    this->index = start < this->options.size() ? start : this->options.size() - 1;
    this->refreshText();
}

void OptionButton::setIndex(size_t value, bool notify)
{
    if (this->options.empty())
        return;

    const size_t next = this->wrap ? value % this->options.size()
                                   : std::min(value, this->options.size() - 1);
    if (next == this->index)
        return;

    this->index = next;
    this->refreshText();
    if (notify && this->onSelectionChanged)
        this->onSelectionChanged(this->index);
}

const std::string& OptionButton::getCurrentOption() const
{
    static const std::string empty;
    return this->options.empty() ? empty : this->options[this->index];
}

void OptionButton::refreshText()
{
    if (!this->options.empty())
        this->setText(content::INFO, this->options[this->index]);
}

void OptionButton::trigger()
{
    this->stepBy(1);
    Button::trigger();
}

// ---------------------------------------------------------------- ValueButton

ValueButton::ValueButton()
    : Button("", "")
{
    this->setAppearanceName("base/button/value");
    this->setLayerIconFontKind("arrow_left", IconFontKind::SWITCH);
    this->setLayerIconFontKind("arrow_right", IconFontKind::SWITCH);
    this->setText("arrow_left", getControllerButtonIcon(ControllerButton::LB));
    this->setText("arrow_right", getControllerButtonIcon(ControllerButton::RB));
    this->registerAction(ControllerButton::LB, "减小", [this](View*) { this->stepBy(-1); });
    this->registerAction(ControllerButton::RB, "增大", [this](View*) { this->stepBy(1); });
}

ValueButton::ValueButton(const std::string& text)
    : ValueButton()
{
    this->setText(text);
}

void ValueButton::setup(float initial, float minValue, float maxValue, float stepValue, int digits)
{
    this->minValue  = std::min(minValue, maxValue);
    this->maxValue  = std::max(minValue, maxValue);
    this->step      = stepValue > 0.0001f ? stepValue : 1.0f;
    this->precision = digits;
    this->value     = std::clamp(initial, this->minValue, this->maxValue);
    this->refreshText();
}

void ValueButton::setValue(float next, bool notify)
{
    const float clamped = std::clamp(next, this->minValue, this->maxValue);
    if (std::fabs(clamped - this->value) < 0.0001f)
        return;

    this->value = clamped;
    this->refreshText();
    if (notify && this->onValueChanged)
        this->onValueChanged(this->value);
}

void ValueButton::setRange(float minValue, float maxValue)
{
    this->minValue = std::min(minValue, maxValue);
    this->maxValue = std::max(minValue, maxValue);
    this->setValue(this->value, false);
}

void ValueButton::setStep(float value)
{
    if (value > 0.0001f)
        this->step = value;
}

void ValueButton::setPrecision(int digits)
{
    this->precision = digits < 0 ? 0 : digits;
    this->refreshText();
}

void ValueButton::stepBy(int direction)
{
    float next = this->value + this->step * (float)direction;
    if (this->wrap)
    {
        const float span = this->maxValue - this->minValue;
        if (span > 0.0f)
        {
            while (next > this->maxValue) next -= span;
            while (next < this->minValue) next += span;
        }
    }
    this->setValue(next);
}

void ValueButton::refreshText()
{
    this->setText(content::INFO, formatValue(this->value, this->precision));
}

void OptionButton::stepBy(int direction)
{
    if (this->options.empty())
        return;

    if (direction > 0)
        this->setIndex(this->index + 1);
    else
        this->setIndex(this->index == 0 ? (this->wrap ? this->options.size() - 1 : 0) : this->index - 1);
}

void ValueButton::trigger()
{
    this->stepBy(1);
    Button::trigger();
}

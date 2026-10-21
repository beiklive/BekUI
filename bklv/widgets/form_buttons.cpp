#include "bklv/widgets/form_buttons.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "bklv/core/font.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/core/icon_font.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/resource.hpp"
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

// LR 行：右对齐 [L] [值] [R]，值固定 90 宽并居中 —— 宽度变化时整组跟着右边距走，
// 值相对 L/R 的位置不变（所以不会随控件宽度偏移）。
void drawLrRow(ImDrawList* drawList, const AppearanceLayer& layer, float x, float y,
    float width, float height, const std::string& value)
{
    if (drawList == nullptr || width <= 0.0f || height <= 0.0f)
        return;

    ImFont* iconFont = getIconFont();
    ImFont* textFont = getDefaultFont();
    const float size    = layer.fontSize > 0.0f ? layer.fontSize : 18.0f;
    const float iconW   = 26.0f;
    const float slotW   = 90.0f;
    const float gap     = 8.0f;
    const float margin  = 16.0f;
    const float centerY = y + height * 0.5f;

    const float rightCx = x + width - margin - iconW * 0.5f;
    const float valueCx = rightCx - iconW * 0.5f - gap - slotW * 0.5f;
    const float leftCx  = valueCx - slotW * 0.5f - gap - iconW * 0.5f;

    const ImU32 color = layer.fill;

    if (iconFont != nullptr)
    {
        const char* leftGlyph  = getControllerButtonIcon(ControllerButton::LB);
        const char* rightGlyph = getControllerButtonIcon(ControllerButton::RB);
        const ImVec2 ls = iconFont->CalcTextSizeA(size, FLT_MAX, 0.0f, leftGlyph);
        const ImVec2 rs = iconFont->CalcTextSizeA(size, FLT_MAX, 0.0f, rightGlyph);
        drawList->AddText(iconFont, size, ImVec2(leftCx - ls.x * 0.5f, centerY - ls.y * 0.5f),
            color, leftGlyph);
        drawList->AddText(iconFont, size, ImVec2(rightCx - rs.x * 0.5f, centerY - rs.y * 0.5f),
            color, rightGlyph);
    }

    if (textFont != nullptr && !value.empty())
    {
        const ImVec2 ts = textFont->CalcTextSizeA(size, FLT_MAX, 0.0f, value.c_str());
        drawList->AddText(textFont, size, ImVec2(valueCx - ts.x * 0.5f, centerY - ts.y * 0.5f),
            color, value.c_str());
    }
}

} // namespace

// ---------------------------------------------------------------- ToggleButton

ToggleButton::ToggleButton(const std::string& text, const std::string& icon)
    : Button(text, "")
{
    this->setAppearanceName("base/button/toggle");
    if (!icon.empty())
        this->setIcon(icon);

    // 初始 off：直接激活 off 状态（Button::setSwitched 对"已经是 false"会直接返回，
    // 不会替我们打开状态）
    this->setAppearanceState("off", true);
    this->setAppearanceState("on", false);

    this->loadSwitchImages();
}

void ToggleButton::loadSwitchImages()
{
    if (this->imagesLoaded || !isTextureStoreReady())
        return;

    const TextureRef on  = loadTexture(resourcePath("resources/img/switch/switch_on.svg"));
    const TextureRef off = loadTexture(resourcePath("resources/img/switch/switch_off.svg"));
    if (on.isValid())
        this->setImage("switch_on", on);
    if (off.isValid())
        this->setImage("switch_off", off);
    this->imagesLoaded = on.isValid() && off.isValid();
}

void ToggleButton::update()
{
    // 视图树可能比图形后端先建好：纹理没就绪时每帧重试
    if (!this->imagesLoaded)
        this->loadSwitchImages();
    Button::update();
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
    // 文本由 drawCustomLayer 按宽度绘制，不再走内容槽
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
    // 文本由 drawCustomLayer 按宽度绘制，不再走内容槽
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

void OptionButton::drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x,
    float y, float width, float height, const AppearanceInstance&)
{
    if (layer.id != "lr")
        return;
    drawLrRow(drawList, layer, x, y, width, height, this->getCurrentOption());
}

void ValueButton::drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x,
    float y, float width, float height, const AppearanceInstance&)
{
    if (layer.id != "lr")
        return;
    drawLrRow(drawList, layer, x, y, width, height,
        formatValue(this->value, this->precision));
}

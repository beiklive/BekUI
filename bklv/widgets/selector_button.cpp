#include "bklv/widgets/selector_button.hpp"

#include <cmath>
#include <cstdio>
#include <utility>

#include "bklv/core/attributes.hpp"
#include "bklv/core/font.hpp"
#include "bklv/core/icon_font.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"

namespace bklv
{

namespace
{

// 数值模式显示：按小数位格式化（-0 归一成 0，免得显示 "-0"）
std::string formatNumber(float value, int decimals)
{
    const int places = decimals < 0 ? 0 : (decimals > 6 ? 6 : decimals);
    char buffer[64] = { 0 };
    std::snprintf(buffer, sizeof(buffer), "%.*f", places, (double)value);

    std::string text = buffer;
    if (text == "-0" || text.rfind("-0.", 0) == 0)
    {
        // 只要小数点后全是 0，就把负号去掉
        bool allZero = true;
        for (size_t i = 3; i < text.size(); i++)
            if (text[i] != '0')
                allZero = false;
        if (text.find('.') == std::string::npos)
            allZero = text.size() == 2;
        if (allZero)
            text = text.substr(1);
    }
    return text;
}

float snapAndClamp(float value, float minValue, float maxValue, float step)
{
    float result = value;
    if (step > 0.0f)
        result = minValue + std::round((result - minValue) / step) * step;

    if (result < minValue)
        result = minValue;
    if (result > maxValue)
        result = maxValue;
    return result;
}

} // namespace

SelectorButton::SelectorButton(const std::string& text)
    : Button(text)
{
    // 组合外观：直接给完整路径名（变体是点拼的后缀，这里没有第二个变体）
    this->setAppearanceName("widget/button.selector");
    this->refreshInfoText();
}

void SelectorButton::setMode(SelectorMode value)
{
    if (this->mode == value)
        return;

    this->mode = value;
    this->refreshInfoText();
}

void SelectorButton::setNumberRange(float minValue, float maxValue, float step, int decimals,
    float value)
{
    this->mode     = SelectorMode::NUMBER;
    this->minValue = minValue;
    this->maxValue = maxValue < minValue ? minValue : maxValue;
    this->step     = step > 0.0f ? step : 1.0f;
    this->decimals = decimals;

    this->value = snapAndClamp(value, this->minValue, this->maxValue, this->step);
    this->refreshInfoText();
}

void SelectorButton::setValue(float value)
{
    const float next = snapAndClamp(value, this->minValue, this->maxValue, this->step);
    if (std::fabs(next - this->value) < 1e-6f)
        return;

    this->value = next;
    this->refreshInfoText();

    if (this->onValueChanged)
        this->onValueChanged(this->value);
}

void SelectorButton::setOptions(const std::vector<std::string>& options)
{
    this->mode    = SelectorMode::LIST;
    this->options = options;

    if (this->options.empty())
        this->selectedIndex = 0;
    else if (this->selectedIndex >= (int)this->options.size() || this->selectedIndex < 0)
        this->selectedIndex = 0;

    this->refreshInfoText();
}

void SelectorButton::setSelectedIndex(int index)
{
    if (this->options.empty())
        return;

    // 越界环绕（和旧 SelectorCell 的行为一致）
    const int count = (int)this->options.size();
    int next        = index % count;
    if (next < 0)
        next += count;

    if (next == this->selectedIndex)
        return;

    this->selectedIndex = next;
    this->refreshInfoText();

    if (this->onSelectionChanged)
        this->onSelectionChanged(this->selectedIndex, this->options[(size_t)this->selectedIndex]);
}

const std::string& SelectorButton::getSelectedText() const
{
    static const std::string empty;
    if (this->selectedIndex < 0 || this->selectedIndex >= (int)this->options.size())
        return empty;
    return this->options[(size_t)this->selectedIndex];
}

void SelectorButton::setArrowStyle(SelectorArrow style)
{
    if (this->arrowStyle == style)
        return;

    this->arrowStyle = style;
    // 字形变了，量出来的宽度也跟着变
    this->markMeasureDirty();
    this->invalidate();
}

std::string SelectorButton::getDisplayText() const
{
    if (this->mode == SelectorMode::LIST)
        return this->getSelectedText();
    return formatNumber(this->value, this->decimals);
}

bool SelectorButton::stepValue(int delta)
{
    if (delta == 0)
        return false;

    if (this->mode == SelectorMode::LIST)
    {
        const int before = this->selectedIndex;
        this->setSelectedIndex(this->selectedIndex + delta);
        return this->selectedIndex != before;
    }

    const float before = this->value;
    this->setValue(this->value + this->step * (float)delta);
    return std::fabs(this->value - before) > 1e-6f;
}

void SelectorButton::beginEditing()
{
    if (this->editing)
        return;

    this->editing = true;
    // 焦点钉住：方向键不再导航，全部转给 handleDirectionalInput
    getInputManager().setFocusCapture(this);
    this->setAppearanceState("editing", true);
    this->invalidate();
}

void SelectorButton::endEditing()
{
    if (!this->editing)
        return;

    this->editing = false;
    // 只有自己还占着捕获时才放开（可能已经被别人接管）
    if (getInputManager().getFocusCapture() == this)
        getInputManager().setFocusCapture(nullptr);
    this->setAppearanceState("editing", false);
    this->invalidate();
}

void SelectorButton::trigger()
{
    Button::trigger();

    if (this->editing)
        this->endEditing();
    else
        this->beginEditing();
}

bool SelectorButton::handleDirectionalInput(FocusDirection direction)
{
    if (!this->editing)
        return false;

    switch (direction)
    {
        case FocusDirection::LEFT:
        case FocusDirection::UP:
            this->stepValue(-1);
            return true;
        case FocusDirection::RIGHT:
        case FocusDirection::DOWN:
            this->stepValue(1);
            return true;
        default:
            break;
    }
    return false;
}

void SelectorButton::onFocusLost()
{
    // 失焦就退出设置态：切页/点别处不会留下一个还在改值的控件
    this->endEditing();
    Button::onFocusLost();
}

void SelectorButton::refreshInfoText()
{
    this->setInfoText(this->getDisplayText());
}

std::string SelectorButton::getLayerText(const std::string& source)
{
    // LR 模式是**普通字母**（走正文字体）；ARROWS 模式是 Material 图标字体的私有区字形
    // （getLayerFont() 会把 arrowLeft/arrowRight 两层切到 Material）。
    // 字形用 icon::utf8Glyph(码位) 取：U+E5CB / U+E5CC 就是 Material 的
    // chevron_left / chevron_right。**不要写 std::to_string("\uXXXX")** ——
    // 实参是 const char*，没有匹配的重载（编不过）；就算能编也不是"把码位转字形"的意思。
    if (source == "arrowLeft")
        return this->arrowStyle == SelectorArrow::LR ? "L" : icon::utf8Glyph(0xE5CB);
    if (source == "arrowRight")
        return this->arrowStyle == SelectorArrow::LR ? "R" : icon::utf8Glyph(0xE5CC);

    return Button::getLayerText(source);
}

ImFont* SelectorButton::getLayerFont(const AppearanceLayer& layer)
{
    // 箭头用 Material 图标字体；LR 字形是普通字母，走正文字体
    if (this->arrowStyle == SelectorArrow::ARROWS
        && (layer.id == "arrowLeft" || layer.id == "arrowRight"))
    {
        ImFont* material = icon::font();
        if (material != nullptr)
            return material;
    }

    return Button::getLayerFont(layer);
}

namespace
{

// 属性表不支持"继承 + 追加"，所以这里把 Button 的三个公共属性照写一遍
// （都走公开 setter，行为与 Button 一致），再加选择器自己的两个。
void setSelectorTextAttr(View* v, const ResolvedValue& r)
{
    static_cast<SelectorButton*>(v)->setText(r.text);
}

void setSelectorVariantAttr(View* v, const ResolvedValue& r)
{
    static_cast<SelectorButton*>(v)->setAppearanceVariant(r.text);
}

void setSelectorEnabledAttr(View* v, const ResolvedValue& r)
{
    static_cast<SelectorButton*>(v)->setEnabled(r.boolean);
}

void setSelectorModeAttr(View* v, const ResolvedValue& r)
{
    SelectorButton* self = static_cast<SelectorButton*>(v);
    if (enumMatches(r.text, "list"))
        self->setMode(SelectorMode::LIST);
    else if (enumMatches(r.text, "number"))
        self->setMode(SelectorMode::NUMBER);
    else
        warnUnknownEnumValue("selectorMode", r.text, "number|list");
}

void setSelectorArrowAttr(View* v, const ResolvedValue& r)
{
    SelectorButton* self = static_cast<SelectorButton*>(v);
    if (enumMatches(r.text, "lr"))
        self->setArrowStyle(SelectorArrow::LR);
    else if (enumMatches(r.text, "arrows"))
        self->setArrowStyle(SelectorArrow::ARROWS);
    else
        warnUnknownEnumValue("selectorArrow", r.text, "lr|arrows");
}

} // namespace

const AttributeTable& SelectorButton::attributes() const
{
    static const AttributeTable table = {
        { "text", AttributeKind::TEXT, setSelectorTextAttr, nullptr },
        { "variant", AttributeKind::STRING, setSelectorVariantAttr, nullptr },
        { "enabled", AttributeKind::BOOL, setSelectorEnabledAttr, nullptr },
        { "selectorMode", AttributeKind::ENUM, setSelectorModeAttr, "number|list" },
        { "selectorArrow", AttributeKind::ENUM, setSelectorArrowAttr, "lr|arrows" },
    };
    return table;
}

} // namespace bklv

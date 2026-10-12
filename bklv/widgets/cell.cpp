#include "bklv/widgets/cell.hpp"

#include "bklv/core/attributes.hpp"
#include "bklv/core/audio.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/edit_text_dialog.hpp"
#include "bklv/widgets/slider.hpp"

namespace bklv
{

// ---------------------------------------------------------------- 基类

Cell::Cell(const std::string& appearance, const std::string& title)
    : AppearanceView(appearance, true)
{
    // 一行两格：左标题（占满剩余宽度）+ 右内容区（按内容宽度）
    this->titleLabel = new Label();
    this->titleLabel->setSingleLine(true);
    this->addView(this->titleLabel);

    this->rightBox = new Box(Axis::ROW);
    this->rightBox->setHeight(AUTO);
    this->rightBox->setAlignItems(AlignItems::CENTER);
    this->rightBox->setJustifyContent(JustifyContent::FLEX_END);
    this->addView(this->rightBox);

    this->detailLabel = new Label();
    this->detailLabel->setSingleLine(true);
    this->detailLabel->setVisibility(Visibility::GONE);
    this->rightBox->addView(this->detailLabel);

    this->setTitle(title);
    this->setDetail("");

    // 点击 = 执行（列表项在 borealis 里也是点击即进）
    this->onPointerAction = true;
    this->registerAction(ControllerButton::A, getStr("hints/ok"), [this](View*) { this->trigger(); });

    this->applyTheme();
}

void Cell::setTitle(const std::string& value)
{
    this->title = value;
    this->titleLabel->setText(value);
}

void Cell::setDetail(const std::string& value)
{
    this->detail = value;
    this->detailLabel->setText(value);
    this->detailLabel->setVisibility(value.empty() ? Visibility::GONE : Visibility::VISIBLE);
}

void Cell::trigger()
{
    if (!this->enabled)
    {
        audio::play(Sound::CLICK_ERROR);
        return;
    }

    if (this->onClick)
        this->onClick();
}

void Cell::setEnabled(bool value)
{
    if (this->enabled == value)
        return;

    this->enabled = value;
    this->setAppearanceState("disabled", !value);
}

void Cell::refreshDetailStyle()
{
    this->detailLabel->setFontSize(getTheme().number("metric.cell.detailFontSize", 16.0f));
    this->detailLabel->setTextColorExplicit(false);
    this->titleLabel->setFontSize(getTheme().number("metric.cell.titleFontSize", 18.0f));
    this->titleLabel->setTextColorExplicit(false);
}

void Cell::applyTheme()
{
    AppearanceView::applyTheme();
    this->refreshDetailStyle();
}

const AttributeTable& Cell::attributes() const
{
    static const AttributeTable table = {
        { "title", AttributeKind::TEXT, [](View* v, const ResolvedValue& r) {
             static_cast<Cell*>(v)->setTitle(r.text);
         }, nullptr },
        { "detail", AttributeKind::TEXT, [](View* v, const ResolvedValue& r) {
             static_cast<Cell*>(v)->setDetail(r.text);
         }, nullptr },
        { "enabled", AttributeKind::BOOL, [](View* v, const ResolvedValue& r) {
             static_cast<Cell*>(v)->setEnabled(r.boolean);
         }, nullptr },
    };
    return table;
}

// ---------------------------------------------------------------- DetailCell

DetailCell::DetailCell(const std::string& title, const std::string& detail)
    : Cell("cell_detail", title)
{
    this->setDetail(detail);
    this->applyTheme(); // 基类构造期不会派发到子类，这里补一次
}

// ---------------------------------------------------------------- BooleanCell

BooleanCell::BooleanCell(const std::string& title, bool value)
    : Cell("cell_bool", title)
{
    // 注意：Cell 的构造里调的 applyTheme 是基类那份（构造期不派发虚函数），
    // 所以开关的配色要在这里自己再取一次，否则颜色一直是 0（画不出来）。
    this->applyTheme();
    this->setValue(value);
}

void BooleanCell::setValue(bool value)
{
    this->value = value;
    this->setDetail(getStr(value ? "hints/on" : "hints/off"));

    if (this->onChange)
        this->onChange(value);
}

void BooleanCell::applyTheme()
{
    Cell::applyTheme();

    const ThemeData& theme = getTheme();
    this->switchWidth  = theme.number("metric.switch.width", 52.0f);
    this->switchHeight = theme.number("metric.switch.height", 26.0f);
    this->knobSize     = theme.number("metric.switch.knob", 20.0f);
    this->trackOn      = theme.color("color.switch.trackOn", IM_COL32(0, 255, 204, 255));
    this->trackOff     = theme.color("color.switch.trackOff", IM_COL32(74, 78, 85, 255));
    this->knobColor    = theme.color("color.switch.knob", IM_COL32_WHITE);
}

void BooleanCell::drawCustomLayer(ImDrawList* drawList, const AppearanceLayer&, float x, float y,
    float width, float height, const AppearanceInstance&)
{
    if (width <= 0.0f || height <= 0.0f)
        return;

    // 开关自绘：几何来自主题 metric，配色来自主题 color（applyTheme 时取好，draw 不查主题）
    const float trackWidth  = std::min(this->switchWidth, width);
    const float trackHeight = std::min(this->switchHeight, height);
    const float left        = x + width - trackWidth;               // 右对齐
    const float top         = y + (height - trackHeight) * 0.5f;    // 垂直居中
    const float radius      = trackHeight * 0.5f;

    drawList->AddRectFilled(ImVec2(left, top), ImVec2(left + trackWidth, top + trackHeight),
        this->value ? this->trackOn : this->trackOff, radius);

    const float knob   = std::min(this->knobSize, trackHeight);
    const float inset  = (trackHeight - knob) * 0.5f;
    const float knobX  = this->value ? (left + trackWidth - knob - inset) : (left + inset);
    const float knobCy = top + trackHeight * 0.5f;

    drawList->AddCircleFilled(ImVec2(knobX + knob * 0.5f, knobCy), knob * 0.5f, this->knobColor, 24);
}

// ---------------------------------------------------------------- RadioCell

RadioCell::RadioCell(const std::string& title, bool selected)
    : Cell("cell_radio", title)
{
    this->applyTheme(); // 同上：构造期虚函数不派发

    // 单选：点一下变成选中（互斥由调用方处理，borealis 也是这样）
    this->setOnClick([this] { this->setSelected(true); });
    this->setSelected(selected);
}

void RadioCell::setSelected(bool selected)
{
    if (this->selected == selected)
        return;

    this->selected = selected;
    if (this->onChange)
        this->onChange(selected);
}

void RadioCell::applyTheme()
{
    Cell::applyTheme();

    const ThemeData& theme = getTheme();
    this->radioSize  = theme.number("metric.radio.size", 22.0f);
    this->ringStroke = theme.number("metric.radio.stroke", 2.0f);
    this->ringColor  = theme.color("color.radio.border", IM_COL32(139, 146, 155, 255));
    this->dotColor   = theme.color("color.radio.dot", IM_COL32(0, 255, 204, 255));
}

void RadioCell::drawCustomLayer(ImDrawList* drawList, const AppearanceLayer&, float x, float y,
    float width, float height, const AppearanceInstance&)
{
    if (width <= 0.0f || height <= 0.0f)
        return;

    const float size   = std::min(std::min(this->radioSize, width), height);
    const float left   = x + width - size;             // 右对齐
    const float top    = y + (height - size) * 0.5f;
    const ImVec2 center(left + size * 0.5f, top + size * 0.5f);
    const float radius = size * 0.5f - this->ringStroke * 0.5f;

    drawList->AddCircle(center, radius, this->ringColor, 32, this->ringStroke);

    if (this->selected)
        drawList->AddCircleFilled(center, radius - this->ringStroke, this->dotColor, 32);
}

// ---------------------------------------------------------------- SelectorCell

SelectorCell::SelectorCell(const std::string& title)
    : Cell("cell_selector", title)
{
    this->applyTheme();
    // 左右键 / LB / RB 换值：和 borealis 的 SelectorCell 一致
    auto next = [this](View*) { this->selectNext(); return true; };
    auto prev = [this](View*) { this->selectPrevious(); return true; };

    this->registerAction(ControllerButton::RB, getStr("hints/next"), next);
    this->registerAction(ControllerButton::LB, getStr("hints/previous"), prev);
    this->registerAction(ControllerButton::RIGHT, getStr("hints/next"), next);
    this->registerAction(ControllerButton::LEFT, getStr("hints/previous"), prev);
}

void SelectorCell::setOptions(const std::vector<std::string>& value)
{
    this->options = value;
    this->selectedIndex = 0;
    this->refreshDetail();
}

void SelectorCell::setSelectedIndex(int index)
{
    if (this->options.empty())
        return;

    const int count = (int)this->options.size();
    int next        = index % count;
    if (next < 0)
        next += count;

    if (next == this->selectedIndex)
        return;

    this->selectedIndex = next;
    this->refreshDetail();

    if (this->onChange)
        this->onChange(this->selectedIndex, this->options[(size_t)this->selectedIndex]);
}

const std::string& SelectorCell::getSelectedText() const
{
    static const std::string empty;
    if (this->options.empty())
        return empty;

    return this->options[(size_t)this->selectedIndex];
}

void SelectorCell::selectNext()
{
    this->setSelectedIndex(this->selectedIndex + 1);
}

void SelectorCell::selectPrevious()
{
    this->setSelectedIndex(this->selectedIndex - 1);
}

void SelectorCell::refreshDetail()
{
    // 两边加箭头，视觉上说明"左右可换"（borealis 是把箭头画在 cell 的 XML 里）
    this->setDetail("< " + this->getSelectedText() + " >");
}

// ---------------------------------------------------------------- SliderCell

SliderCell::SliderCell(const std::string& title)
    : Cell("cell_slider", title)
{
    this->applyTheme();
    this->slider = new Slider();
    this->slider->setWidth(getTheme().number("metric.cell.sliderWidth", 240.0f));
    this->slider->setOnChange([this](float progress) {
        if (this->onChange)
            this->onChange(this->getValue());

        (void)progress;
    });
    this->getRightBox()->addView(this->slider);

    this->setOnClick(nullptr);
}

void SliderCell::setRange(float minValue, float maxValue)
{
    this->minValue = minValue;
    this->maxValue = maxValue > minValue ? maxValue : minValue + 1.0f;
    this->setValue(this->getValue());
}

void SliderCell::setValue(float value)
{
    const float clamped = std::max(this->minValue, std::min(this->maxValue, value));
    const float progress = (clamped - this->minValue) / (this->maxValue - this->minValue);
    this->slider->setProgress(progress);

    char buffer[64] = { 0 };
    std::snprintf(buffer, sizeof(buffer), "%.2f", clamped);
    this->setDetail(buffer);
}

float SliderCell::getValue() const
{
    return this->minValue + this->slider->getProgress() * (this->maxValue - this->minValue);
}

View* SliderCell::getDefaultFocus()
{
    return this->slider;
}

// ---------------------------------------------------------------- InputCell

InputCell::InputCell(const std::string& title)
    : Cell("cell_input", title)
{
    this->applyTheme();
    this->setOnClick([this] { this->openInput(); });
}

EditTextDialog* InputCell::getDialog()
{
    if (this->dialog != nullptr)
        return this->dialog;

    this->dialog = new EditTextDialog();
    this->dialog->setOnSubmit([this](const std::string& value) { this->submitText(value); });

    return this->dialog;
}

void InputCell::openInput()
{
    EditTextDialog* value = this->getDialog();
    value->setNumeric(this->numeric);
    value->setTitle(this->title);
    value->setText(this->detail);
    value->open();
}

void InputCell::submitText(const std::string& value)
{
    // 数字模式：只留数字与负号（和 borealis 的 InputNumericCell 一样是"收不进就丢"）
    std::string filtered;
    if (this->numeric)
    {
        for (char c : value)
            if ((c >= '0' && c <= '9') || c == '-')
                filtered += c;
    }
    else
    {
        filtered = value;
    }

    this->setDetail(filtered);

    if (this->onSubmit)
        this->onSubmit(filtered);
}

} // namespace bklv

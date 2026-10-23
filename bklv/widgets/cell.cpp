#include "bklv/core/app.hpp"
#include "bklv/widgets/cell.hpp"
#include "bklv/widgets/dropdown.hpp"

#include "bklv/core/attributes.hpp"
#include "bklv/core/json_actions.hpp"
#include "bklv/core/logger.hpp"
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
    // 标题吃掉剩余宽度，右侧内容（说明/开关/滑块）才会贴到单元格右边
    this->titleLabel->setGrow(1.0f);
    this->titleLabel->setShrink(1.0f);
    this->titleLabel->setMinWidth(0.0f);
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
    this->registerAction(ControllerButton::A, translate("hints/ok"), [this](View*) { this->trigger(); });

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
        const std::string override = this->getSoundOverride("clickError");
        if (!override.empty())
            audio::playFile(override);
        else
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
        { "onClick", AttributeKind::STRING, [](View* v, const ResolvedValue& r) {
             Cell* cell = static_cast<Cell*>(v);
             const std::string action = r.text;
             cell->setOnClick([cell, action] {
                 std::string error;
                 if (!invokeJsonAction(action, *cell, error))
                     BKLV_LOG_ERROR("%s", error.c_str());
             });
         }, nullptr },
    };
    return table;
}

// ---------------------------------------------------------------- DetailCell

DetailCell::DetailCell(const std::string& title, const std::string& detail)
    : Cell("base/cell/detail", title)
{
    this->setDetail(detail);
    this->applyTheme(); // 基类构造期不会派发到子类，这里补一次
}

// ---------------------------------------------------------------- BooleanCell

BooleanCell::BooleanCell(const std::string& title, bool value)
    : Cell("base/cell/bool", title)
{
    // 注意：Cell 的构造里调的 applyTheme 是基类那份（构造期不派发虚函数），
    // 所以开关的配色要在这里自己再取一次，否则颜色一直是 0（画不出来）。
    this->applyTheme();
    this->setValue(value);
}

void BooleanCell::setValue(bool value)
{
    const bool changed = (this->value != value);
    this->value        = value;

    // borealis：切换时字号弹一下（1 -> 0.8 -> 1，各 100ms）
    if (changed && this->style == BooleanCellStyle::TEXT)
    {
        this->bounce.stop();
        this->bounce.reset(1.0f);
        this->bounce.addStep(0.8f, 0.1f, EasingFunction::QUADRATIC_OUT);
        this->bounce.addStep(1.0f, 0.1f, EasingFunction::QUADRATIC_IN);
        this->bounce.start();
        this->bouncing = true;
    }

    this->refreshTextMode();

    if (this->onChange)
        this->onChange(value);
}

void BooleanCell::refreshTextMode()
{
    if (this->style != BooleanCellStyle::TEXT)
        return;

    // borealis 的 updateUI()：文字在"开/关"之间切，颜色也跟着切
    this->setDetail(translate(this->value ? "hints/on" : "hints/off"));

    if (this->detailLabel != nullptr)
    {
        this->detailLabel->setTextColorExplicit(true);
        this->detailLabel->setTextColor(
            this->value ? this->valueColor : this->disabledColor);
    }
}

void BooleanCell::stepTextBounce(float deltaSeconds)
{
    if (!this->bouncing)
        return;

    this->bounce.update(deltaSeconds);
    this->textScale = (float)this->bounce;

    if (this->detailLabel != nullptr)
        this->detailLabel->setFontSize(this->baseDetailFontSize * this->textScale);

    if (this->bounce.isStopped())
    {
        this->bouncing   = false;
        this->textScale  = 1.0f;
        if (this->detailLabel != nullptr)
            this->detailLabel->setFontSize(this->baseDetailFontSize);
    }
}

void BooleanCell::update()
{
    Cell::update();
    this->stepTextBounce(getFrameDeltaSeconds());
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
    // 文字模式：开 = value 色，关 = 禁用色（和 borealis 的 updateUI 一致）
    this->valueColor    = theme.color("color.text.value", IM_COL32(88, 195, 169, 255));
    this->disabledColor = theme.color("color.text.disabled", IM_COL32(80, 80, 80, 255));
    this->baseDetailFontSize = theme.number("metric.cell.detailFontSize", 16.0f);

    this->refreshTextMode();
}

void BooleanCell::drawCustomLayer(ImDrawList* drawList, const AppearanceLayer&, float x, float y,
    float width, float height, const AppearanceInstance&)
{
    // 默认（borealis）是文字开/关，没有图形开关
    if (this->style != BooleanCellStyle::SWITCH)
        return;

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
    : Cell("base/cell/radio", title)
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
    this->refreshTitleColor();

    if (this->onChange)
        this->onChange(selected);
}

void RadioCell::refreshTitleColor()
{
    // borealis：选中时标题用 value 色，未选中用正文色
    if (this->titleLabel == nullptr)
        return;

    this->titleLabel->setTextColorExplicit(true);
    this->titleLabel->setTextColor(this->selected
            ? getTheme().color("color.text.value", IM_COL32(88, 195, 169, 255))
            : getTheme().color("color.text.primary", IM_COL32(255, 255, 255, 255)));
}

void RadioCell::applyTheme()
{
    Cell::applyTheme();

    const ThemeData& theme = getTheme();
    this->circleRadius = theme.number("metric.checkbox.radius", 15.0f);
    this->circleColor  = theme.color("color.checkbox.fill", IM_COL32(88, 195, 169, 255));
    this->markColor    = theme.color("color.checkbox.mark", IM_COL32(26, 28, 33, 255));

    this->refreshTitleColor();
}

void RadioCell::drawCustomLayer(ImDrawList* drawList, const AppearanceLayer&, float x, float y,
    float width, float height, const AppearanceInstance&)
{
    // borealis 的 CheckBox：未选中时整块 GONE（什么都不画），选中才画"填充圆 + 对勾"
    if (!this->selected || width <= 0.0f || height <= 0.0f)
        return;

    const float radius = std::min(this->circleRadius,
        std::min(width, height) * 0.5f);
    const float cx     = x + width * 0.5f;
    const float cy     = y + height * 0.5f;

    drawList->AddCircleFilled(ImVec2(cx, cy), radius, this->circleColor, 32);

    // 对勾：两笔（长笔画 -45°，短笔画 +45°），线宽是半径的 10%（同 borealis）
    const float thickness = std::max(1.0f, radius * 0.10f * 2.0f);
    const ImVec2 longFrom(cx - radius * 0.50f, cy + radius * 0.05f);
    const ImVec2 corner(cx - radius * 0.10f, cy + radius * 0.45f);
    const ImVec2 shortTo(cx + radius * 0.55f, cy - radius * 0.45f);

    drawList->AddLine(longFrom, corner, this->markColor, thickness);
    drawList->AddLine(corner, shortTo, this->markColor, thickness);
}

// ---------------------------------------------------------------- SelectorCell

SelectorCell::SelectorCell(const std::string& title)
    : Cell("base/cell/selector", title)
{
    this->applyTheme();
    // 左右键 / LB / RB 换值：和 borealis 的 SelectorCell 一致
    auto next = [this](View*) { this->selectNext(); return true; };
    auto prev = [this](View*) { this->selectPrevious(); return true; };

    this->registerAction(ControllerButton::RB, translate("hints/next"), next);
    this->registerAction(ControllerButton::LB, translate("hints/previous"), prev);
    this->registerAction(ControllerButton::RIGHT, translate("hints/next"), next);
    this->registerAction(ControllerButton::LEFT, translate("hints/previous"), prev);

    // borealis 的 SelectorCell 点击就是弹一个 Dropdown；设了 onOpen 就交给调用方
    this->setOnClick([this] {
        if (this->onOpen)
            this->onOpen();
        else
            this->openDropdown();
    });
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

void SelectorCell::openDropdown()
{
    if (this->options.empty())
        return;

    // 无窗口（测试）里没有 App / 模态栈，退化成切到下一项
    App* app = App::getCurrent();
    if (app == nullptr)
    {
        this->selectNext();
        return;
    }

    if (this->dropdown == nullptr)
    {
        this->dropdown = new Dropdown();
        this->dropdown->setOnChange([this](size_t index) { this->setSelectedIndex((int)index); });
    }

    this->dropdown->setTitle(this->title);
    this->dropdown->setOptions(this->options, (size_t)this->selectedIndex);
    this->dropdown->open();
}

void SelectorCell::refreshDetail()
{
    // borealis 的 updateUI() 就是 detail->setText(data[selection])：**纯文本，没有 ‹ ›**
    this->setDetail(this->getSelectedText());
}

// ---------------------------------------------------------------- SliderCell

SliderCell::SliderCell(const std::string& title)
    : Cell("base/cell/slider", title)
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
    : Cell("base/cell/input", title)
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

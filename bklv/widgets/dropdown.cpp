#include "bklv/widgets/dropdown.hpp"

#include <algorithm>
#include <utility>

#include "bklv/core/app.hpp"
#include "bklv/core/attributes.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

namespace
{

// 触发器：外观驱动的一行（左标题 / 右当前值），能聚焦、按 A 展开
class DropdownTrigger : public AppearanceView
{
  public:
    DropdownTrigger()
        : AppearanceView("dropdown", true)
    {
        // 宽高/内边距来自 widgets/dropdown.json 的 box 段
        this->onPointerAction = true;
        this->enableContentMeasurement();
        this->applyTheme();
    }

    void setTitle(const std::string& value) { this->title = value; }
    void setValue(const std::string& value) { this->value = value; }

    std::function<void()> onActivate;

  protected:
    std::string getLayerText(const std::string& source) override
    {
        if (source == "title")
            return this->title;
        if (source == "value")
            return this->value;

        return AppearanceView::getLayerText(source);
    }

  private:
    std::string title;
    std::string value;
};

} // namespace

// ---------------------------------------------------------------------------
// 选项
// ---------------------------------------------------------------------------

DropdownOption::DropdownOption()
    : AppearanceView("dropdown_item", true)
{
    // 宽高/内边距来自 widgets/dropdown_item.json 的 box 段
    this->onPointerAction = true;
    this->enableContentMeasurement();
    this->registerAction(ControllerButton::A, "选择", [this](View*) { this->trigger(); });
    this->applyTheme();
}

void DropdownOption::setSelected(bool value)
{
    if (this->selected == value)
        return;

    this->selected = value;
    this->setAppearanceState("selected", value);
}

void DropdownOption::trigger()
{
    if (this->onSelect)
        this->onSelect();
}

// ---------------------------------------------------------------------------
// 弹出层
// ---------------------------------------------------------------------------

DropdownPopup::DropdownPopup()
    : Box(Axis::COLUMN)
{
    // 全屏接点击：点到列表外面就关掉（不穿透）
    this->onPointerAction = true;
    this->registerAction(ControllerButton::A, "关闭", [this](View*) { this->dismiss(); });

    this->anchorBox = new Box(Axis::COLUMN);
    this->anchorBox->setPositionType(PositionType::ABSOLUTE);
    this->anchorBox->setWidth(getTheme().number("metric.dropdown.popupWidth", 280.0f));

    this->list = new ScrollingFrame();
    this->list->setWidthPercentage(100.0f);
    
    this->list->setShowScrollingIndicator(false);
    this->anchorBox->addView(this->list);

    this->listContent = new Box(Axis::COLUMN);
    this->listContent->setWidthPercentage(100.0f);
    this->listContent->setHeight(AUTO);
    this->list->setContentView(this->listContent);

    this->addView(this->anchorBox);
}

void DropdownPopup::setOptions(const std::vector<std::string>& names, size_t selected,
    std::function<void(size_t)> onSelect)
{
    for (DropdownOption* option : this->options)
        this->listContent->removeView(option, true);
    this->options.clear();

    for (size_t i = 0; i < names.size(); i++)
    {
        DropdownOption* option = new DropdownOption();
        option->setLabel(names[i]);
        option->setSelected(i == selected);
        option->setOnSelect([this, i, onSelect] {
            if (onSelect)
                onSelect(i);

            // 选完自动关：和系统下拉一致
            this->dismiss();
        });

        this->listContent->addView(option);
        this->options.push_back(option);
    }
}

void DropdownPopup::setAnchor(float x, float y, float width)
{
    this->anchorBox->setPositionLeft(x);
    this->anchorBox->setPositionTop(y);
    this->anchorBox->setWidth(
        std::max(getTheme().number("metric.dropdown.popupMinWidth", 180.0f), width));
}

void DropdownPopup::dismiss()
{
    if (App* app = App::getCurrent())
        app->popOverlay(this, false);

    if (this->onClosed)
        this->onClosed();
}

// ---------------------------------------------------------------------------
// 下拉控件
// ---------------------------------------------------------------------------

Dropdown::Dropdown()
    : Box(Axis::COLUMN)
{
    this->setWidthPercentage(100.0f);
    this->setHeight(AUTO);

    DropdownTrigger* trigger = new DropdownTrigger();
    trigger->onActivate        = [this] { this->open(); };
    this->trigger              = trigger;

    this->addView(trigger);
    this->registerAction(ControllerButton::A, "展开", [this](View*) { this->open(); });
}

void Dropdown::setTitle(const std::string& value)
{
    if (DropdownTrigger* trigger = static_cast<DropdownTrigger*>(this->trigger))
        trigger->setTitle(value);

    this->refreshTriggerText();
}

void Dropdown::setOptions(const std::vector<std::string>& names, size_t selected)
{
    this->options  = names;
    this->selected = names.empty() ? 0 : std::min(selected, names.size() - 1);
    this->refreshTriggerText();
}

void Dropdown::setSelectedIndex(size_t index)
{
    if (this->options.empty())
        return;

    this->selected = std::min(index, this->options.size() - 1);
    this->refreshTriggerText();

    if (this->onChange)
        this->onChange(this->selected);
}

const std::string& Dropdown::getSelectedText() const
{
    static const std::string empty;
    if (this->options.empty())
        return empty;

    return this->options[this->selected];
}

void Dropdown::refreshTriggerText()
{
    DropdownTrigger* trigger = static_cast<DropdownTrigger*>(this->trigger);
    if (trigger != nullptr)
        trigger->setValue(this->getSelectedText());
}

void Dropdown::open()
{
    if (this->opened || this->options.empty())
        return;

    App* app = App::getCurrent();
    if (app == nullptr)
    {
        BKLV_LOG_ERROR("Dropdown::open 失败：找不到 App 实例");
        return;
    }

    if (this->popup == nullptr)
    {
        this->popup = new DropdownPopup();
        this->popup->setOnClosed([this] { this->opened = false; });
    }

    // 列表锚在触发器正下方（触发器的布局矩形此时已经是上一帧的结果）
    const Rect frame = this->trigger->getFrame();
    this->popup->setOptions(this->options, this->selected, [this](size_t index) {
        this->selected = index;
        this->refreshTriggerText();
        if (this->onChange)
            this->onChange(index);
    });
    this->popup->setAnchor(frame.x, frame.getMaxY() + 4.0f, frame.width);

    if (!app->pushOverlay(this->popup))
        return;

    this->opened = true;
}

void Dropdown::close()
{
    this->opened = false;
}

void Dropdown::update()
{
    Box::update();


    // 弹层是不是已经关掉了（点到外面 / 选了某一项都会 popOverlay）
    if (this->opened && App::getCurrent() != nullptr
        && getInputManager().getTopModal() != this->popup)
        this->opened = false;
}

const AttributeTable& Dropdown::attributes() const
{
    static const AttributeTable table = {
        { "title", AttributeKind::Text, [](View* v, const ResolvedValue& r) {
             static_cast<Dropdown*>(v)->setTitle(r.text);
         }, nullptr },
        { "selected", AttributeKind::Number, [](View* v, const ResolvedValue& r) {
             static_cast<Dropdown*>(v)->setSelectedIndex((size_t)r.number);
         }, nullptr },
    };
    return table;
}

} // namespace bklv

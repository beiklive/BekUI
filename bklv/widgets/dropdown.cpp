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

        // **必须自己注册 A 动作**：指针点击只认命中视图自己的动作（不沿父链冒泡），
        // 只把 A 动作挂在 Dropdown 上时鼠标点触发器什么都不做（键盘 A 靠冒泡能开）。
        // 之前 onActivate 是个"设了但没人调"的死成员，鼠标点下拉因此完全没反应。
        this->registerAction(ControllerButton::A, "展开", [this](View*) {
            if (this->onActivate)
                this->onActivate();
        });
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
    // 全屏接点击：点到列表外面就关掉（不穿透）。
    // 注意它**不可聚焦**：指针命中到它自己会返回 nullptr，那时走模态的"空白点击"回调关闭。
    this->onPointerAction = true;
    this->registerAction(ControllerButton::A, "关闭", [this](View*) { this->dismiss(); });

    // 遮罩铺满：先压暗背景，面板再画在它上面（子节点顺序 = 绘制顺序）
    this->mask = new Box(Axis::COLUMN);
    this->mask->setPositionType(PositionType::ABSOLUTE);
    this->mask->setPositionTop(0.0f);
    this->mask->setPositionRight(0.0f);
    this->mask->setPositionBottom(0.0f);
    this->mask->setPositionLeft(0.0f);
    this->addView(this->mask);

    // 那块板是**外观驱动**的：底/圆角/边框/阴影来自 widgets/dropdown_popup.json
    // （以前这里及弹层本身都是裸 Box，主题里注册好的那份外观根本没人用 →
    //  弹出来只有几行悬空文字，没有"面板"）
    this->anchorBox = new DropdownPanel();
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

    this->applyTheme();
}

// 遮罩色跟随主题（`color.common.overlay`：和弹窗遮罩同一个键）
void DropdownPopup::applyTheme()
{
    if (this->mask != nullptr)
        this->mask->setBackgroundColor(getTheme().color("color.common.overlay"));
}

DropdownOption* DropdownPopup::getSelectedOption() const
{
    const size_t index = static_cast<size_t>(this->selectedIndex);
    return index < this->options.size() ? this->options[index] : nullptr;
}

void DropdownPopup::setOptions(const std::vector<std::string>& names, size_t selected,
    std::function<void(size_t)> onSelect)
{
    for (DropdownOption* option : this->options)
        this->listContent->removeView(option, true);
    this->options.clear();
    this->selectedIndex = names.empty() ? 0 : std::min(selected, names.size() - 1);

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

    // 默认焦点 = **当前已选中的那一项**（pushModal 会按 getDefaultFocus 找它）
    this->listContent->setDefaultFocusedIndex(
        this->options.empty() ? -1 : static_cast<int>(this->selectedIndex));
}

void DropdownPopup::setAnchor(float x, float y, float width)
{
    this->anchorBox->setPositionLeft(x);
    this->anchorBox->setPositionTop(y);
    this->anchorBox->setWidth(
        std::max(getTheme().number("metric.dropdown.popupMinWidth", 180.0f), width));
}

// 弹出列表的"列表高度"与位置：优先挂在触发器下方；下方放不下、上方更宽裕就翻到上面；
// 两边都放不下就把列表高度压到能放下的最大值（列表自己可滚动）。
void DropdownPopup::fitToSpace(float triggerTop, float triggerBottom, float gap,
    float canvasHeight, size_t optionCount)
{
    const float margin   = getTheme().number("metric.spacing.xs", 4.0f);
    const float item     = getTheme().number("metric.dropdown.itemHeight", 60.0f);
    const AppearanceBox& panel = this->anchorBox->getActiveAppearance().getBox();
    const float padding        = panel.padding[0] + panel.padding[2]; // top + bottom
    const float needed   = static_cast<float>(optionCount) * item + padding;

    const float below = canvasHeight - (triggerBottom + gap) - margin;
    const float above = triggerTop - gap - margin;

    float height = needed;
    float top    = triggerBottom + gap;

    if (needed > below)
    {
        if (above > below)
        {
            // 翻到触发器上方：列表贴住触发器上沿往下排
            height = std::min(needed, above);
            top    = triggerTop - gap - height;
        }
        else
        {
            // 只能放下面：把高度压到放得下的部分，列表内部滚动
            height = std::max(item, below);
        }
    }

    // anchorBox 的位置也要跟着改（翻到上方时列表贴住触发器上沿往下排）
    this->anchorBox->setPositionTop(top);
    this->list->setHeight(height);
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

    // 列表锚在触发器上/下方（触发器的布局矩形此时已经是上一帧的结果）
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

    // 弹层已经挂到根上了，这时能拿到画布高度 → 决定列表多高、要不要翻到触发器上方
    const float canvasHeight = app->getRootView() != nullptr
        ? app->getRootView()->getHeight()
        : 0.0f;
    this->popup->fitToSpace(frame.y, frame.getMaxY(), 4.0f, canvasHeight, this->options.size());

    // 焦点落到**当前已选中**的那一项（没有就第一项），并把它滚进视野
    // —— pushModal 的默认焦点逻辑按 defaultFocusedIndex 也会找到同一个，
    //    这里显式给一次是为了"上次焦点记忆"不会盖过它。
    if (DropdownOption* selected = this->popup->getSelectedOption())
        getInputManager().giveFocus(selected);
    getInputManager().ensureFocusVisible();

    // 点到列表外面（弹层自己的非可聚焦区域）就关掉。弹层是**非可聚焦**容器，
    // hitTest 返回 nullptr 时走的是模态的"空白点击"回调 —— 正是这个场景。
    getInputManager().setModalEmptyClickHandler([this](const Point&) {
        if (this->popup != nullptr)
            this->popup->dismiss();
    });

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
        { "title", AttributeKind::TEXT, [](View* v, const ResolvedValue& r) {
             static_cast<Dropdown*>(v)->setTitle(r.text);
         }, nullptr },
        { "selected", AttributeKind::NUMBER, [](View* v, const ResolvedValue& r) {
             static_cast<Dropdown*>(v)->setSelectedIndex((size_t)r.number);
         }, nullptr },
    };
    return table;
}

} // namespace bklv

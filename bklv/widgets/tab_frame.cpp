#include "bklv/widgets/tab_frame.hpp"

#include <utility>

#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/h_scrolling_frame.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/scrolling_frame.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

TabButton::TabButton()
    : AppearanceView("base/button/tab_button", true)
{
    // 高度/内边距/图标层全在 base/button/tab_button.<方向>.json 里
    this->setAppearanceVariant("horizontal");

    this->onPointerAction = true;
    this->registerAction(ControllerButton::A, "切换", [this](View*) { this->trigger(); });

    // tab 格子是叶子控件：宽度按"文字 + 两侧 inset"算出来
    this->enableContentMeasurement();

    this->applyTheme();
}

void TabButton::setOrientation(TabOrientation value)
{
    if (this->orientation == value && !this->getAppearanceName().empty())
        return;
    this->orientation = value;
    this->setAppearanceVariant(value == TabOrientation::VERTICAL ? "vertical" : "horizontal");
}

void TabButton::setTitle(const std::string& title)
{
    this->setText(title);
}

void TabButton::setSelected(bool value)
{
    if (this->selected == value)
        return;

    this->selected = value;
    this->setAppearanceState("selected", value);
}

void TabButton::trigger()
{
    if (this->onSelect)
        this->onSelect();
}

void TabButton::onFocusGained()
{
    AppearanceView::onFocusGained();

    // 侧栏靠它做聚焦即切换子页面（borealis 的 SidebarItem::onFocusGained）
    if (this->onFocus)
        this->onFocus();
}

void TabButton::onFocusLost()
{
    AppearanceView::onFocusLost();
}

TabFrame::TabFrame(TabOrientation orientation, TabTriggerMode trigger)
    : Box(Axis::COLUMN)
{
    this->orientation = orientation;
    this->trigger     = trigger;

    this->setWidthPercentage(100.0f);
    this->setHeightPercentage(100.0f);

    // 栏容器（外观按方向切变体）与内容框（两个方向共用同一份外观）
    this->barFrame = new AppearanceView(this->barAppearance, false);
    this->itemBox  = new Box(Axis::ROW);
    this->itemBox->setGap(getTheme().number("metric.spacing.xs", 4.0f));

    this->contentBox = new AppearanceView(this->contentAppearance, false);

    this->rebuildLayout();
}

void TabFrame::rebuildLayout()
{
    const bool horizontal = this->orientation == TabOrientation::HORIZONTAL;
    this->setAxis(horizontal ? Axis::COLUMN : Axis::ROW);

    // 先把两个块摘下来（不销毁），再按位置重新挂
    if (this->barFrame != nullptr && this->barFrame->hasParent())
        this->removeView(this->barFrame, false);
    if (this->contentBox != nullptr && this->contentBox->hasParent())
        this->removeView(this->contentBox, false);

    // 栏内的滚动器按轴重建：条目容器（itemBox）保住，里面是 tab 格子
    if (this->barScroller != nullptr)
    {
        this->barScroller->removeView(this->itemBox, false);
        if (this->barScroller->hasParent())
            this->barFrame->removeView(this->barScroller, true);
        this->barScroller = nullptr;
    }

    if (horizontal)
    {
        auto scroller = new HScrollingFrame();
        scroller->setWidthPercentage(100.0f);
        scroller->setHeight(AUTO);
        this->barScroller = scroller;
        this->itemBox->setAxis(Axis::ROW);
        this->itemBox->setWidthPercentage(100.0f);
    }
    else
    {
        auto scroller = new ScrollingFrame();
        scroller->setWidthPercentage(100.0f);
        scroller->setGrow(1.0f);
        this->barScroller = scroller;
        this->itemBox->setAxis(Axis::COLUMN);
        this->itemBox->setWidthPercentage(100.0f);
    }

    this->barScroller->addView(this->itemBox);
    if (this->barTitleLabel != nullptr && this->barTitleLabel->hasParent())
        this->barFrame->removeView(this->barTitleLabel, false);
    if (this->barTitleLabel != nullptr && !horizontal)
        this->barFrame->addView(this->barTitleLabel);
    this->barFrame->addView(this->barScroller);

    this->barFrame->setAppearanceVariant(horizontal ? "horizontal" : "vertical");
    if (horizontal)
    {
        this->barFrame->setWidthPercentage(100.0f);
        this->barFrame->setHeight(AUTO);
    }
    else
    {
        this->barFrame->setWidth(AUTO); // 宽度由外观的 box.width 决定
        this->barFrame->setHeightPercentage(100.0f);
    }

    // 内容框按轴取尺寸：横向占满宽度、高度由 grow 拿剩余；纵向反过来。
    // 以前两个方向都写 width:100%，纵向时"栏 + 100% 内容"会超出父宽，
    // 内容被压扁 → 页里的 Label/按钮叠在一起（实测）。
    if (horizontal)
    {
        this->contentBox->setWidthPercentage(100.0f);
        this->contentBox->setHeight(AUTO);
    }
    else
    {
        this->contentBox->setWidth(AUTO);
        this->contentBox->setHeightPercentage(100.0f);
    }
    this->contentBox->setGrow(1.0f);
    this->contentBox->setShrink(1.0f);

    // **必须显式调 Box::addView**：TabFrame 重写了 addView 用来接管 TabItem，
    // 内部这两个块要绕过重写，否则会被当成页面内容收进 tab 里（实测：栏和内容框消失）。
    if (this->placement == TabBarPlacement::START)
    {
        Box::addView(this->barFrame);
        Box::addView(this->contentBox);
    }
    else
    {
        Box::addView(this->contentBox);
        Box::addView(this->barFrame);
    }
}

void TabFrame::setOrientation(TabOrientation value)
{
    if (this->orientation == value)
        return;
    this->orientation = value;
    for (TabButton* cell : this->cells)
        cell->setOrientation(value);
    this->rebuildLayout();
}

void TabFrame::setBarPlacement(TabBarPlacement value)
{
    if (this->placement == value)
        return;
    this->placement = value;
    this->rebuildLayout();
}

void TabFrame::setTriggerMode(TabTriggerMode value)
{
    this->trigger = value;
    for (size_t i = 0; i < this->cells.size(); i++)
        this->applyCellTrigger(this->cells[i], i);
}

void TabFrame::applyCellTrigger(TabButton* cell, size_t index)
{
    // CLICK：只有点击 / A 才切页，焦点留在 tab 上
    // FOCUS：焦点移过来就切页；点击 / A 再把握焦点交给内容区
    if (this->trigger == TabTriggerMode::FOCUS)
        cell->setOnFocus([this, index] { this->selectFromInput(index, false); });
    else
        cell->setOnFocus(nullptr);

    cell->setOnSelect([this, index] { this->selectFromInput(index, this->trigger == TabTriggerMode::FOCUS); });
}

View* TabFrame::firstFocusableIn(View* root)
{
    if (root == nullptr)
        return nullptr;
    if (root->isFocusable())
        return root;
    Box* box = root->asBox();
    if (box != nullptr)
    {
        for (View* child : box->getChildren())
            if (View* found = firstFocusableIn(child))
                return found;
    }
    return nullptr;
}

void TabFrame::selectFromInput(size_t index, bool focusContent)
{
    this->setSelectedIndex(index);
    if (!focusContent || index >= this->contents.size())
        return;

    // FOCUS 模式：点击 / A 后焦点进入子页面的默认焦点；没有可聚焦项就留在 tab 上
    if (View* target = firstFocusableIn(this->contents[index]))
        getInputManager().giveFocus(target);
}

size_t TabFrame::addTab(const std::string& title, View* content)
{
    return this->addTab(title, std::string(), content);
}

size_t TabFrame::addTab(const std::string& title, const std::string& icon, View* content)
{
    if (content == nullptr)
        return static_cast<size_t>(-1);

    const size_t index = this->cells.size();

    TabButton* cell = new TabButton();
    cell->setAppearanceName(this->buttonAppearance);
    cell->setOrientation(this->orientation);
    if (this->orientation == TabOrientation::HORIZONTAL)
        cell->setGrow(1.0f); // 横向栏里等分宽度
    cell->setTitle(title);
    if (!icon.empty())
        cell->setIcon(icon);

    this->applyCellTrigger(cell, index);
    this->itemBox->addView(cell);
    this->cells.push_back(cell);

    this->contents.push_back(content);
    if (index == this->selected)
        this->contentBox->addView(content);

    return index;
}

size_t TabFrame::addTab(TabButton* button, View* content)
{
    if (button == nullptr || content == nullptr)
        return static_cast<size_t>(-1);

    const size_t index = this->cells.size();

    button->setOrientation(this->orientation);
    this->applyCellTrigger(button, index);
    this->itemBox->addView(button);
    this->cells.push_back(button);

    this->contents.push_back(content);
    if (index == this->selected)
        this->contentBox->addView(content);
    return index;
}

void TabFrame::setBarAppearance(const std::string& name)
{
    this->barAppearance = name;
    this->barFrame->setAppearanceName(name);
    this->barFrame->setAppearanceVariant(
        this->orientation == TabOrientation::HORIZONTAL ? "horizontal" : "vertical");
}

void TabFrame::setContentAppearance(const std::string& name)
{
    this->contentAppearance = name;
    this->contentBox->setAppearanceName(name);
}

void TabFrame::setTabButtonAppearance(const std::string& name)
{
    this->buttonAppearance = name;
    for (TabButton* cell : this->cells)
    {
        cell->setAppearanceName(name);
        cell->setOrientation(this->orientation);
    }
}

void TabFrame::setTabTitle(size_t index, const std::string& title)
{
    if (index < this->cells.size())
        this->cells[index]->setTitle(title);
}

void TabFrame::setTabIcon(size_t index, const std::string& icon)
{
    if (index < this->cells.size())
        this->cells[index]->setIcon(icon);
}

void TabFrame::addSeparator()
{
    if (this->orientation == TabOrientation::HORIZONTAL)
    {
        BKLV_LOG_WARNING("TabFrame::addSeparator：横向 tab 不支持分组线，已忽略");
        return;
    }

    Box* line = new Box(Axis::COLUMN);
    line->setWidthPercentage(100.0f);
    line->setHeight(getTheme().number("metric.sidebar.separatorHeight", 1.0f));
    line->setBackgroundColor(getTheme().color("color.sidebar.separator"));
    this->itemBox->addView(line);
}

void TabFrame::setTitle(const std::string& title)
{
    // 栏标题：栏容器最上方的一行小字（纵向侧栏常见；横向时是栏左端）
    this->barTitle = title;
    if (this->barTitleLabel == nullptr)
    {
        this->barTitleLabel = new Label();
        this->barTitleLabel->setFontSize(getTheme().number("metric.font.size.xs", 12.0f));
        this->barTitleLabel->setTextColor(getTheme().color("color.text.dim"));
        this->rebuildLayout(); // 顺序：标题 → 条目
    }
    this->barTitleLabel->setText(title);
}

void TabFrame::setSelectedIndex(size_t index)
{
    if (index >= this->contents.size() || index == this->selected)
        return;

    this->contentBox->removeView(this->contents[this->selected], false);
    this->contentBox->addView(this->contents[index]);
    this->selected = index;

    for (size_t i = 0; i < this->cells.size(); i++)
        this->cells[i]->setSelected(i == index);
}


// ---- TabItem ----
TabItem::TabItem()
    : Box(Axis::COLUMN)
{
    this->setWidthPercentage(100.0f);
    this->setGrow(1.0f);
}

namespace
{

void setTabItemTitle(View* v, const ResolvedValue& value)
{
    static_cast<TabItem*>(v)->setTabTitle(value.text);
}

void setTabItemIcon(View* v, const ResolvedValue& value)
{
    static_cast<TabItem*>(v)->setTabIcon(value.text);
}

void setFrameOrientation(View* v, const ResolvedValue& value)
{
    static_cast<TabFrame*>(v)->setOrientation(
        value.text == "vertical" ? TabOrientation::VERTICAL : TabOrientation::HORIZONTAL);
}

void setFramePlacement(View* v, const ResolvedValue& value)
{
    static_cast<TabFrame*>(v)->setBarPlacement(
        value.text == "end" ? TabBarPlacement::END : TabBarPlacement::START);
}

void setFrameTrigger(View* v, const ResolvedValue& value)
{
    static_cast<TabFrame*>(v)->setTriggerMode(
        value.text == "focus" ? TabTriggerMode::FOCUS : TabTriggerMode::CLICK);
}

void setFrameTitle(View* v, const ResolvedValue& value)
{
    static_cast<TabFrame*>(v)->setTitle(value.text);
}

void setFrameButton(View* v, const ResolvedValue& value)
{
    static_cast<TabFrame*>(v)->setTabButtonAppearance(value.text);
}

void setFrameBar(View* v, const ResolvedValue& value)
{
    static_cast<TabFrame*>(v)->setBarAppearance(value.text);
}

void setFrameContent(View* v, const ResolvedValue& value)
{
    static_cast<TabFrame*>(v)->setContentAppearance(value.text);
}

} // namespace

const AttributeTable& TabItem::attributes() const
{
    static const AttributeTable table = {
        { "title", AttributeKind::TEXT, setTabItemTitle, nullptr },
        { "icon", AttributeKind::TEXT, setTabItemIcon, nullptr },
    };
    return table;
}

const AttributeTable& TabFrame::attributes() const
{
    static const AttributeTable table = {
        { "orientation", AttributeKind::ENUM, setFrameOrientation, "horizontal|vertical" },
        { "placement", AttributeKind::ENUM, setFramePlacement, "start|end" },
        { "trigger", AttributeKind::ENUM, setFrameTrigger, "click|focus" },
        { "title", AttributeKind::TEXT, setFrameTitle, nullptr },
        { "button", AttributeKind::STRING, setFrameButton, nullptr },
        { "bar", AttributeKind::STRING, setFrameBar, nullptr },
        { "content", AttributeKind::STRING, setFrameContent, nullptr },
    };
    return table;
}

// ---- TabFrame：建树时接管子节点 ----
void TabFrame::addView(View* view)
{
    if (view == nullptr)
        return;

    if (view->getClassString() != "bklv::TabItem")
    {
        // 普通 View：直接当成一个 tab 的页面内容（C++ 手写场景）
        this->addTab(std::string(), view);
        return;
    }

    TabItem* item = static_cast<TabItem*>(view);
    const std::string title = item->getTabTitle();
    const std::string icon  = item->getTabIcon();

    // TabItem 的子节点搬到页面框里；描述节点自己删掉（它不参与布局）
    Box* page = new Box(Axis::COLUMN);
    page->setWidthPercentage(100.0f);
    page->setGrow(1.0f);
    page->setAlignItems(AlignItems::STRETCH);
    for (View* child : item->getChildren())
        page->addView(child, page->getChildrenCount());
    item->removeAllViews();
    delete item;

    this->addTab(title, icon, page);
}

} // namespace bklv

#include "bklv/widgets/tab_frame.hpp"

#include <utility>

#include "bklv/core/theme_data.hpp"

namespace bklv
{

TabCell::TabCell()
    : AppearanceView("base/frame/tab_button", true)
{
    // 高度/内边距/图标层全在 base/frame/tab_button.<方向>.json 里
    this->setAppearanceVariant("horizontal");

    this->onPointerAction = true;
    this->registerAction(ControllerButton::A, "切换", [this](View*) { this->trigger(); });

    // tab 格子是叶子控件：宽度按"文字 + 两侧 inset"算出来
    this->enableContentMeasurement();

    this->applyTheme();
}

void TabCell::setOrientation(TabOrientation value)
{
    if (this->orientation == value && !this->getAppearanceName().empty())
        return;
    this->orientation = value;
    this->setAppearanceVariant(value == TabOrientation::VERTICAL ? "vertical" : "horizontal");
}

void TabCell::setTitle(const std::string& title)
{
    this->setText(title);
}

void TabCell::setSelected(bool value)
{
    if (this->selected == value)
        return;

    this->selected = value;
    this->setAppearanceState("selected", value);
}

void TabCell::trigger()
{
    if (this->onSelect)
        this->onSelect();
}

void TabCell::onFocusGained()
{
    AppearanceView::onFocusGained();

    // 侧栏靠它做聚焦即切换子页面（borealis 的 SidebarItem::onFocusGained）
    if (this->onFocus)
        this->onFocus();
}

void TabCell::onFocusLost()
{
    AppearanceView::onFocusLost();
}

TabFrame::TabFrame()
    : Box(Axis::COLUMN)
{
    this->setWidthPercentage(100.0f);
    this->setHeightPercentage(100.0f);

    // tab 栏：横向，可滚动（tab 多的时候）
    this->tabBar = new Box(Axis::ROW);
    this->tabBar->setWidthPercentage(100.0f);
    this->tabBar->setHeight(AUTO);
    this->tabBar->setGap(getTheme().number("metric.spacing.sm", 8.0f));
    this->tabBar->setMarginBottom(getTheme().number("metric.spacing.md", 12.0f));
    this->addView(this->tabBar);

    // 内容区：撑满剩余空间
    this->contentBox = new Box(Axis::COLUMN);
    this->contentBox->setWidthPercentage(100.0f);
    this->contentBox->setGrow(1.0f);
    this->contentBox->setShrink(1.0f);
    this->addView(this->contentBox);
}

void TabFrame::addTab(const std::string& title, View* content)
{
    if (content == nullptr)
        return;

    const size_t index = this->cells.size();

    TabCell* cell = new TabCell();
    cell->setTitle(title);

    // borealis 的 TabFrame::addTab 里写得很明确：
    // "Only trigger when the sidebar item gains focus" —— 聚焦到哪个 tab 就切到哪一页。
    // （我们的改进：内容不销毁，切回来状态还在；borealis 会把旧 tab 删掉重建。）
    cell->setOnFocus([this, index] { this->setSelectedIndex(index); });
    cell->setOnSelect([this, index] { this->setSelectedIndex(index); });

    this->tabBar->addView(cell);
    this->cells.push_back(cell);

    // 内容全部由 TabFrame 持有，但只有当前选中的挂在树上
    contents.push_back(content);

    if (index == this->selected)
        this->contentBox->addView(content);
}

void TabFrame::setTabTitle(size_t index, const std::string& title)
{
    if (index >= this->cells.size())
        return;
    this->cells[index]->setTitle(title);
}

void TabFrame::setSelectedIndex(size_t index)
{
    if (index >= this->contents.size() || index == this->selected)
        return;

    // 旧页面脱离但不销毁：切回来时状态还在
    this->contentBox->removeView(this->contents[this->selected], false);
    this->contentBox->addView(this->contents[index]);

    this->selected = index;

    for (size_t i = 0; i < this->cells.size(); i++)
        this->cells[i]->setSelected(i == index);
}

} // namespace bklv

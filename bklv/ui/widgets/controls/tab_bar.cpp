#include "bklv/ui/widgets/controls/tab_bar.hpp"

#include <utility>

#include "bklv/foundation/core/i18n.hpp"
#include "bklv/ui/input/input.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/view/view.hpp"

using namespace bklv;

namespace
{

const char* kIconSlot = "icon";
const char* kLabelSlot = "label";

} // namespace

// ---- TabItem ---------------------------------------------------------------

TabItem::TabItem(const std::string& label, const std::string& glyph, bool vertical)
    : AppearanceView("base/tab/tabitem", true), label(label), glyph(glyph), vertical(vertical)
{
    this->setAppearanceVariant(vertical ? "vertical" : "");
    this->setText(kLabelSlot, this->label);
    this->setIcon(kIconSlot, this->glyph);

    // 点一下等同按确认（和 Button 一个套路）
    this->onPointerAction = true;
    this->registerAction(ControllerButton::A, translate("hints/ok"), [this](View*) {
        if (this->onActivate)
            this->onActivate();
    });

    this->applyTheme();
    this->applyLayout();
    this->setSelected(false);
}

TabItem::~TabItem() = default;

void TabItem::applyLayout()
{
    // 两种排列里，项的内部都是"图标 + 文字"横排；差别在宽度/对齐（外观 JSON 里）
    this->setAxis(Axis::ROW);
    this->invalidate();
}

void TabItem::setVertical(bool vertical)
{
    if (this->vertical == vertical)
        return;

    this->vertical = vertical;
    this->setAppearanceVariant(vertical ? "vertical" : "");
    this->applyTheme();
    this->applyLayout();
}

void TabItem::setSelected(bool selected)
{
    this->selected = selected;

    // states.on / states.off 在 JSON 里各写一套（选中态换底色、文字色、指示条）
    this->setAppearanceState("on", selected);
    this->setAppearanceState("off", !selected);
    this->invalidate();
}

// ---- TabBar ----------------------------------------------------------------

TabBar::TabBar(Orientation orientation)
    : AppearanceView("base/tab/tabbar", false), orientation(orientation)
{
    this->applyLayout();
    this->applyTheme();
}

TabBar::~TabBar() = default;

void TabBar::applyLayout()
{
    const bool vertical = this->orientation == Orientation::VERTICAL;

    this->setAppearanceVariant(vertical ? "vertical" : "");
    this->setAxis(vertical ? Axis::COLUMN : Axis::ROW);
    this->invalidate();
}

int TabBar::addItem(const std::string& label, const std::string& glyph, const std::string& id)
{
    auto* item = new TabItem(label, glyph, this->isVertical());
    this->addView(item);

    const int index = (int)this->items.size();
    this->items.push_back(item);
    this->ids.push_back(id);

    item->setOnActivate([this, index]() { this->setActiveIndex(index, true); });

    if (this->active < 0)
        this->setActiveIndex(0, false);

    return index;
}

void TabBar::setOrientation(Orientation orientation)
{
    if (this->orientation == orientation)
        return;

    this->orientation = orientation;
    this->applyLayout();

    for (TabItem* item : this->items)
        item->setVertical(this->isVertical());

    this->invalidate();
}

void TabBar::setActiveIndex(int index, bool notify)
{
    if (index < 0 || index >= (int)this->items.size())
        return;

    this->active = index;
    for (size_t i = 0; i < this->items.size(); i++)
        this->items[i]->setSelected((int)i == index);

    this->invalidate();

    if (notify && this->onTabSelected)
        this->onTabSelected(index);
}

TabItem* TabBar::getItem(int index) const
{
    if (index < 0 || index >= (int)this->items.size())
        return nullptr;
    return this->items[(size_t)index];
}

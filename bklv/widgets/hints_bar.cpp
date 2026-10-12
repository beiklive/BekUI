#include "bklv/widgets/hints_bar.hpp"

#include <string>

#include "bklv/core/font.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

HintItemView::HintItemView()
    : AppearanceView("hint_item", false)
{
    // 内边距/高度来自 widgets/hint_item.json 的 box 段（borealis 的 4/16）
    this->enableContentMeasurement();
    this->applyTheme();
}

void HintItemView::setButton(int button)
{
    // -1 表示"这次用文字键名，不要图标"
    this->icon = button < 0 ? std::string() : getControllerButtonIcon((ControllerButton)button);
}

void HintItemView::setLabel(const std::string& text)
{
    this->label = text;
    this->markMeasureDirty();
    this->invalidate();
}

ImFont* HintItemView::getLayerFont(const AppearanceLayer& layer)
{
    // 图标层走图标字体：私有区字形只有它有
    if (layer.id == "icon")
        return getIconFont();

    return AppearanceView::getLayerFont(layer);
}

std::string HintItemView::getLayerText(const std::string& source)
{
    if (source == "icon")
        return this->icon;
    if (source == "label")
        return this->label;

    return AppearanceView::getLayerText(source);
}

HintsBar::HintsBar(const std::string&)
    : Box(Axis::ROW)
{
    // 宽高/内边距/间距/不压缩 全在 widgets/hints_bar.json 的 box 段里
}

void HintsBar::refresh()
{
    const std::vector<HintItem>& hints = getInputManager().getHints();

    // 先把签名拼出来：焦点没变就不重建（重建会不断触发 Yoga 重排）
    std::string signature;
    for (const HintItem& hint : hints)
    {
        if (hint.hintText.empty())
            continue;
        signature += std::to_string((int)hint.button) + ":" + hint.hintText + "|";
    }

    if (signature == this->lastSignature)
        return;


    this->lastSignature = signature;

    for (HintItemView* item : this->items)
        this->removeView(item, true);
    this->items.clear();

    for (const HintItem& hint : hints)
    {
        if (hint.hintText.empty())
            continue;

        // 只在有图标字体时用图标字形；没有就退回键名文字，免得显示成方块
        HintItemView* item  = new HintItemView();
        const bool useIcon  = hasIconFont();

        item->setButton(useIcon ? (int)hint.button : -1);
        item->setLabel(useIcon ? hint.hintText
                               : std::string(getControllerButtonName(hint.button)) + " "
                                     + hint.hintText);
        this->addView(item);
        this->items.push_back(item);
    }

    if (this->hideWhenEmpty)
        this->setVisibility(this->items.empty() ? Visibility::GONE : Visibility::VISIBLE);
}

void HintsBar::update()
{
    Box::update();




    this->refresh();
}

} // namespace bklv

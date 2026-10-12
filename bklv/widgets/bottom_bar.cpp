#include "bklv/widgets/bottom_bar.hpp"

#include "bklv/core/theme_data.hpp"

namespace bklv
{

BottomBar::BottomBar(const std::string& variant)
    : AppearanceView(variant.empty() ? "bottom_bar" : "bottom_bar." + variant, false)
{
    // 高度/外边距/内边距/上边线都在 bottom_bar.json 的 box 段与 layers 里
    this->setAxis(Axis::ROW);
    this->hints = new HintsBar();
    this->addView(this->hints);

    // 右侧状态区自己 grow 吸走剩余宽度并右对齐
    this->status = new StatusArea();
    this->status->setGrow(1.0f);
    this->status->setJustifyContent(JustifyContent::FLEX_END);
    this->addView(this->status);
}

void BottomBar::applyTheme()
{
    AppearanceView::applyTheme();
}

} // namespace bklv

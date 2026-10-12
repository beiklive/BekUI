#include "bklv/widgets/bottom_bar.hpp"
#include "bklv/views/padding.hpp"

#include "bklv/core/theme_data.hpp"

namespace bklv
{

BottomBar::BottomBar(const std::string& variant)
    : AppearanceView(variant.empty() ? "bottom_bar" : "bottom_bar." + variant, false)
{
    // 高度/外边距/内边距/上边线都在 bottom_bar.json 的 box 段与 layers 里
    this->setAxis(Axis::ROW);
    this->hints = new HintsBar();
    
    this->status = new StatusArea();

    this->addView(this->status);
    this->addView(new bklv::Padding());
    this->addView(this->hints);
}

void BottomBar::applyTheme()
{
    AppearanceView::applyTheme();
}

} // namespace bklv

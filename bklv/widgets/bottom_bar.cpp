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

    // 顺序按 borealis 的 bottomBarXML：Hints 在前、Status 在后，中间用 Padding 顶开
    // （AGENTS.md 的约定 + widget_test 有断言；之前被改成 status 在前，断言一直红）
    this->addView(this->hints);
    this->addView(new bklv::Padding());
    this->addView(this->status);
}

void BottomBar::applyTheme()
{
    AppearanceView::applyTheme();
}

} // namespace bklv

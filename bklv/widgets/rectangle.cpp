#include "bklv/widgets/rectangle.hpp"

#include "bklv/core/attributes.hpp"

namespace bklv
{

namespace
{

// 矩形上可用的额外属性。底/边框/圆角这些走类级外观表；
// 这里只暴露"用哪个 variant 的外观"和几个常用覆盖项。
void setVariantAttr(View* v, const ResolvedValue& r)
{
    // 运行时换 variant 相当于换一份外观
    static_cast<Rectangle*>(v)->setAppearanceVariant(r.text);
}

} // namespace

Rectangle::Rectangle(const std::string& variant)
    : AppearanceView(variant.empty() ? "rectangle" : "rectangle." + variant, false)
{
    this->setHeight(AUTO);
    this->applyTheme();
}

const AttributeTable& Rectangle::attributes() const
{
    static const AttributeTable table = {
        { "variant", AttributeKind::String, setVariantAttr, nullptr },
    };
    return table;
}

} // namespace bklv

#include "bklv/widgets/header.hpp"

#include "bklv/core/attributes.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

namespace
{

void setHeaderTitleAttr(View* v, const ResolvedValue& r)
{
    static_cast<Header*>(v)->setTitle(r.text);
}

void setHeaderSubtitleAttr(View* v, const ResolvedValue& r)
{
    static_cast<Header*>(v)->setSubtitle(r.text);
}

void setHeaderVariantAttr(View* v, const ResolvedValue& r)
{
    static_cast<Header*>(v)->setAppearanceVariant(r.text);
}

} // namespace

Header::Header(const std::string& variant)
    : AppearanceView(variant.empty() ? "header" : "header." + variant, false)
{
    // 高度来自主题；宽度铺满父容器
    this->setWidthPercentage(100.0f);
    this->setHeight(getTheme().number("metric.header.height", 48.0f));
    this->setShrink(0.0f); // 页头不参与压缩
    this->enableContentMeasurement();

    this->applyTheme();
}

void Header::setTitle(const std::string& text)
{
    this->title = text;
}

void Header::setSubtitle(const std::string& text)
{
    this->subtitle = text;
}

std::string Header::getLayerText(const std::string& source)
{
    if (source == "title")
        return this->title;
    if (source == "subtitle")
        return this->subtitle;

    return AppearanceView::getLayerText(source);
}

const AttributeTable& Header::attributes() const
{
    static const AttributeTable table = {
        { "title", AttributeKind::Text, setHeaderTitleAttr, nullptr },
        { "subtitle", AttributeKind::Text, setHeaderSubtitleAttr, nullptr },
        { "variant", AttributeKind::String, setHeaderVariantAttr, nullptr },
    };
    return table;
}

} // namespace bklv

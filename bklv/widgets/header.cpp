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

void setHeaderIconAttr(View* v, const ResolvedValue& r)
{
    static_cast<Header*>(v)->setIcon(r.text);
}

void setHeaderVariantAttr(View* v, const ResolvedValue& r)
{
    static_cast<Header*>(v)->setAppearanceVariant(r.text);
}

} // namespace

HeaderIcon::HeaderIcon()
    : AppearanceView("header_icon", false)
{
    // 图标尺寸来自 header_icon.json 的 box；固有尺寸由 image 层的纹理决定
    this->enableContentMeasurement();
    this->applyTheme();
}

TextureRef HeaderIcon::getLayerImage(const std::string&)
{
    return this->texture;
}

Header::Header(const std::string& variant)
    : AppearanceView(variant.empty() ? "header" : "header." + variant, false)
{
    // 页头是"一行三格"：图标 / 标题 / 右侧区，尺寸留白都在主题里
    this->icon = new HeaderIcon();
    this->addView(this->icon);

    this->titleLabel = new Label();
    this->addView(this->titleLabel);

    this->hintBox = new Box(Axis::ROW);
    this->hintBox->setHeight(AUTO);
    this->hintBox->setGrow(1.0f);
    this->hintBox->setAlignItems(AlignItems::CENTER);
    this->hintBox->setJustifyContent(JustifyContent::FLEX_END);
    this->addView(this->hintBox);

    this->subtitleLabel = new Label();
    this->subtitleLabel->setSingleLine(true);
    this->hintBox->addView(this->subtitleLabel);

    this->applyTheme();
}

void Header::setTitle(const std::string& text)
{
    this->title = text;
    this->titleLabel->setText(text);
}

void Header::setSubtitle(const std::string& text)
{
    this->subtitle = text;
    this->subtitleLabel->setText(text);
    this->subtitleLabel->setVisibility(text.empty() ? Visibility::GONE : Visibility::VISIBLE);
}

void Header::setIcon(const std::string& path)
{
    if (path.empty())
    {
        this->icon->setTexture(TextureRef());
    }
    else
    {
        // 图标文件按资源目录解析（和 Image::setImageFromFile 一致）
        this->icon->setTexture(loadTexture(path));
    }

    this->icon->setVisibility(this->icon->hasTexture() ? Visibility::VISIBLE : Visibility::GONE);
    this->invalidate();
}

void Header::refreshLabels()
{
    this->titleLabel->setText(this->title);
    this->subtitleLabel->setText(this->subtitle);
    this->subtitleLabel->setVisibility(this->subtitle.empty() ? Visibility::GONE : Visibility::VISIBLE);
}

void Header::applyTheme()
{
    AppearanceView::applyTheme();

    const ThemeData& theme = getTheme();

    // 标题/副标题是 Label：字号与颜色来自主题键（换肤、改缩放都会重算）
    this->titleLabel->setFontSize(theme.number("metric.header.titleFontSize", 28.0f));
    this->titleLabel->setTextColorExplicit(false);

    this->subtitleLabel->setFontSize(theme.number("metric.font.size.sm", 15.0f));
    this->subtitleLabel->setTextColorExplicit(false);

    if (this->title.empty())
        this->icon->setVisibility(Visibility::GONE);
}

const AttributeTable& Header::attributes() const
{
    static const AttributeTable table = {
        { "title", AttributeKind::Text, setHeaderTitleAttr, nullptr },
        { "subtitle", AttributeKind::Text, setHeaderSubtitleAttr, nullptr },
        { "icon", AttributeKind::ResourcePath, setHeaderIconAttr, nullptr },
        { "variant", AttributeKind::String, setHeaderVariantAttr, nullptr },
    };
    return table;
}

} // namespace bklv

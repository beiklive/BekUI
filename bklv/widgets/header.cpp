#include "bklv/widgets/header.hpp"

#include <algorithm>

#include "bklv/views/padding.hpp"

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

void setHeaderLargeTitleAttr(View* v, const ResolvedValue& r)
{
    static_cast<Header*>(v)->setLargeTitle(r.text);
}

} // namespace

HeaderIcon::HeaderIcon()
    : AppearanceView("base/header_icon", false)
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
    : AppearanceView(variant.empty() ? "widget/header" : "widget/header." + variant, false)
{
    // 页头是"一行三格"：图标 / 标题 / 右侧区，尺寸留白都在主题里
    this->icon = new HeaderIcon();
    this->addView(this->icon);

    this->titleLabel = new Label();
    this->addView(this->titleLabel);

    // 标题与右侧区之间用一个 Padding 顶开（borealis 的 header XML 就是
    // [矩形][标题][Padding][副标题]），比"给右侧区加 grow"更直白
    this->padding = new Padding();
    this->addView(this->padding);

    this->hintBox = new Box(Axis::ROW);
    this->hintBox->setHeight(AUTO);
    this->hintBox->setAlignItems(AlignItems::CENTER);
    this->hintBox->setJustifyContent(JustifyContent::FLEX_END);
    this->addView(this->hintBox);

    this->subtitleLabel = new Label();
    this->subtitleLabel->setSingleLine(true);
    this->hintBox->addView(this->subtitleLabel);

    // 大标题：绝对定位在页头下沿之外（不占页头布局），没设文本时隐藏。
    // 位置在 applyTheme() 里按主题尺寸给（换肤 / 改 uiScale 会重算）。
    this->largeTitleLabel = new Label();
    this->largeTitleLabel->setSingleLine(true);
    this->largeTitleLabel->setPositionType(PositionType::ABSOLUTE);
    this->largeTitleLabel->setVisibility(Visibility::GONE);
    this->addView(this->largeTitleLabel);

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
    this->largeTitleLabel->setText(this->largeTitle);
}

void Header::setLargeTitle(const std::string& text)
{
    this->largeTitle = text;
    this->largeTitleLabel->setText(text);
    this->largeTitleLabel->setVisibility(text.empty() ? Visibility::GONE : Visibility::VISIBLE);

    // 没有大标题就回到"页头小标题常显"，否则设空之后标题会一直透明
    this->setCollapseProgress(text.empty() ? 0.0f : this->collapseProgress);
    if (text.empty())
        this->titleLabel->setAlpha(1.0f);
}

void Header::setCollapseDistance(float pixels)
{
    this->collapseDistance = pixels > 0.0f ? pixels : 1.0f;
}

void Header::setCollapseProgress(float progress)
{
    this->collapseProgress = std::clamp(progress, 0.0f, 1.0f);

    // 没有大标题：页头小标题常显，进度不参与（避免"标题一直看不见"这种坑）
    if (this->largeTitle.empty())
    {
        this->titleLabel->setAlpha(1.0f);
        return;
    }

    const float t = this->collapseProgress;

    // 全部是绘制期量（alpha / 字体缩放 / 位移）→ 零重排
    this->titleLabel->setAlpha(t);
    this->largeTitleLabel->setAlpha(1.0f - t);

    const float scale = 1.0f - 0.12f * t;
    this->largeTitleLabel->setFontScaleX(scale);
    this->largeTitleLabel->setFontScaleY(scale);
    this->largeTitleLabel->setTranslateY(-8.0f * t);
}

void Header::update()
{
    // 约定：先递归子树，再干自己的事
    AppearanceView::update();

    if (this->collapseSource == nullptr)
        return;

    // 内容还没滚（=0）时展开大标题，滚过一个行高之后完全收起
    const float offset   = this->collapseSource->getContentOffsetY();
    const float distance = this->collapseDistance > 0.0f ? this->collapseDistance : 1.0f;
    this->setCollapseProgress(offset / distance);
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

    // 大标题：iOS 的 largeTitle 就是"比页头标题大一号、左对齐、贴在栏下面"
    const float barHeight = theme.number("metric.appletFrame.headerHeight",
        theme.number("metric.header.height", 50.0f));
    this->largeTitleLabel->setFontSize(theme.number("metric.font.size.title", 28.0f));
    this->largeTitleLabel->setTextColor(theme.color("color.text.primary"));
    this->largeTitleLabel->setPositionLeft(0.0f);
    this->largeTitleLabel->setPositionTop(barHeight + theme.number("metric.spacing.sm", 8.0f));

    if (this->title.empty())
        this->icon->setVisibility(Visibility::GONE);
}

const AttributeTable& Header::attributes() const
{
    static const AttributeTable table = {
        { "title", AttributeKind::TEXT, setHeaderTitleAttr, nullptr },
        { "subtitle", AttributeKind::TEXT, setHeaderSubtitleAttr, nullptr },
        { "icon", AttributeKind::RESOURCE_PATH, setHeaderIconAttr, nullptr },
        { "variant", AttributeKind::STRING, setHeaderVariantAttr, nullptr },
        { "largeTitle", AttributeKind::TEXT, setHeaderLargeTitleAttr, nullptr },
    };
    return table;
}

} // namespace bklv

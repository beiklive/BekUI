#include "bklv/ui/widgets/controls/notification.hpp"

#include <algorithm>

#include "bklv/foundation/anim/time.hpp"
#include "bklv/ui/theme/theme_data.hpp"

namespace bklv
{

namespace
{
// 动效时长（秒）。0.22 是照着"看得见但不耽误事"定的：
// 再短就看不出是滑进来的，再长点两下会觉得通知被"塞"住。
constexpr float kAppearSeconds  = 0.22f;
constexpr float kDismissSeconds = 0.22f;

// 内容尺寸兜底（主题里没给值时用）
constexpr float kIconSizeDefault  = 24.0f;
constexpr float kImageSizeDefault = 44.0f;
constexpr float kGapDefault       = 10.0f;
constexpr float kRadiusDefault    = 6.0f;

constexpr ImU32 kTextDefault = IM_COL32(255, 255, 255, 255);
} // namespace

Notification::Notification(NotificationKind kind, const std::string& text,
    const std::string& glyphOrImagePath, float duration)
    : AppearanceView("base/widget/notification", false)
{
    this->kind        = kind;
    this->text        = text;
    this->glyphOrPath = glyphOrImagePath;
    this->duration    = duration > 0.0f ? duration : 3.0f;
    this->remaining   = this->duration;

    this->setupContent();

    // 卡片外观（底色 / 描边 / 阴影）在 JSON 里，内容样式在这里上：
    // applyTheme() 会同时刷新两者，换肤不用重建通知。
    this->applyTheme();

    // 初始停在屏幕右侧外：appearProgress = 0 时位移最大
    this->setTranslateX(this->slideDistance);
}

Notification::Notification(const std::string& text, float duration)
    : Notification(NotificationKind::TEXT, text, std::string(), duration)
{
}

Notification* Notification::withIcon(
    const std::string& text, const std::string& glyph, float duration)
{
    return new Notification(NotificationKind::ICON, text, glyph, duration);
}

Notification* Notification::withImage(
    const std::string& text, const std::string& imagePath, float duration)
{
    return new Notification(NotificationKind::IMAGE, text, imagePath, duration);
}

void Notification::setupContent()
{
    // 卡片本身已经带着外观层的背景/描边/内边距，这里只往里塞一行内容：
    //   [图标 或 图片] [文案]
    this->row = new Box(Axis::ROW);
    this->row->setAlignItems(AlignItems::CENTER);
    this->addView(this->row);

    if (this->kind == NotificationKind::IMAGE)
    {
        this->image = new Image();
        this->image->setScalingType(ImageScalingType::FILL);
        this->image->setImageFromFileAsync(this->glyphOrPath);
        this->row->addView(this->image);
    }
    else if (this->kind == NotificationKind::ICON)
    {
        this->iconLabel = new Label();
        this->iconLabel->setIconFontEnabled(true);
        this->iconLabel->setText(this->glyphOrPath);
        this->iconLabel->setVerticalAlign(VerticalAlign::CENTER);
        this->row->addView(this->iconLabel);
    }

    this->textLabel = new Label();
    this->textLabel->setSingleLine(true);
    this->textLabel->setText(this->text);
    this->textLabel->setVerticalAlign(VerticalAlign::CENTER);
    this->row->addView(this->textLabel);
}

void Notification::setText(const std::string& value)
{
    this->text = value;
    if (this->textLabel != nullptr)
        this->textLabel->setText(value);
    else
        this->setupContent();
}

void Notification::applyContentTheme()
{
    const ThemeData& theme = getTheme();

    if (this->row != nullptr)
        this->row->setGap(theme.number("metric.notification.contentGap", kGapDefault));

    const ImU32 textColor = theme.color("color.notification.text", kTextDefault);
    const float fontSize  = theme.number("metric.notification.fontSize", 18.0f);

    if (this->textLabel != nullptr)
    {
        this->textLabel->setFontSize(fontSize);
        this->textLabel->setTextColor(textColor);
    }

    if (this->iconLabel != nullptr)
    {
        this->iconLabel->setFontSize(theme.number("metric.notification.iconSize", kIconSizeDefault));
        this->iconLabel->setTextColor(textColor);
    }

    if (this->image != nullptr)
    {
        const float size = theme.number("metric.notification.imageSize", kImageSizeDefault);
        this->image->setWidth(size);
        this->image->setHeight(size);
        this->image->setCornerRadius(
            theme.number("metric.notification.imageRadius", kRadiusDefault));
    }

    // 滑动距离 = 卡片宽度 + 右边距 + 余量。多给一点，保证完全移出可视区，
    // 否则边缘会漏一条（踩过：正好等于宽度时右边缘还看得见）。
    const float cardWidth   = theme.number("metric.notification.width", 360.0f);
    const float marginRight = theme.number("metric.notification.marginRight", 16.0f);
    this->slideDistance     = cardWidth + marginRight + 32.0f;
}

void Notification::applyTheme()
{
    AppearanceView::applyTheme();
    this->applyContentTheme();

    // 外观重套之后把当前动画进度重新映射到位移上（否则换肤会把它"瞬移"回原位）
    if (!this->dismissed)
        this->setTranslateX((1.0f - this->appearProgress) * this->slideDistance);
    else
        this->setTranslateX(this->dismissProgress * this->slideDistance);
}

void Notification::dismiss()
{
    if (this->dismissed)
        return;

    this->dismissed = true;
    // 记下当前高度：消失时逐步收成 0，下面那几条才会跟着往上挪，而不是"跳一下"
    this->fullHeight = this->getHeight();

    // 淡出仍走外观状态（JSON 里的 shown / hidden）
    this->setAppearanceState("shown", false);
    this->setAppearanceState("hidden", true);
}

void Notification::update()
{
    AppearanceView::update();

    const float delta = getFrameDeltaSeconds();
    if (delta <= 0.0f)
        return;

    if (!this->dismissed)
    {
        if (this->appearProgress < 1.0f)
        {
            this->appearProgress =
                std::min(1.0f, this->appearProgress + delta / kAppearSeconds);
            this->setTranslateX((1.0f - this->appearProgress) * this->slideDistance);
        }

        this->remaining -= delta;
        if (this->remaining <= 0.0f)
            this->dismiss();
        return;
    }

    if (this->dismissProgress >= 1.0f)
        return;

    if (this->fullHeight <= 0.0f)
        this->fullHeight = this->getHeight();

    this->dismissProgress = std::min(1.0f, this->dismissProgress + delta / kDismissSeconds);
    this->setTranslateX(this->dismissProgress * this->slideDistance);

    if (this->fullHeight > 0.0f)
        this->setHeight(this->fullHeight * (1.0f - this->dismissProgress));
}

// ---------------------------------------------------------------- 管理器

NotificationManager::NotificationManager()
    : Box(Axis::COLUMN)
{
    // 右上角一列：自己不吃焦点，也不挡下面的控件（外层是绝对定位的浮层）
    this->setHeight(AUTO);
    this->setAlignItems(AlignItems::FLEX_END);
    this->setJustifyContent(JustifyContent::FLEX_START);
}

Notification* NotificationManager::add(Notification* item)
{
    if (item == nullptr)
        return nullptr;

    this->items.push_back(item);
    this->addView(item); // 不设上限：一列往下排，排到屏幕外由外层裁掉
    this->invalidate();
    return item;
}

Notification* NotificationManager::notify(const std::string& text, float duration)
{
    return this->add(new Notification(text, duration));
}

Notification* NotificationManager::notifyIcon(
    const std::string& text, const std::string& glyph, float duration)
{
    return this->add(Notification::withIcon(text, glyph, duration));
}

Notification* NotificationManager::notifyImage(
    const std::string& text, const std::string& imagePath, float duration)
{
    return this->add(Notification::withImage(text, imagePath, duration));
}

void NotificationManager::cleanup()
{
    auto it = this->items.begin();
    while (it != this->items.end())
    {
        Notification* item = *it;
        // 只有"滑回去 + 高度收完"才真正摘掉；光标记 dismissed 时它还在占位
        if (item->isFinished())
        {
            this->removeView(item, true);
            it = this->items.erase(it);
            continue;
        }
        ++it;
    }
}

void NotificationManager::update()
{
    Box::update();
    this->cleanup();
}

void NotificationManager::applyTheme()
{
    const ThemeData& theme = getTheme();
    this->setGap(theme.number("metric.notification.gap", 8.0f));
}

} // namespace bklv

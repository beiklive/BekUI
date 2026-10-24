#include "bklv/ui/widgets/controls/notification.hpp"

#include <algorithm>

#include "bklv/foundation/anim/time.hpp"
#include "bklv/ui/theme/theme_data.hpp"
#include "bklv/ui/widgets/basic/image.hpp"

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

// ---------------------------------------------------------------- 基类

Notification::Notification(
    const std::string& text, float duration, const std::string& appearanceName)
    : AppearanceView(appearanceName, false)
{
    this->text      = text;
    this->duration  = duration > 0.0f ? duration : 3.0f;
    this->remaining = this->duration;

    this->setupBaseContent();

    // 卡片外观（底色 / 描边 / 阴影）在 JSON 里，内容样式在这里上：
    // applyTheme() 会同时刷新两者，换肤不用重建通知。
    this->applyTheme();

    // 初始停在屏幕右侧外：appearProgress = 0 时位移最大
    this->setTranslateX(this->slideDistance);
}

void Notification::setupBaseContent()
{
    // 卡片本身已经带着外观层的背景/描边/内边距，这里只往里塞一行内容：
    //   [图标 或 图片] [文案]
    // 图标/图片由派生类的 buildContent() 插到第 0 位，文案固定在后。
    this->row = new Box(Axis::ROW);
    this->row->setAlignItems(AlignItems::CENTER);
    this->addView(this->row);

    this->textLabel = new Label();
    this->textLabel->setSingleLine(true);
    this->textLabel->setText(this->text);
    this->textLabel->setVerticalAlign(VerticalAlign::CENTER);
    this->row->addView(this->textLabel);
}

void Notification::applyContentTheme()
{
    const ThemeData& theme = getTheme();

    if (this->row != nullptr)
        this->row->setGap(theme.number("metric.notification.contentGap", kGapDefault));

    if (this->textLabel != nullptr)
    {
        this->textLabel->setFontSize(theme.number("metric.notification.fontSize", 18.0f));
        this->textLabel->setTextColor(theme.color("color.notification.text", kTextDefault));
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
    // 消失动效全在代码里（滑回右侧 + 高度收缩），不依赖外观 JSON 里的状态：
    // 卡片外观只负责"长什么样"，动效由本类驱动。
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
            this->appearProgress = std::min(1.0f, this->appearProgress + delta / kAppearSeconds);
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

// ---------------------------------------------------------------- 图标 + 文字

IconNotification::IconNotification(
    const std::string& text, const std::string& glyph, float duration)
    : Notification(text, duration, "base/notification/icon")
{
    this->glyph = glyph;

    // 派生类构造体里调用虚函数是安全的（此时对象已经是派生类）
    this->buildContent();
    this->applyTheme();
}

void IconNotification::buildContent()
{
    this->iconLabel = new Label();
    this->iconLabel->setIconFontEnabled(true);
    this->iconLabel->setText(this->glyph);
    this->iconLabel->setVerticalAlign(VerticalAlign::CENTER);

    // 插到内容行最前面：图标在左、文案在右
    this->getContentRow()->addView(this->iconLabel, 0);
}

void IconNotification::setIcon(const std::string& glyph)
{
    this->glyph = glyph;
    if (this->iconLabel != nullptr)
        this->iconLabel->setText(glyph);
}

void IconNotification::applyContentTheme()
{
    Notification::applyContentTheme();

    if (this->iconLabel == nullptr)
        return;

    const ThemeData& theme = getTheme();
    this->iconLabel->setFontSize(theme.number("metric.notification.iconSize", kIconSizeDefault));
    this->iconLabel->setTextColor(theme.color("color.notification.text", kTextDefault));
}

// ---------------------------------------------------------------- 图片 + 文字

ImageNotification::ImageNotification(
    const std::string& text, const std::string& imagePath, float duration)
    : Notification(text, duration, "base/notification/image")
{
    this->imagePath = imagePath;

    this->buildContent();
    this->applyTheme();
}

void ImageNotification::buildContent()
{
    this->image = new Image();
    this->image->setScalingType(ImageScalingType::FILL);
    this->image->setImageFromFileAsync(this->imagePath);

    this->getContentRow()->addView(this->image, 0);
}

void ImageNotification::applyContentTheme()
{
    Notification::applyContentTheme();

    if (this->image == nullptr)
        return;

    const ThemeData& theme = getTheme();
    const float size       = theme.number("metric.notification.imageSize", kImageSizeDefault);

    this->image->setWidth(size);
    this->image->setHeight(size);
    this->image->setCornerRadius(theme.number("metric.notification.imageRadius", kRadiusDefault));
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

Notification* NotificationManager::notifyIcon(
    const std::string& text, const std::string& glyph, float duration)
{
    return this->add(new IconNotification(text, glyph, duration));
}

Notification* NotificationManager::notifyImage(
    const std::string& text, const std::string& imagePath, float duration)
{
    return this->add(new ImageNotification(text, imagePath, duration));
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

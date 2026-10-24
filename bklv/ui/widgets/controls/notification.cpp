#include "bklv/ui/widgets/controls/notification.hpp"

#include <algorithm>

#include "bklv/foundation/anim/time.hpp"
#include "bklv/foundation/core/resource.hpp"
#include "bklv/graphics/render/graphics.hpp"
#include "bklv/ui/theme/theme_data.hpp"

namespace bklv
{

namespace
{
// 动效时长（秒）。0.22 是照着"看得见但不耽误事"定的：
// 再短就看不出是滑进来的，再长点两下会觉得通知被"塞"住。
constexpr float kAppearSeconds  = 0.22f;
constexpr float kDismissSeconds = 0.22f;
} // namespace

// ---------------------------------------------------------------- 基类

Notification::Notification(
    const std::string& text, float duration, const std::string& appearanceName)
    : AppearanceView(appearanceName, false)
{
    this->text      = text;
    this->duration  = duration > 0.0f ? duration : 3.0f;
    this->remaining = this->duration;

    // 文案走内容槽：外观 JSON 里 `"data_id": "text"` 的那一层负责画它
    this->setText(notification_slot::TEXT, this->text);

    // 外观（底色 / 描边 / 阴影 / 图标与文字的层）全在 JSON 里，换肤不用重建通知
    this->applyTheme();

    // 初始停在屏幕右侧外：appearProgress = 0 时位移最大
    this->setTranslateX(this->slideDistance);
}

void Notification::setTextContent(const std::string& text)
{
    this->text = text;
    this->setText(notification_slot::TEXT, this->text);
}

void Notification::updateSlideDistance()
{
    // 滑动距离 = 卡片宽度 + 右边距 + 余量。多给一点，保证完全移出可视区，
    // 否则边缘会漏一条（踩过：正好等于宽度时右边缘还看得见）。
    const ThemeData& theme  = getTheme();
    const float cardWidth   = theme.number("metric.notification.width", 360.0f);
    const float marginRight = theme.number("metric.notification.marginRight", 16.0f);
    this->slideDistance     = cardWidth + marginRight + 32.0f;
}

void Notification::applyTheme()
{
    AppearanceView::applyTheme();
    this->updateSlideDistance();

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
    : Notification(text, duration, "base/notification/toast_icon")
{
    this->glyph = glyph;
    // 图标走内容槽：外观里 `"data_id": "icon"` 的 text 层（font_kind: icon）负责画它
    AppearanceView::setIcon(notification_slot::ICON, this->glyph);
}

void IconNotification::setIcon(const std::string& glyph)
{
    this->glyph = glyph;
    AppearanceView::setIcon(notification_slot::ICON, this->glyph);
}

// ---------------------------------------------------------------- 图片 + 文字

ImageNotification::ImageNotification(
    const std::string& text, const std::string& imagePath, float duration)
    : Notification(text, duration, "base/notification/toast_image")
{
    this->imagePath = imagePath;
    // 图片走内容槽：外观里 `"data_id": "image"` 的 image 层负责画它
    this->reloadImage();
}

void ImageNotification::setImagePath(const std::string& imagePath)
{
    this->imagePath = imagePath;
    this->reloadImage();
}

void ImageNotification::reloadImage()
{
    if (this->imagePath.empty())
    {
        this->clearSlot(notification_slot::IMAGE);
        return;
    }

    // 缩略图直接取纹理：同一路径第二次就命中纹理缓存，不再解码
    this->setImage(notification_slot::IMAGE, loadTexture(resourcePath(this->imagePath)));
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

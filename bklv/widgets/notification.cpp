#include "bklv/widgets/notification.hpp"

#include "bklv/core/theme_data.hpp"
#include "bklv/core/time.hpp"

namespace bklv
{

Notification::Notification(const std::string& text, float duration)
    : AppearanceView("notification", false)
{
    this->setText(text);
    this->duration  = duration > 0.0f ? duration : 0.0f;
    this->remaining = this->duration;

    // 出现动效：先 hidden（初值），再切 shown（和 Dialog 的 open/closed 一个套路）。
    // 两个状态同时激活不行——hidden 的透明度 0 会把 shown 盖掉（踩过）。
    this->setAppearanceState("hidden", true);
    this->setAppearanceState("hidden", false);
    this->setAppearanceState("shown", true);

    this->applyTheme();
}

void Notification::setText(const std::string& value)
{
    this->text = value;
    if (this->label == nullptr)
    {
        this->label = new Label();
        this->label->setSingleLine(true);
        this->addView(this->label);
    }

    this->label->setText(value);
}

void Notification::dismiss()
{
    if (this->dismissed)
        return;

    this->dismissed = true;
    this->setAppearanceState("shown", false);
    this->setAppearanceState("hidden", true);
}

void Notification::update()
{
    AppearanceView::update();

    if (this->dismissed)
        return;

    const float delta = getFrameDeltaSeconds();
    if (delta <= 0.0f)
        return;

    this->remaining -= delta;
    if (this->remaining <= 0.0f)
        this->dismiss();
}

// ---------------------------------------------------------------- 管理器

NotificationManager::NotificationManager(size_t maxVisible)
    : Box(Axis::COLUMN)
{
    this->maxVisible = maxVisible > 0 ? maxVisible : 1;

    // 右上角一列：自己不吃焦点，也不能挡住下面的控件
    this->setHeight(AUTO);
    this->setAlignItems(AlignItems::FLEX_END);
    this->setJustifyContent(JustifyContent::FLEX_START);
    // 管理器本身不参与焦点（它是浮层）
}

Notification* NotificationManager::notify(const std::string& text, float duration)
{
    Notification* item = new Notification(text, duration);
    this->items.push_back(item);
    this->addView(item);

    // 超出上限：最旧的立刻消失（它自己会在动画结束后被 cleanup 摘掉）
    const size_t alive = this->items.size();
    if (alive > this->maxVisible)
    {
        size_t toDismiss = alive - this->maxVisible;
        for (Notification* old : this->items)
        {
            if (toDismiss == 0)
                break;
            if (!old->isDismissed())
            {
                old->dismiss();
                toDismiss--;
            }
        }
    }

    return item;
}

void NotificationManager::cleanup()
{
    auto it = this->items.begin();
    while (it != this->items.end())
    {
        Notification* item = *it;
        // 消失动效播完（外观不再动画）且已经标记消失，就真正移除
        if (item->isDismissed() && !item->isAppearanceAnimating())
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

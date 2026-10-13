#include "bklv/widgets/account.hpp"

#include "bklv/core/theme_data.hpp"
#include "bklv/views/image.hpp"

namespace bklv
{

namespace
{

// 取 UTF-8 的第一个字符（不是第一个字节），中文/emoji 都不会被切坏
std::string firstUtf8Char(const std::string& text)
{
    if (text.empty())
        return "";

    const unsigned char lead = (unsigned char)text[0];
    size_t length            = 1;
    if ((lead & 0x80) == 0x00)
        length = 1;
    else if ((lead & 0xE0) == 0xC0)
        length = 2;
    else if ((lead & 0xF0) == 0xE0)
        length = 3;
    else if ((lead & 0xF8) == 0xF0)
        length = 4;

    if (length > text.size())
        length = 1;

    return text.substr(0, length);
}

} // namespace

AccountWidget::AccountWidget()
    : AppearanceView("widget/account", false)
{
    // 头像：有图就用图（盖住自绘的圆形），没图就露出圆形 + 首字
    this->avatar = new Image();
    // 尺寸跟主题一致，和自绘的圆形层对齐（有图时正好盖住它）
    const float avatarSize = getTheme().number("metric.account.avatarSize", 32.0f);
    this->avatar->setDimensions(avatarSize, avatarSize);
    this->avatar->setScalingType(ImageScalingType::FILL);
    this->addView(this->avatar);

    Box* textBox = new Box(Axis::COLUMN);
    textBox->setGrow(1.0f);
    textBox->setShrink(1.0f);

    this->nameLabel = new Label();
    this->nameLabel->setSingleLine(true);
    textBox->addView(this->nameLabel);

    this->subtitleLabel = new Label();
    this->subtitleLabel->setSingleLine(true);
    textBox->addView(this->subtitleLabel);

    this->addView(textBox);

    this->applyTheme();
    this->refreshLabels();
}

void AccountWidget::setProvider(std::function<AccountInfo()> value)
{
    this->provider = value != nullptr ? std::move(value) : [] { return AccountInfo{}; };
    this->refresh();
}

void AccountWidget::refresh()
{
    if (this->provider != nullptr)
        this->setAccount(this->provider());
}

void AccountWidget::setAccount(const AccountInfo& value)
{
    this->info    = value;
    this->initial = firstUtf8Char(value.name);
    this->refreshLabels();

    if (this->avatar != nullptr)
    {
        if (!value.avatarPath.empty())
            this->avatar->setImageFromFileAsync(value.avatarPath);
        else
            this->avatar->setImage(TextureRef());
    }

    this->invalidate();
}

void AccountWidget::setName(const std::string& value)
{
    this->info.name = value;
    this->initial   = firstUtf8Char(value);
    this->refreshLabels();
}

void AccountWidget::setSubtitle(const std::string& value)
{
    this->info.subtitle = value;
    this->refreshLabels();
}

void AccountWidget::setAvatar(const std::string& path)
{
    this->info.avatarPath = path;
    if (this->avatar != nullptr)
    {
        if (path.empty())
            this->avatar->setImage(TextureRef());
        else
            this->avatar->setImageFromFileAsync(path);
    }
}

bool AccountWidget::hasAvatarImage() const
{
    return this->avatar != nullptr && this->avatar->getTexture().isValid();
}

void AccountWidget::refreshLabels()
{
    if (this->nameLabel != nullptr)
        this->nameLabel->setText(this->info.name);

    if (this->subtitleLabel != nullptr)
    {
        this->subtitleLabel->setText(this->info.subtitle);
        this->subtitleLabel->setVisibility(
            this->info.subtitle.empty() ? Visibility::GONE : Visibility::VISIBLE);
    }
}

void AccountWidget::applyTheme()
{
    AppearanceView::applyTheme();

    const ThemeData& theme = getTheme();

    if (this->nameLabel != nullptr)
    {
        this->nameLabel->setFontSize(theme.number("metric.account.nameFontSize", 17.0f));
        this->nameLabel->setTextColorExplicit(false);
    }
    if (this->subtitleLabel != nullptr)
    {
        this->subtitleLabel->setFontSize(theme.number("metric.account.subtitleFontSize", 14.0f));
        this->subtitleLabel->setTextColorExplicit(false);
    }

    this->avatarColor  = theme.color("color.account.avatar", IM_COL32(0, 255, 204, 255));
    this->initialColor = theme.color("color.account.initial", IM_COL32(24, 26, 30, 255));
}

std::string AccountWidget::getLayerText(const std::string& source)
{
    if (source == "initial")
        return this->hasAvatarImage() ? std::string() : this->initial;

    return AppearanceView::getLayerText(source);
}

} // namespace bklv

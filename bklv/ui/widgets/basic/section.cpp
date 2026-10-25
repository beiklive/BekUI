#include "section.hpp"

namespace bklv
{

Section::Section(const std::string& title, const std::string& info)
    : AppearanceView("base/section/section", false),
      title(title),
      info(info)
{
    // 文字由层画，不建 Label 子视图：这是"标题 + 一条竖条"，不需要参与子视图测量。
    this->refreshContent();
    this->applyTheme();
}

Section::~Section() = default;

void Section::setTitle(const std::string& value)
{
    if (this->title == value)
        return;

    this->title = value;
    this->refreshContent();
    this->invalidate();
}

void Section::setInfo(const std::string& value)
{
    if (this->info == value)
        return;

    this->info = value;
    this->refreshContent();
    this->invalidate();
}

void Section::refreshContent()
{
    this->setText(Section::TITLE_SLOT, this->title);
    this->setText(Section::INFO_SLOT, this->info);
}

void Section::applyTheme()
{
    AppearanceView::applyTheme();
    // 外观换了之后层还在，但文字得重新灌一遍（setText 按槽名找层）
    this->refreshContent();
}

} // namespace bklv

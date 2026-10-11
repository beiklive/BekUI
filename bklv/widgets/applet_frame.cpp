#include "bklv/widgets/applet_frame.hpp"

#include <utility>

#include "bklv/core/input.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

AppletFrame::AppletFrame(View* contentView)
    : Box(Axis::COLUMN)
{
    this->setWidthPercentage(100.0f);
    this->setHeightPercentage(100.0f);

    this->header = new Header();
    this->addView(this->header);

    // 内容区：撑满页头与底栏之间的空间
    this->contentBox = new Box(Axis::COLUMN);
    this->contentBox->setWidthPercentage(100.0f);
    this->contentBox->setGrow(1.0f);
    this->contentBox->setShrink(1.0f);
    this->contentBox->setMarginTop(getTheme().number("metric.spacing.md", 12.0f));
    this->addView(this->contentBox);

    this->footer = new BottomBar();
    this->addView(this->footer);

    // B：逐层返回；已经在最外层就交给 onDismiss
    this->registerAction(ControllerButton::B, "返回", [this](View*) {
        if (this->contentStack.size() > 1)
        {
            this->popContentView();
            return;
        }

        if (this->onDismiss)
            this->onDismiss();
    });

    if (contentView != nullptr)
    {
        this->contentStack.push_back(contentView);
        this->attachContentView(contentView);
    }

    this->refreshFooter();
}

AppletFrame::~AppletFrame()
{
    // 内容栈里的视图由本框架持有；当前挂在树上的那个会被 Box 析构回收，
    // 其余脱离的这里补上，避免泄漏。
    for (View* view : this->contentStack)
    {
        if (view != nullptr && view != this->contentView && view->getParent() == nullptr)
            delete view;
    }
}

void AppletFrame::setTitle(const std::string& title)
{
    this->header->setTitle(title);
}

void AppletFrame::setSubtitle(const std::string& subtitle)
{
    this->header->setSubtitle(subtitle);
}

void AppletFrame::setHeaderVisibility(Visibility visibility)
{
    this->header->setVisibility(visibility);
}

void AppletFrame::setFooterVisibility(Visibility visibility)
{
    this->footer->setVisibility(visibility);
}

void AppletFrame::setHeaderStyle(AppletHeaderStyle style)
{
    // Popup 走另一份外观文件（widgets/header.popup.json），代码不用改
    this->header->setAppearanceVariant(style == AppletHeaderStyle::Popup ? "popup" : "");
}

void AppletFrame::attachContentView(View* view)
{
    if (view == nullptr)
        return;

    if (this->contentView != nullptr && this->contentView->getParent() == this->contentBox)
        this->contentBox->removeView(this->contentView, false);

    this->contentView = view;
    this->contentBox->addView(view);
    this->refreshFooter();
}

void AppletFrame::pushContentView(View* view)
{
    if (view == nullptr)
        return;

    this->contentStack.push_back(view);
    this->attachContentView(view);
}

void AppletFrame::popContentView(std::function<void()> callback)
{
    if (this->contentStack.size() <= 1)
    {
        if (callback)
            callback();
        return;
    }

    this->contentStack.pop_back();
    this->attachContentView(this->contentStack.back());

    // 退回上一层时把焦点放到新内容里的第一个可聚焦项上
    if (View* inner = this->contentBox->getDefaultFocus())
        getInputManager().giveFocus(inner);

    if (callback)
        callback();
}

void AppletFrame::refreshFooter()
{
    if (this->footer == nullptr)
        return;

    // 底栏显示当前焦点的动作提示（和焦点绑定的提示由输入层给）
    this->footer->showHintsFromFocus();
    if (this->footer->getText().empty())
        this->footer->setText(this->contentStack.size() > 1 ? "B 返回" : "");
}

void AppletFrame::update()
{
    Box::update();
}

} // namespace bklv

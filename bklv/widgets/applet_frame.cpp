#include "bklv/widgets/applet_frame.hpp"

#include <utility>

#include "bklv/core/input.hpp"
#include "bklv/core/theme_data.hpp"

namespace bklv
{

AppletFrame::AppletFrame(View* contentView)
    : Box(Axis::COLUMN)
{
    // 外壳尺寸与页面底色来自 widgets/applet_frame.json 的 box 段

    this->header = new Header();
    // 应用外壳的页头比独立页头高（88，borealis 的 applet_frame/header_height）：
    // 走 header 的 "applet" 变体，也就是另一份 JSON，而不是在代码里改它的高度
    this->header->setAppearanceVariant("applet");
    this->addView(this->header);

    // 内容区：撑满页头与底栏之间的空间
    this->contentBox = new Box(Axis::COLUMN);
    this->contentBox->setWidthPercentage(100.0f);
    this->contentBox->setGrow(1.0f);
    this->contentBox->setShrink(1.0f);
    this->contentBox->setMarginTop(getTheme().number("metric.appletFrame.contentTop", 12.0f));
    this->addView(this->contentBox);

    // 底栏：默认是提示栏（[按键图标] 说明 …），这也正是 borealis 的做法
    this->hintsBar = new HintsBar();
    this->addView(this->hintsBar);

    this->footer = new BottomBar();
    this->footer->setVisibility(Visibility::GONE); // 文字底栏默认不显示
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

    this->applyTheme();

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

void AppletFrame::applyTheme()
{
    // 尺寸 + 页面底色（对应 borealis 的 brls/background）都来自 themes/<主题>/widgets/applet_frame.json
    applyWidgetBox(this, "applet_frame");
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
    this->hintsBar->setVisibility(visibility == Visibility::VISIBLE && this->useTextFooter
            ? Visibility::GONE
            : visibility);
}

void AppletFrame::setUseTextFooter(bool value)
{
    this->useTextFooter = value;
    this->hintsBar->setVisibility(value ? Visibility::GONE : Visibility::VISIBLE);
    this->footer->setVisibility(value ? Visibility::VISIBLE : Visibility::GONE);
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
    // 提示栏的内容由 HintsBar::update() 按当前焦点刷新（这里是切页后的即时同步）
    if (this->hintsBar != nullptr)
        this->hintsBar->refresh();
}

void AppletFrame::update()
{
    Box::update();
}

} // namespace bklv

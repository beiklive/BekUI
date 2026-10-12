#include "bklv/widgets/applet_frame.hpp"

#include <algorithm>
#include <utility>

#include "bklv/core/app.hpp"
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
    // 内容子节点默认横向拉满：这样应用给内容加"auto 宽 + 左右 margin"时，
    // 拿到的是"内容区宽 - 边距"，不会像 100% + margin 那样多出边距那一段
    this->contentBox->setAlignItems(AlignItems::STRETCH);
    this->contentBox->setGrow(1.0f);
    this->contentBox->setShrink(1.0f);
    this->addView(this->contentBox);

    // 底栏：一个控件同时装提示（左）与状态区（右），对应 borealis 的 BottomBar
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

void AppletFrame::setHeaderVisible(bool visible)
{
    if (this->headerVisible == visible)
        return;

    this->headerVisible = visible;

    if (this->barsAnimated)
    {
        this->startBarAnimation();
        return;
    }

    this->header->setVisibility(visible ? Visibility::VISIBLE : Visibility::GONE);
    App::requestLayout();
}

void AppletFrame::setBottomBarVisible(bool visible)
{
    if (this->bottomBarVisible == visible)
        return;

    this->bottomBarVisible = visible;

    if (this->barsAnimated)
    {
        this->startBarAnimation();
        return;
    }

    this->footer->setVisibility(visible ? Visibility::VISIBLE : Visibility::GONE);
    App::requestLayout();
}

// 折叠补间：把栏高从整高补到 0（或反过来）。
// 动画期间每帧写一次高度并请求重排——布局是"一帧最多一次"的，所以不会白算。
void AppletFrame::startBarAnimation()
{
    const float headerFull = getTheme().number("metric.appletFrame.headerHeight", 88.0f);
    const float footerFull = getTheme().number("metric.appletFrame.footerHeight", 73.0f);

    this->headerAnim.reset(this->headerVisible ? 0.0f : 1.0f);
    this->headerAnim.addStep(this->headerVisible ? 1.0f : 0.0f, this->barDuration);

    this->footerAnim.reset(this->bottomBarVisible ? 0.0f : 1.0f);
    this->footerAnim.addStep(this->bottomBarVisible ? 1.0f : 0.0f, this->barDuration);

    // 动画期间必须是可见的（不然 GONE 了就没有高度可补）
    this->header->setVisibility(Visibility::VISIBLE);
    this->footer->setVisibility(Visibility::VISIBLE);
    this->header->setHeight(std::max(1.0f, headerFull * this->headerAnim.getValue()));
    this->footer->setHeight(std::max(1.0f, footerFull * this->footerAnim.getValue()));

    // 两个补间都结束时再收敛（期间每帧写高度 + 请求重排）
    this->headerAnim.setEndCallback([this](bool) { this->finishBarAnimation(); });
    this->footerAnim.setEndCallback([this](bool) { this->finishBarAnimation(); });

    this->animating = true;
    this->headerAnim.start();
    this->footerAnim.start();

    App::requestLayout();
}

void AppletFrame::updateBars()
{
    if (!this->animating)
        return;

    const float headerFull = getTheme().number("metric.appletFrame.headerHeight", 88.0f);
    const float footerFull = getTheme().number("metric.appletFrame.footerHeight", 73.0f);

    const float headerRatio = (float)this->headerAnim;
    const float footerRatio = (float)this->footerAnim;

    this->header->setHeight(std::max(1.0f, headerFull * headerRatio));
    this->footer->setHeight(std::max(1.0f, footerFull * footerRatio));

    App::requestLayout(); // 补间期间每帧都要重排（布局是延迟的，一帧最多一次）
}

// 两段补间都跑完：把该藏的栏真正 GONE 掉（这样空间是平滑让出来的）
void AppletFrame::finishBarAnimation()
{
    if (!this->headerAnim.isStopped() || !this->footerAnim.isStopped())
        return;

    this->animating = false;

    if (!this->headerVisible)
    {
        this->header->setHeight(getTheme().number("metric.appletFrame.headerHeight", 88.0f));
        this->header->setVisibility(Visibility::GONE);
    }
    else
    {
        this->header->setHeight(getTheme().number("metric.appletFrame.headerHeight", 88.0f));
    }

    if (!this->bottomBarVisible)
    {
        this->footer->setHeight(getTheme().number("metric.appletFrame.footerHeight", 73.0f));
        this->footer->setVisibility(Visibility::GONE);
    }
    else
    {
        this->footer->setHeight(getTheme().number("metric.appletFrame.footerHeight", 73.0f));
    }

    App::requestLayout();
}

void AppletFrame::setHeaderVisibility(Visibility visibility)
{
    this->setHeaderVisible(visibility == Visibility::VISIBLE);
}

void AppletFrame::setFooterVisibility(Visibility visibility)
{
    this->setBottomBarVisible(visibility == Visibility::VISIBLE);
}

void AppletFrame::setHeaderStyle(AppletHeaderStyle style)
{
    // Popup 走另一份外观文件（widgets/header.popup.json），代码不用改
    this->header->setAppearanceVariant(style == AppletHeaderStyle::POPUP ? "popup" : "");
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
    // 提示按当前焦点刷新（HintsBar 自己每帧也会按签名差异刷，这里是切页后的即时同步）
    if (this->footer != nullptr)
        this->footer->refreshHints();
}

void AppletFrame::update()
{
    Box::update();
    this->updateBars();
}

} // namespace bklv

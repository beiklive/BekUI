#include "app/demo_app.hpp"

#include "app/pages.hpp"

using namespace bklv;

namespace demo
{

DemoApp::DemoApp()
{
    this->shell = new Shell();
    this->shell->setGlassEnabled(true);

    this->header = new Header();
    this->header->setAppearanceVariant("applet");
    this->header->setTitle(getStr("app/title"));
    this->shell->setHeaderView(this->header);
    this->shell->getOverlayLayer()->addView(new Padding());
    this->shell->setHeaderInset(
        getTheme().number("metric.appletFrame.headerHeight", 88.0f));

    // 内容：统一的 TabFrame（横向 / 栏在顶部 / 点击切换）
    // demo 只有一个页面；将来加页面就是多一个 addTab。
    this->contentRoot = new Box(Axis::COLUMN);
    this->contentRoot->setWidthPercentage(100.0f);
    this->contentRoot->setHeightPercentage(100.0f);
    this->contentRoot->setAlignItems(AlignItems::STRETCH);

    this->frame = new TabFrame(TabOrientation::HORIZONTAL, TabTriggerMode::CLICK);
    this->frame->setTitle(getStr("app/title"));
    this->contentRoot->addView(this->frame);

    this->shell->pushContentView(this->contentRoot);

    BKLV_LOG_INFO("demo 已就绪：1 个页面（图标总览）");
}

DemoApp::~DemoApp() = default;

void DemoApp::update()
{
    if (this->pageSelected)
        return;

    // 首帧建页：图标字形要等 App::run() 建好字体图集之后才扫得到
    this->pageSelected = true;

    ScrollingFrame* scroller = new ScrollingFrame();
    scroller->setWidthPercentage(100.0f);
    scroller->setGrow(1.0f);
    scroller->setShowScrollingIndicator(true);
    scroller->addView(buildIconsPage());

    this->frame->addTab(getStr("tab/icons"), scroller);
}

} // namespace demo

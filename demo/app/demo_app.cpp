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

    // 内容：滚动槽（页面挂这里）+ 底部 Tab 栏
    this->contentRoot = new Box(Axis::COLUMN);
    this->contentRoot->setWidthPercentage(100.0f);
    this->contentRoot->setHeightPercentage(100.0f);
    this->contentRoot->setAlignItems(AlignItems::STRETCH);

    this->scroller = new ScrollingFrame();
    this->scroller->setWidthPercentage(100.0f);
    this->scroller->setGrow(1.0f);
    this->scroller->setShowScrollingIndicator(true);
    this->contentRoot->addView(this->scroller);

    this->tabBar = new TabBar();
    this->contentRoot->addView(this->tabBar);

    this->shell->pushContentView(this->contentRoot);

    this->pager = new Pager();
    this->pager->setSlot(this->scroller);
    this->pager->setPageFactory("icons", [](PageContext&) -> View* {
        Box* page = buildIconsPage();
        if (page == nullptr)
            return nullptr;

        // 页面外层：左右内边距与页头/底栏同一套主题值
        Box* frame = new Box(Axis::COLUMN);
        frame->setWidthPercentage(100.0f);
        frame->setHeight(AUTO);
        frame->setAlignItems(AlignItems::STRETCH);
        frame->setPadding(getTheme().number("metric.appletFrame.contentPaddingTopBottom", 28.0f));
        frame->setPaddingLeft(getTheme().number("metric.appletFrame.contentPaddingSides", 28.0f));
        frame->setPaddingRight(getTheme().number("metric.appletFrame.contentPaddingSides", 28.0f));
        frame->addView(page);
        return frame;
    });

    this->tabBar->addItem(getStr("tab/icons"), [this] { this->pager->show("icons"); });

    BKLV_LOG_INFO("demo 已就绪：1 个页面（图标总览）");
}

DemoApp::~DemoApp()
{
    // Pager 拥有当前页，必须先于视图树析构
    delete this->pager;
    this->pager = nullptr;
}

void DemoApp::update()
{
    if (this->pageSelected)
        return;

    this->pageSelected = true;
    this->tabBar->setSelectedIndex(0);
    this->pager->show("icons");
}

} // namespace demo

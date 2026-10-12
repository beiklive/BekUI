#include "app/demo_app.hpp"

#include "app/pages.hpp"
#include "app/ui.hpp"
#include "bklv/bklv.hpp"

using namespace bklv;

namespace
{

} // namespace

namespace demo
{

DemoApp::DemoApp()
{
    // ---- 外壳：AppletFrame（页头 + 提示栏底栏）----
    this->root = new AppletFrame();
    this->root->setTitle(getStr("app/title"));
    this->root->setSubtitle(getStr("app/subtitle"));

    // ---- 内容：左 侧栏 + 右 子页面 ----
    Box* body = makeRow(getTheme().number("metric.spacing.xl", 24.0f), AlignItems::STRETCH);
    // 内容列吃满内容区：父（AppletFrame 的 contentBox）高度是确定的，
    // 用 grow 拿剩余高度，再靠交叉轴 stretch 让左右两列都填满。
    // 之前 body 高度是 auto，百分比/百分比高度全都无效 → 整个内容区高度跟着页面内容变
    body->setHeight(AUTO);
    body->setGrow(1.0f);
    body->setWidth(AUTO); // 有左右 margin 时不能写 100%：会多出边距那一段
    body->setAlignItems(AlignItems::STRETCH);

    this->sidebar = new Sidebar(getStr("app/pages"));
    body->addView(this->sidebar);

    Box* right = makeColumn(getTheme().number("metric.spacing.md", 12.0f));
    right->setGrow(1.0f);
    right->setShrink(1.0f);
    right->setHeight(AUTO); // 交叉轴 stretch 会让它填满 body

    // 右上角：日志开关 + 全局页头/底栏显隐（这两样是外壳级的，所有页面共用）
    Box* topRow = makeRow(getTheme().number("metric.spacing.sm", 8.0f), AlignItems::CENTER);
    topRow->setJustifyContent(JustifyContent::FLEX_END);

    Button* headerButton = new Button(getStr("app/hide_header"));
    headerButton->setOnClick([this, headerButton] {
        const bool next = !this->root->isHeaderVisible();
        this->root->showHeader(next);
        headerButton->setText(next ? getStr("app/hide_header") : getStr("app/show_header"));
    });
    topRow->addView(headerButton);

    Button* footerButton = new Button(getStr("app/hide_footer"));
    footerButton->setOnClick([this, footerButton] {
        const bool next = !this->root->isBottomBarVisible();
        this->root->showBottomBar(next);
        footerButton->setText(next ? getStr("app/hide_footer") : getStr("app/show_footer"));
    });
    topRow->addView(footerButton);

    Button* logButton = new Button(getStr("app/show_log"));
    logButton->setOnClick([this, logButton] {
        this->debugWindow.toggle();
        logButton->setText(this->debugWindow.isEnabled() ? getStr("app/hide_log")
                                                        : getStr("app/show_log"));
    });
    topRow->addView(logButton);
    right->addView(topRow);

    this->scroller = new ScrollingFrame();
    this->scroller->setWidthPercentage(100.0f);
    this->scroller->setGrow(1.0f);
    this->scroller->setShowScrollingIndicator(true);

    this->pageSlot = new Box(Axis::COLUMN);
    this->pageSlot->setWidthPercentage(100.0f);
    this->pageSlot->setGrow(1.0f);

    // 滚动器直接装"当前页"：它负责裁剪 + 滚动。
    // 页面比内容区高时会被裁掉而不是画到底栏上（以前 scroller 建了没挂进树，
    // 页面就直接画到栏上，看着像底栏在闪）。
    right->addView(this->scroller);
    body->addView(right);
    // 内容区左右留白：和页头/底栏的 margin 用同一个主题值，四边看起来才是一套
    const float sideMargin = getTheme().number("metric.appletFrame.paddingSides", 30.0f);
    body->setMargins(0.0f, sideMargin, 0.0f, sideMargin);

    this->root->pushContentView(body);

    // ---- 页面（顺序就是侧栏顺序）----
    this->addPage(getStr("tab/theme"), getStr("page/theme_sub"), buildThemePage(), false);
    this->addPage(getStr("tab/widgets"), getStr("page/widgets_sub"), buildWidgetsPage(), false);
    this->addPage(getStr("tab/text"), getStr("page/text_sub"), buildTextPage(), false);
    this->addPage(getStr("tab/layout"), getStr("page/layout_sub"), buildLayoutPage(), false);

    ListPageResult listPage = buildListPage();
    this->addPage(getStr("tab/list"), getStr("page/list_sub"), listPage.view, true);
    this->mainList = listPage.list;

    this->addPage(getStr("tab/json"), getStr("page/json_sub"), buildJsonPage(), false);
    this->addPage(getStr("tab/nav"), getStr("page/nav_sub"), buildNavPage(), false);
    this->addPage(getStr("tab/overlay"), getStr("page/overlay_sub"), buildOverlayPage(), false);

    BKLV_LOG_INFO("界面已就绪：%d 个页面（左侧栏导航）", (int)this->pages.size());
    BKLV_LOG_INFO("图形后端：%s", getGraphicsBackendName());
    BKLV_LOG_INFO("UI 缩放：%.3f（主题尺寸按 1280x720 写）", (double)getUiScale());

    this->selectPage(0);
}

void DemoApp::addPage(const std::string& name, const std::string& subtitle, Box* view,
    bool selfScrolling)
{
    const size_t index = this->pages.size();

    Page page;
    page.name          = name;
    page.subtitle      = subtitle;
    page.view          = view;
    page.selfScrolling = selfScrolling;
    this->pages.push_back(page);

    // 捕获 this + index：DemoApp 活到程序结束，安全；捕获局部下标引用会悬空
    this->sidebar->addItem(name, [this, index] { this->selectPage(index); });
}

void DemoApp::sizeSlotToContent()
{
    const float height = this->scroller->getHeight();
    if (height > 0.0f)
        this->pageSlot->setHeight(height);
}

void DemoApp::selectPage(size_t index)
{
    if (index >= this->pages.size() || index == this->currentPage)
        return;

    this->currentPage = index;
    this->sidebar->setSelectedIndex(index);

    // 页头显示当前页
    this->root->setTitle(this->pages[index].name);
    this->root->setSubtitle(this->pages[index].subtitle);

    View* target = this->pages[index].view;

    // 先把当前页从滚动器上摘下来（不销毁：页面归 this->pages 持有，切回来复用），
    // 再把新页挂上去。setContentView 会自己接管挂载，别再手动 addView（会变成
    // "该视图已有父节点"，并且留下一棵没挂在树上的孤儿）。
    this->scroller->evictContentView();

    for (Page& page : this->pages)
        if (page.view->getParent() == this->pageSlot)
            this->pageSlot->removeView(page.view, false);

    if (this->pages[index].selfScrolling)
    {
        // 页面自己滚动（内部有列表）：给它一个和内容区等高的槽，让列表在槽里滚
        this->sizeSlotToContent();
        this->pageSlot->addView(target);
        this->scroller->setContentView(this->pageSlot);
    }
    else
    {
        this->scroller->setContentView(target);
    }

    this->scroller->setContentOffsetY(0.0f, false);

    // 进子页面后把焦点交给页面里的第一个可聚焦控件（borealis 的 TabFrame 也是这样）
    if (View* first = target->getDefaultFocus())
        getInputManager().giveFocus(first);
}

void DemoApp::update()
{
    if (this->currentPage < this->pages.size() && this->pages[this->currentPage].selfScrolling)
        this->sizeSlotToContent();
}

} // namespace demo

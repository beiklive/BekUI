#include "app/page_host.hpp"

#include "app/page_helpers.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/theme_data.hpp"

using namespace bklv;

namespace demo
{

// ---------------------------------------------------------------------------
// TextButton
// ---------------------------------------------------------------------------

TextButton::TextButton(const std::string& text)
    : Box(Axis::ROW)
{
    this->setHeight(30.0f);
    this->setAlignItems(AlignItems::CENTER);
    this->setJustifyContent(JustifyContent::CENTER);
    this->setCornerRadius(7.0f);
    this->setPadding(0.0f, 14.0f, 0.0f, 14.0f);
    this->setShrink(0.0f);
    this->setBackgroundColor(rgb(52, 58, 70));

    this->label = makeText(text, 14.0f, rgb(215, 222, 232), true);
    this->addView(this->label);

    // 按钮：点一下就执行，不用先聚焦再按回车
    this->onPointerAction = true;
}

void TextButton::onFocusGained()
{
    this->setBackgroundColor(rgb(48, 86, 140));
    this->setBorderColor(rgb(120, 190, 255));
    this->setBorderThickness(2.0f);
}

void TextButton::onFocusLost()
{
    this->setBackgroundColor(rgb(52, 58, 70));
    this->setBorderThickness(0.0f);
}

PageHost::PageHost()
{
    // ---- 外层：AppletFrame（页头 + 提示栏底栏）----
    this->root = new AppletFrame();
    this->root->setTitle(bklv::getStr("app/title"));
    this->root->setSubtitle(bklv::getStr("app/subtitle"));

    // ---- 内容：左 侧栏 + 右 子页面 ----
    Box* body = new Box(Axis::ROW);
    body->setWidthPercentage(100.0f);
    body->setHeight(AUTO); // 由 AppletFrame 内容区的 stretch 决定高度
    body->setGap(getTheme().number("metric.spacing.lg", 16.0f));

    this->sidebar = new Sidebar(bklv::getStr("app/pages"));
    body->addView(this->sidebar);

    Box* right = new Box(Axis::COLUMN);
    right->setGrow(1.0f);
    right->setShrink(1.0f);
    right->setHeight(AUTO);

    // 内容区：不自管滚动的页面放进 scroller，自管滚动的页面直接进 pageSlot。
    // 两个容器互斥挂载的原因见下面 addPage/selectPage 的注释。
    this->contentHost = new Box(Axis::COLUMN);
    this->contentHost->setWidthPercentage(100.0f);
    this->contentHost->setGrow(1.0f);

    this->scroller = new ScrollingFrame();
    this->scroller->setWidthPercentage(100.0f);
    this->scroller->setGrow(1.0f);
    this->scroller->setShowScrollingIndicator(true);

    this->pageSlot = new Box(Axis::COLUMN);
    this->pageSlot->setWidthPercentage(100.0f);
    this->pageSlot->setGrow(1.0f);

    // 日志窗口开关：留在界面上，不用记快捷键
    TextButton* logButton = new TextButton(bklv::getStr("app/show_log"));
    logButton->registerAction(ControllerButton::A, bklv::getStr("app/show_log"),
        [this, logButton](View*) {
            this->debugWindow.toggle();
            logButton->setText(this->debugWindow.isEnabled() ? bklv::getStr("app/hide_log")
                                                            : bklv::getStr("app/show_log"));
        });
    Box* logRow = new Box(Axis::ROW);
    logRow->setWidthPercentage(100.0f);
    logRow->setJustifyContent(JustifyContent::FLEX_END);
    logRow->setMarginBottom(getTheme().number("metric.spacing.md", 12.0f));
    logRow->addView(logButton);

    right->addView(logRow);
    right->addView(this->contentHost);
    body->addView(right);

    // AppletFrame 的内容区（内容栈第一层）
    this->root->pushContentView(body);

    // ---- 页面（顺序就是侧栏顺序：先把"外观与主题"放前面，它是这套东西的重点）----
    this->addPage(bklv::getStr("tab/theme"), bklv::getStr("page/theme_sub"), buildThemePage(), false);
    this->addPage(bklv::getStr("tab/widgets"), bklv::getStr("page/widgets_sub"), buildWidgetsPage(), false);
    this->addPage(bklv::getStr("tab/text"), bklv::getStr("page/text_sub"), buildTextPage(), false);
    this->addPage(bklv::getStr("tab/layout"), bklv::getStr("page/layout_sub"), buildLayoutPage(), false);

    ListPage listPage = buildListPage();
    this->addPage(bklv::getStr("tab/list"), bklv::getStr("page/list_sub"), listPage.view,
        listPage.selfScrolling);
    this->addPage(bklv::getStr("tab/json"), bklv::getStr("page/json_sub"), buildJsonPage(), false);
    this->addPage(bklv::getStr("tab/nav"), bklv::getStr("page/nav_sub"), buildNavPage(), false);
    this->addPage(bklv::getStr("tab/applet"), bklv::getStr("page/applet_sub"), buildAppletPage(), false);
    this->mainList = listPage.list;

    BKLV_LOG_INFO("界面已就绪：%d 个页面（左侧栏导航）", static_cast<int>(this->pages.size()));
    BKLV_LOG_INFO("图形后端：%s", getGraphicsBackendName());
    BKLV_LOG_DEBUG("这是 DEBUG 级别，默认会被过滤掉");
    BKLV_LOG_WARNING("这是一条 WARNING 示例");
    BKLV_LOG_ERROR("这是一条 ERROR 示例");

    // ---- 初始页 ----
    // contentHost 要先挂进 body，selectPage 里的 removeView/addView 才有意义。
    this->selectPage(0);
}

void PageHost::addPage(const std::string& name, const std::string& subtitle, Box* view,
    bool selfScrolling)
{
    const size_t index = this->pages.size();

    this->pages.push_back(view);
    this->selfScrolling.push_back(selfScrolling);
    this->pageSubtitles.push_back(subtitle);

    // 捕获 this + index。PageHost 的生命周期覆盖整个程序，安全；
    // 用引用捕获局部下标则会悬空。
    this->sidebar->addItem(name, [this, index] { this->selectPage(index); });
}

void PageHost::sizeSlotToContent()
{
    if (this->pageSlot == nullptr)
        return;

    // pageSlot 自己占满内容区（grow=1），但挂进来的自滚动页面用的是
    // height:100%，Yoga 需要父节点有确定高度才能解析出来。
    const float height = this->contentHost->getHeight();
    if (height > 0.0f)
        this->pageSlot->setHeight(height);
}

void PageHost::selectPage(size_t index)
{
    if (index >= this->pages.size() || index == this->currentPage)
        return;

    this->currentPage = index;
    this->sidebar->setSelectedIndex(index);

    // 页头显示当前页的名字与说明
    if (index < this->pageSubtitles.size())
    {
        this->root->setTitle(this->sidebar->getItemTitle(index));
        this->root->setSubtitle(this->pageSubtitles[index]);
    }

    Box* holder  = this->contentHost;
    View* target = this->pages[index];
    const bool selfScroll = this->selfScrolling[index];

    // ---- 把两个容器都从舞台上摘下来，并交出当前页面 ----
    // ScrollingFrame 用 evictContentView：它不拥有内容视图，
    // 直接 setContentView 覆盖会销毁旧页面，而 pages 里还留着指针。
    if (this->scroller->getParent() == holder)
        holder->removeView(this->scroller, false);
    this->scroller->evictContentView();

    if (this->pageSlot->getParent() == holder)
        holder->removeView(this->pageSlot, false);

    // 上一页如果挂在 pageSlot 里，也要摘下来（不能跟着 pageSlot 一起被替换掉）
    for (View* page : this->pages)
    {
        if (page->getParent() == this->pageSlot)
            this->pageSlot->removeView(page, false);
    }

    // ---- 挂上需要的那个容器，并把目标页放进去 ----
    if (selfScroll)
    {
        this->sizeSlotToContent();
        holder->addView(this->pageSlot);
        this->pageSlot->addView(target);
    }
    else
    {
        holder->addView(this->scroller);
        this->scroller->setContentView(target);
        this->scroller->setContentOffsetY(0.0f, false);
    }
}

void PageHost::update()
{

    // 自滚动页的高度要跟着内容区走（窗口尺寸变化时会变）。
    if (this->currentPage < this->selfScrolling.size() && this->selfScrolling[this->currentPage])
        this->sizeSlotToContent();
}

} // namespace demo

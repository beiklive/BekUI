#include "app/page_host.hpp"

#include <cstdio>

#include "app/page_helpers.hpp"
#include "bklv/core/graphics.hpp"

using namespace bklv;

namespace demo
{

PageHost::PageHost()
{
    // ---- 外层：标题 + tab 栏 + 内容区 ----
    this->root = new Box(Axis::COLUMN);
    this->root->setWidthPercentage(100.0f);
    this->root->setHeightPercentage(100.0f);
    this->root->setBackgroundColor(rgb(26, 28, 33));
    this->root->setPadding(20.0f);
    this->root->setGap(14.0f);

    // 标题行
    Box* header = new Box(Axis::ROW);
    header->setWidthPercentage(100.0f);
    header->setAlignItems(AlignItems::CENTER);
    header->setJustifyContent(JustifyContent::SPACE_BETWEEN);

    Label* title = makeText("bklv框架demo", 26.0f, rgb(255, 255, 255));
    title->setSingleLine(true);
    header->addView(title);

    Label* subtitle =
        makeText("左右切换标签 · 上下移动焦点 · 回车触发 · F1 调试层", 13.0f, rgb(150, 160, 175));
    subtitle->setSingleLine(true);
    subtitle->setHorizontalAlign(HorizontalAlign::RIGHT);
    header->addView(subtitle);
    this->root->addView(header);

    // 内容区：不自管滚动的页面放进 scroller，自管滚动的页面直接进 contentHost
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

    // tab 栏
    this->tabBar = new TabBar();
    this->root->addView(this->tabBar);

    // ---- 页面 ----
    this->addPage("控件", buildWidgetsPage(), false);
    this->addPage("文本", buildTextPage(), false);
    this->addPage("布局", buildLayoutPage(), false);

    ListPage listPage = buildListPage();
    this->addPage("列表", listPage.view, listPage.selfScrolling);
    this->mainList = listPage.list;

    DebugPage debugPage = buildDebugPage();
    this->addPage("调试", debugPage.view, false);
    this->statsLabel = debugPage.stats;

    // ---- 初始页 ----
    // contentHost 要先挂进 root，selectPage 里的 removeView/addView 才有意义。
    this->root->addView(this->contentHost);
    this->selectPage(0);
}

void PageHost::addPage(const std::string& name, Box* view, bool selfScrolling)
{
    const size_t index = this->pages.size();

    this->pages.push_back(view);
    this->selfScrolling.push_back(selfScrolling);

    // 捕获 this + index。PageHost 的生命周期覆盖整个程序，安全；
    // 用引用捕获局部下标则会悬空。
    this->tabBar->addTab(name, [this, index] { this->selectPage(index); },
        [this, index] { this->selectPage(index); });
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
    this->tabBar->setSelectedIndex(index);

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
    if (this->statsLabel == nullptr)
        return;

    // 统计文字不需要每帧刷新，0.25 秒一次就够，省得每帧重排文本
    this->statsTimer += ImGui::GetIO().DeltaTime;
    if (this->statsTimer < 0.25f)
        return;
    this->statsTimer = 0.0f;

    // 窗口尺寸变化后内容区高度会变，自滚动页要跟着重新定尺寸
    if (this->selfScrolling[this->currentPage])
    {
        this->sizeSlotToContent();
    }

    char text[512];
    std::snprintf(text, sizeof(text),
        "图形后端：%s\n"
        "存活纹理：%d\n"
        "帧率：%.0f FPS\n"
        "主列表当前显示的 cell：%zu（固定 1000 条数据）\n"
        "主列表累计创建的 cell：%zu",
        getGraphicsBackendName(), getLiveTextureCount(), ImGui::GetIO().Framerate,
        this->mainList != nullptr ? this->mainList->getVisibleCellCount() : 0,
        this->mainList != nullptr ? this->mainList->getCreatedCellCount() : 0);

    this->statsLabel->setText(text);
}

} // namespace demo

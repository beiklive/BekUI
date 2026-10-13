#include "bklv/widgets/pager.hpp"

#include "bklv/core/i18n.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/scrolling_frame.hpp"

namespace bklv
{

namespace
{

// 内置占位：一行文案（loading / 加载失败）。上层可以注册自己的占位工厂替换它。
View* makePlaceholderView(const std::string& text)
{
    Box* box = new Box(Axis::COLUMN);
    box->setWidthPercentage(100.0f);
    box->setHeight(AUTO);
    box->setPadding(getTheme().number("metric.spacing.lg", 16.0f));
    box->setAlignItems(AlignItems::CENTER);
    box->setJustifyContent(JustifyContent::CENTER);

    Label* label = new Label();
    label->setText(text);
    label->setFontSize(getTheme().number("metric.font.size.md", 18.0f));
    label->setTextColor(getTheme().color("color.common.textMuted"));
    label->setSingleLine(false);
    box->addView(label);

    return box;
}

} // namespace

Pager::~Pager()
{
    // 当前页归 Pager 所有。槽还在就先让它摘（ScrollingFrame 只摘不删），再自己删，
    // 这样不会和槽的析构重复释放。
    if (this->currentView != nullptr)
    {
        if (this->slot != nullptr && this->currentView->getParent() == this->slot)
            this->slot->evictContentView();

        delete this->currentView;
        this->currentView = nullptr;
    }
}

void Pager::setPageFactory(const std::string& id, PageFactory factory)
{
    this->factories[id] = std::move(factory);
}

void Pager::setPreloader(const std::string& id, PagePreloader preloader)
{
    this->preloaders[id] = std::move(preloader);
}

// ---------------------------------------------------------------------------
// 切页
// ---------------------------------------------------------------------------

View* Pager::buildPage(const std::string& id, PageBuildResult& resultOut)
{
    PageContext ctx;
    ctx.userData = this->userData;
    ctx.result   = PageBuildResult::OK;

    auto it = this->factories.find(id);
    if (it == this->factories.end() || !it->second)
    {
        // 没注册工厂属于配置错误：按失败处理，别让它表现成"一直在加载"
        BKLV_LOG_WARNING("Pager::show: 页面 %s 没有注册工厂", id.c_str());
        resultOut = PageBuildResult::FAILED;
        return nullptr;
    }

    View* page = it->second(ctx);
    resultOut  = ctx.result;
    return page;
}

View* Pager::makePlaceholder(const std::string& id, PageBuildResult result)
{
    if (result == PageBuildResult::FAILED)
    {
        if (this->errorViewFactory)
            return this->errorViewFactory(id);
        return makePlaceholderView(getStr("pager/load_failed"));
    }

    if (this->loadingViewFactory)
        return this->loadingViewFactory();
    return makePlaceholderView(getStr("pager/loading"));
}

void Pager::show(const std::string& id)
{
    if (this->isShowing(id) && this->currentView != nullptr)
        return;

    // 快速连按：切页会把"别的页正在飞的预加载"取消掉（它再来回调必须被丢弃）
    if (!this->pendingPreloadId.empty() && this->pendingPreloadId != id)
        this->cancelPreload(this->pendingPreloadId);

    // 离开前记下当前页的焦点与滚动位置
    this->rememberPageState();

    PageBuildResult result = PageBuildResult::OK;
    View* page             = this->buildPage(id, result);
    this->placeholderShown = (page == nullptr);

    if (page == nullptr)
        page = this->makePlaceholder(id, result);

    View* outgoing = this->currentView;

    // ---- 顺序不能反（附录 D1）----
    // ① 先挂新页（setContentView 只摘旧页、不销毁）
    if (this->slot != nullptr)
        this->slot->setContentView(page);
    else
        BKLV_LOG_WARNING("Pager::show: 没有槽（setSlot），页面 %s 挂不上去", id.c_str());

    this->currentView   = page;
    this->currentPageId = id;

    // ② 再给新页焦点（没有默认可聚焦子节点时保持当前焦点不动 —— 比如 loading 占位）
    if (View* first = page->getDefaultFocus())
        getInputManager().giveFocus(first);

    // ③ 最后才删旧页：此时旧页已经脱离槽，焦点也不在它身上了
    if (outgoing != nullptr && outgoing != page)
        delete outgoing;

    // 回到这一页：恢复上次的焦点（路径）与滚动位置
    this->loadPageState(id);

    if (this->onPageChanged)
        this->onPageChanged(id);
}

// ---------------------------------------------------------------------------
// 预加载：只拉数据、不建视图、可取消
// ---------------------------------------------------------------------------

void Pager::preload(const std::string& id)
{
    auto it = this->preloaders.find(id);
    if (it == this->preloaders.end() || !it->second)
        return;

    // 同一时刻只留一个 pending：新的一次预加载会顶掉旧的（旧的回来时会被丢弃）
    this->pendingPreloadId = id;

    // 用成员而不是栈上的临时对象：preloader 里如果走到异步，别让它拿到一个已析构的引用。
    this->preloadContext.userData = this->userData;
    this->preloadContext.result   = PageBuildResult::OK;
    it->second(this->preloadContext);
}

void Pager::cancelPreload(const std::string& id)
{
    if (!this->isPreloadPending(id))
        return;

    this->pendingPreloadId.clear();
}

bool Pager::applyPreload(const std::string& id)
{
    // 已经被取消 / 被别的页抢走 → 丢弃。调用方（异步回调）靠这个返回值决定
    // 要不要继续改 UI —— 切页会 delete 旧页，继续改就是 use-after-free。
    if (!this->isPreloadPending(id))
        return false;

    this->pendingPreloadId.clear();
    this->preloadedPages[id] = true;
    return true;
}

// ---------------------------------------------------------------------------
// 页面级记忆
// ---------------------------------------------------------------------------

void Pager::rememberPageState()
{
    if (this->currentPageId.empty())
        return;

    getInputManager().rememberFor(this->currentPageId);

    if (this->slot != nullptr)
        this->pageScrollOffsets[this->currentPageId] = this->slot->getContentOffsetY();
}

bool Pager::loadPageState(const std::string& id)
{
    const bool restoredFocus = getInputManager().loadFocusFor(id);

    auto it = this->pageScrollOffsets.find(id);
    if (it == this->pageScrollOffsets.end() || this->slot == nullptr)
    {
        // 这一页没记过：把上一页残留的 pending 清掉，别把它的偏移落到这一页上
        this->pendingScrollOffset = -1.0f;
        return restoredFocus;
    }

    // 布局是延迟的：刚 setContentView 的内容高度还是 0，getScrollRange() 为 0，
    // 当场 setContentOffsetY 会被钳成 0。所以"有范围"时立刻落位，否则挂成 pending
    // 交给 update()（它由宿主每帧调用）。
    this->pendingScrollOffset = it->second;
    this->update();

    return restoredFocus;
}

void Pager::update()
{
    if (this->pendingScrollOffset < 0.0f || this->slot == nullptr)
        return;

    // 布局还没算完（滚动范围是 0）就等下一帧
    if (this->slot->getScrollRange() <= 0.0f)
        return;

    this->slot->setContentOffsetY(this->pendingScrollOffset, false);
    this->pendingScrollOffset = -1.0f;
}

} // namespace bklv

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

    // 重入保护：show() 会交接焦点，而焦点变化可能触发容器回调（例如侧栏的
    // "聚焦即切页" onFocusGained → selectPage → 又一次 show）。重入会让外层 show
    // 继续往下跑、把内层刚建好的页面再删掉 —— 这里直接忽略内层那次（带日志）。
    if (this->showing)
    {
        BKLV_LOG_WARNING("Pager::show(%s) 重入被忽略（当前正在切到 %s）", id.c_str(),
            this->currentPageId.c_str());
        return;
    }
    this->showing = true;

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

    // 焦点原本在不在这一页里？（要在 setContentView 之前算：挂上新页后旧页就脱离树了）
    // 在 → 切页时把焦点交接给新页；不在（比如在侧栏上）→ 别动焦点，
    // 否则"聚焦即切页"的侧栏会被新页抢走焦点，上下移动就失效了。
    // 焦点为空时也**不**交接：那是"还没人给过焦点"（App::setRootView 之前），
    // 交给上层按自己的默认焦点规则决定（否则会把焦点从侧栏抢进页面）。
    View* focus         = getInputManager().getCurrentFocus();
    bool focusWasInPage = false;
    for (View* v = focus; v != nullptr; v = v->getParent())
    {
        if (v == outgoing)
        {
            focusWasInPage = true;
            break;
        }
    }

    // ---- 顺序不能反（附录 D1）----
    // ① 先挂新页（setContentView 只摘旧页、不销毁）
    if (this->slot != nullptr)
        this->slot->setContentView(page);
    else
        BKLV_LOG_WARNING("Pager::show: 没有槽（setSlot），页面 %s 挂不上去", id.c_str());

    this->currentView   = page;
    this->currentPageId = id;

    // ② 再给新页焦点。注意：交接失败时**必须**把焦点从旧页上挪开 ——
    // 下面就要 delete 旧页了，焦点还指着里面某个控件就是悬空指针。
    if (focusWasInPage)
    {
        if (View* first = page->getDefaultFocus())
            getInputManager().giveFocus(first);
        else
            getInputManager().giveFocus(nullptr);
    }

    // ③ 最后才删旧页：此时旧页已经脱离槽，焦点也不在它身上了。
    //    删之前先把正在跑的"上一次切页转场"收掉（它记着旧页的指针，不能再碰）。
    const bool wasTransitioning = this->isPageTransitioning();
    this->pageAnim.stop();
    this->transitionProgress = 1.0f;
    this->transitionPage     = nullptr;

    if (outgoing != nullptr && outgoing != page)
        delete outgoing;

    // ④ 新页滑入/淡入。两种情况不做转场：
    //    · 首屏（没有旧页可让位）
    //    · **上一次转场还没播完**（快速连按切页）：这时再起一次会让每一页都停在
    //      "刚起步"的位置（内容大半在屏幕外），越快按越看不到内容 —— 直接落位更实用。
    if (outgoing != nullptr && !wasTransitioning)
        this->beginPageTransition(page);

    // 回到这一页：恢复上次的焦点（路径）与滚动位置
    this->loadPageStateInternal(id, focusWasInPage);

    if (this->onPageChanged)
        this->onPageChanged(id);

    this->showing = false;
}

// ---------------------------------------------------------------------------
// 同级换页的转场（只画，不参与布局）
// ---------------------------------------------------------------------------

void Pager::beginPageTransition(View* page)
{
    if (page == nullptr || this->pageTransition == ContentTransition::NONE
        || this->pageTransitionDuration <= 0.0f)
    {
        this->pageAnim.stop();
        this->transitionProgress = 1.0f;
        this->transitionPage     = nullptr;

        if (page != nullptr)
        {
            page->resetTransform();
            page->setAlpha(1.0f);
        }
        return;
    }

    this->transitionWidth    = this->slot != nullptr ? this->slot->getWidth() : 0.0f;
    this->transitionPage     = page;
    this->transitionProgress = 0.0f;

    this->pageAnim.stop();
    this->pageAnim.reset(0.0f);
    this->pageAnim.addStep(1.0f, this->pageTransitionDuration, EasingFunction::CUBIC_OUT);
    this->pageAnim.start();

    this->applyPageTransition(0.0f);
}

void Pager::applyPageTransition(float progress)
{
    if (this->transitionPage == nullptr)
        return;

    const bool slide = this->pageTransition == ContentTransition::SLIDE
        || this->pageTransition == ContentTransition::SLIDE_FADE;
    const bool fade = this->pageTransition == ContentTransition::FADE
        || this->pageTransition == ContentTransition::SLIDE_FADE;

    // 新页从右边滑进来（旧页已经不在了，这块区域先是背景色，再被新页盖住）
    this->transitionPage->setTranslateX(slide ? (1.0f - progress) * this->transitionWidth : 0.0f);
    this->transitionPage->setAlpha(fade ? progress : 1.0f);
}

void Pager::stepPageTransition(float deltaSeconds)
{
    if (this->transitionProgress >= 1.0f || this->transitionPage == nullptr)
        return;

    this->pageAnim.update(deltaSeconds);

    const float progress     = (float)this->pageAnim;
    this->transitionProgress = progress;
    this->applyPageTransition(progress);

    if (progress >= 1.0f)
    {
        // 落位：清掉绘制期变换，回到正常状态
        this->transitionPage->resetTransform();
        this->transitionPage->setAlpha(1.0f);
        this->transitionPage = nullptr;
    }
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

// 焦点现在是不是在"当前页"里？
bool Pager::isFocusInsidePage() const
{
    View* focus = getInputManager().getCurrentFocus();
    if (focus == nullptr || this->currentView == nullptr)
        return false;

    for (View* v = focus; v != nullptr; v = v->getParent())
        if (v == this->currentView)
            return true;

    return false;
}

void Pager::rememberPageState()
{
    if (this->currentPageId.empty())
        return;

    // **只有焦点确实在这一页里才记焦点**。
    // 焦点在页面之外（侧栏 / 页头这类）时记下来，还原时会把焦点抢回侧栏；
    // 而侧栏是"聚焦即切页"（TabCell::onFocusGained → 宿主 selectPage → show），
    // 于是形成 show → loadPageState → giveFocus(侧栏) → selectPage → show 的无限递归
    // —— 实测踩过（demo 14 页里有 5 页栈溢出崩在这里）。
    if (this->isFocusInsidePage())
        getInputManager().rememberFor(this->currentPageId);

    if (this->slot != nullptr)
        this->pageScrollOffsets[this->currentPageId] = this->slot->getContentOffsetY();
}

bool Pager::loadPageState(const std::string& id)
{
    return this->loadPageStateInternal(id, this->isFocusInsidePage());
}

bool Pager::loadPageStateInternal(const std::string& id, bool restoreFocus)
{
    // 焦点只在"焦点本来就在这一页里"时还原：否则会把焦点从侧栏抢走（同 rememberPageState 的坑）
    bool restoredFocus = false;
    if (restoreFocus)
        restoredFocus = getInputManager().loadFocusFor(id);

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
    this->applyPendingScrollOffset();

    return restoredFocus;
}

void Pager::update()
{
    // ① 同级换页的滑入/淡入（绘制期变换，零重排）
    this->stepPageTransition(getFrameDeltaSeconds());

    // ② 页面记忆的滚动位置：布局是延迟的，等算完（滚动范围 > 0）才落得下去
    this->applyPendingScrollOffset();
}

// 只做"把记忆的滚动位置落到槽上"。单独抽出来是因为 loadPageStateInternal 也要用它，
// 而**不能**在那里调整条 update()：那会顺手把刚起步的换页转场推进一帧。
void Pager::applyPendingScrollOffset()
{
    if (this->pendingScrollOffset < 0.0f || this->slot == nullptr)
        return;

    if (this->slot->getScrollRange() <= 0.0f)
        return; // 还没算完，等下一帧

    this->slot->setContentOffsetY(this->pendingScrollOffset, false);
    this->pendingScrollOffset = -1.0f;
}

} // namespace bklv

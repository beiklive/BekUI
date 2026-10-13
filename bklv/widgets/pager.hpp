#pragma once

// Pager：内容层的**页面栈 / 同层换页**控制器，对应 borealis 的 Activity 里
// "切 tab / 切页面" 那一层（方案 docs/shell-implementation-plan.md 附录 D）。
//
// 它不是 View，而是一个控制器：真正的视图树里只有一个滚动槽（`ScrollingFrame`），
// Pager 负责"建新页 → 挂上槽 → 给焦点 → 删旧页"。
//
// 为什么工厂式而不是继承：页面是**数据 + 一次性搭建**，不需要虚函数表；
// 工厂能直接复用 demo 里已有的 `buildXxxPage()`。
//
// 关键行为（都来自附录 D 的场景推演）：
//   1. 切页顺序**不能反**：`slot->setContentView(新页)` → 给新页默认焦点 → 才 delete 旧页。
//      反过来会让焦点短暂落在已销毁的旧页上（use-after-free）。
//   2. 快速连按：`preload` 是**可取消**的。取消之后到达的回调调用 `applyPreload()` 会被丢弃，
//      不许再改 UI（`task::run*` 那条"必须带 AliveFlag 护栏"的约定在这里的统一落点）。
//   3. 失败与 loading：工厂返回 nullptr（或把 `PageContext::result` 设成 LOADING/FAILED）时
//      显示统一的占位视图，不许白屏。
//   4. 页面级记忆：焦点走 `InputManager::rememberFor/loadFocusFor`（存路径，不存裸指针），
//      滚动位置存在 Pager 自己这里（`ScrollingFrame::getContentOffsetY`）。
//
// 生命周期：**Pager 拥有"当前页"**（切页会 delete 旧页），槽只接管挂载、不接管所有权
// （`ScrollingFrame::setContentView` 只摘不删）。所以 Pager 必须先于那个
// `ScrollingFrame` 所在的视图树析构，否则当前页会随槽一起被删。

#include <functional>
#include <map>
#include <string>

#include "bklv/core/time.hpp"
#include "bklv/core/view.hpp"
#include "bklv/widgets/shell.hpp" // ContentTransition（页面/内容栈转场共用一套枚举）

namespace bklv
{

class ScrollingFrame;

// 页面构建结果：工厂里发现"数据还没到 / 取失败"时把它设上，
// Pager 据此决定显示 loading 还是错误占位（不靠返回值猜）。
enum class PageBuildResult
{
    OK,
    LOADING,
    FAILED,
};

// 页面按需从这里取数据（不要直接摸全局）。具体成员由上层填。
struct PageContext
{
    void* userData             = nullptr;
    PageBuildResult result     = PageBuildResult::OK;
};

using PageFactory   = std::function<View*(PageContext&)>;
using PagePreloader = std::function<void(PageContext&)>;

class Pager
{
  public:
    Pager() = default;
    ~Pager();

    // ---- 槽 ----
    void setSlot(ScrollingFrame* slot) { this->slot = slot; }
    ScrollingFrame* getSlot() const { return this->slot; }

    // ---- 注册 ----
    void setPageFactory(const std::string& id, PageFactory factory);
    // 预加载只拉**数据**（不建视图）；异步完成后必须调 applyPreload(id)。
    void setPreloader(const std::string& id, PagePreloader preloader);
    void setUserData(void* data) { this->userData = data; }
    void setOnPageChanged(std::function<void(const std::string&)> callback)
    {
        this->onPageChanged = std::move(callback);
    }

    // 占位视图工厂（可选）。不设就用内置的最小占位（一行文案）。
    void setLoadingViewFactory(std::function<View*()> factory)
    {
        this->loadingViewFactory = std::move(factory);
    }
    void setErrorViewFactory(std::function<View*(const std::string&)> factory)
    {
        this->errorViewFactory = std::move(factory);
    }

    // ---- 切页 ----
    //
    // 建页 → 挂槽 → 给焦点 → 删旧页 → 恢复这一页的记忆。
    // 返回 void：`show` 不在 check_naming.py 的动作前缀白名单里（与 v10 的 `restore` 同类）。
    void show(const std::string& id);

    View* getCurrentView() const { return this->currentView; }

    // ---- 同级换页的转场（方案 5.5：转场归 Pager，用**绘制期变换**，零重排）----
    //
    // 只做"新页滑入/淡入"：旧页在切页时就删了（附录 D1 的删除顺序不能反），
    // 所以没有旧页滑出的那一段 —— 要双向滑动得在过渡期把两页都留在树上，
    // 属于后续增量（见方案 v22 的已知遗留）。
    void setPageTransition(ContentTransition value) { this->pageTransition = value; }
    ContentTransition getPageTransition() const { return this->pageTransition; }
    void setPageTransitionDuration(float seconds) { this->pageTransitionDuration = seconds; }
    float getPageTransitionDuration() const { return this->pageTransitionDuration; }
    bool isPageTransitioning() const { return this->transitionProgress < 1.0f; }
    // 确定性步进（update() 内部按帧间隔调；测试直接喂固定 dt）
    void stepPageTransition(float deltaSeconds);
    const std::string& getCurrentPageId() const { return this->currentPageId; }
    bool isShowing(const std::string& id) const
    {
        return !this->currentPageId.empty() && this->currentPageId == id;
    }
    // 当前显示的是占位（loading）而不是真页面
    bool isPlaceholderShown() const { return this->placeholderShown; }
    bool hasPageFactory(const std::string& id) const { return this->factories.count(id) > 0; }

    // ---- 预加载（可取消）----
    void preload(const std::string& id);
    void cancelPreload(const std::string& id);
    // 异步数据到齐后由 preloader 调。只有仍在 pending 的 id 才被接受（返回 true），
    // 被取消/已被切页抢走的返回 false —— 调用方据它决定要不要继续改 UI。
    bool applyPreload(const std::string& id);
    bool isPreloadPending(const std::string& id) const
    {
        return !this->pendingPreloadId.empty() && this->pendingPreloadId == id;
    }
    bool isPagePreloaded(const std::string& id) const
    {
        return this->preloadedPages.count(id) > 0;
    }

    // ---- 页面级记忆（焦点 + 滚动位置）----
    // 离开这一页之前调（show() 内部会自动调一次）。
    void rememberPageState();
    // 回到这一页时调（show() 内部在给完默认焦点后自动调一次）。
    bool loadPageState(const std::string& id);

    // 每帧由宿主调一次（demo 走 App::onUpdate）。
    //
    // 目前只做一件事：把"记忆的滚动位置"延后到布局算完之后落位。
    // 布局是**延迟**的（view.cpp 的 invalidate + App::frame），show() 里刚 setContentView
    // 的内容高度还是 0 → `getScrollRange()` 为 0 → 当场 setContentOffsetY 会被钳成 0。
    void update();

  private:
    View* buildPage(const std::string& id, PageBuildResult& resultOut);
    // 新页的滑入/淡入（不负责旧页：旧页此刻已经被 delete）
    void beginPageTransition(View* page);
    // 把记忆的滚动位置落到槽上（update() 与 loadPageStateInternal 共用）
    void applyPendingScrollOffset();
    void applyPageTransition(float progress);

    View* makePlaceholder(const std::string& id, PageBuildResult result);
    // 焦点是否在"当前页"里（页面级记忆的还原条件，见 .cpp 里的无限递归注释）
    bool isFocusInsidePage() const;
    bool loadPageStateInternal(const std::string& id, bool restoreFocus);

    ScrollingFrame* slot = nullptr;

    std::map<std::string, PageFactory> factories;
    std::map<std::string, PagePreloader> preloaders;
    std::map<std::string, float> pageScrollOffsets;

    // "这一页的预加载还在飞"：同一时刻只允许一个（快速连按切页会抢走它）
    std::string pendingPreloadId;
    std::map<std::string, bool> preloadedPages;
    // 预加载用的上下文（成员而不是栈上临时对象：preloader 里可能走异步）
    PageContext preloadContext;

    View* currentView = nullptr;
    std::string currentPageId;
    bool placeholderShown = false;
    // show() 重入保护（焦点交接可能触发容器回调 → 又一次 show）
    bool showing = false;

    // 同级换页转场
    ContentTransition pageTransition = ContentTransition::SLIDE_FADE;
    float pageTransitionDuration     = 0.22f; // 秒
    Animatable pageAnim{ 1.0f };
    View* transitionPage             = nullptr;
    float transitionProgress         = 1.0f;  // 1 = 没有转场
    float transitionWidth            = 0.0f;
    // 待落位的滚动位置（< 0 表示没有）。见 update() 的注释。
    float pendingScrollOffset = -1.0f;

    void* userData = nullptr;
    std::function<void(const std::string&)> onPageChanged;
    std::function<View*()> loadingViewFactory;
    std::function<View*(const std::string&)> errorViewFactory;
};

} // namespace bklv

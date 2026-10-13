// 无窗口测试：第 5 步 · Pager（方案 docs/shell-implementation-plan.md 附录 D / D3）。
//
// 覆盖：
//   1. 切页只活一个页面视图（构造/析构计数）+ 重复 show 同一页不重建
//   2. 预加载只拉数据、不建视图；取消/被切页抢走后到达的回调一律丢弃
//   3. 失败与 loading 路径：工厂返回 nullptr / 未注册工厂都不崩，且显示占位
//   4. 页面级记忆：焦点（路径）与滚动位置离开再回来能还原
//
// 布局手工驱动（YGNodeCalculateLayout → 读尺寸），纯计算，不需要窗口。

#include <cstdio>
#include <vector>

#include <yoga/Yoga.h>

#include "bklv/core/appearance.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/view.hpp"
#include "bklv/views/scrolling_frame.hpp"
#include "bklv/widgets/pager.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 页面构造 / 析构计数：用来断言"切页只活一个页面"
int g_livePages = 0;
int g_createdPages = 0;

class FocusBox : public Box
{
  public:
    explicit FocusBox(Axis axis = Axis::COLUMN)
        : Box(axis)
    {
    }

    bool isFocusable() const override { return true; }

    std::string getClassString() const override { return "FocusBox"; }
};

// 页面根：构造/析构都计数，用来验证"切页只活一个页面"
class CountingBox : public Box
{
  public:
    explicit CountingBox(Axis axis = Axis::COLUMN)
        : Box(axis)
    {
        g_livePages++;
        g_createdPages++;
    }

    ~CountingBox() override { g_livePages--; }

    std::string getClassString() const override { return "CountingBox"; }
};

// 一个页面：高 1000（保证滚动范围 > 0），里面一个可聚焦控件
Box* makePage()
{
    Box* page = new CountingBox();
    page->setWidthPercentage(100.0f);
    page->setHeight(1000.0f);

    FocusBox* focus = new FocusBox();
    focus->setDimensions(200.0f, 40.0f);
    page->addView(focus);

    page->addView(new FocusBox()); // 第二个：让"恢复的是哪一个"有意义

    return page;
}

void layoutTree(View* root, float width, float height)
{
    YGNodeCalculateLayout(root->getYGNode(), width, height, YGDirectionLTR);
    root->notifyLayout();
}

void resetInput()
{
    InputManager& input = getInputManager();
    input.resetFocusState();
    input.setRoot(nullptr);
}

} // namespace

int main()
{
    bklv::initResources();
    setTheme("default");

    InputManager& input = getInputManager();

    std::printf("\n== 1. 切页只活一个页面视图 ==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 300.0f);
        ScrollingFrame* slot = new ScrollingFrame();
        slot->setDimensions(400.0f, 200.0f);
        root->addView(slot);
        layoutTree(root, 400.0f, 300.0f);
        input.setRoot(root);

        Pager* pager = new Pager();
        pager->setSlot(slot);
        pager->setPageFactory("a", [](PageContext&) -> View* { return makePage(); });
        pager->setPageFactory("b", [](PageContext&) -> View* { return makePage(); });

        g_livePages    = 0;
        g_createdPages = 0;

        pager->show("a");
        check(g_livePages == 1, "show 之后活 1 个页面");
        check(g_createdPages == 1, "show 只建一次页面");
        check(pager->isShowing("a"), "isShowing(\"a\") 为真");
        check(!pager->isShowing("b"), "isShowing(\"b\") 为假");
        check(pager->getCurrentView() != nullptr, "有当前页");
        check(slot->getContentView() == pager->getCurrentView(), "新页挂在槽里");
        check(pager->getCurrentView()->getParent() != nullptr, "新页确实进了视图树");

        View* pageA = pager->getCurrentView();
        pager->show("b");
        check(pager->isExiting(), "切页先让旧页出场（两段式转场的第一段）");
        check(pager->getCurrentView() != pageA, "逻辑上的当前页立刻换成新页");
        check(pager->isShowing("b"), "isShowing(\"b\") 为真");
        pager->finishPageTransition();
        check(g_livePages == 1, "转场落位后只活 1 个页面（旧页已 delete）");
        check(g_createdPages == 2, "切页建了第 2 个页面");
        check(slot->getContentView() == pager->getCurrentView(), "槽里是新页");

        // 重复 show 同一页：不重建、不重挂
        const int createdBefore = g_createdPages;
        View* currentBefore     = pager->getCurrentView();
        pager->show("b");
        check(g_createdPages == createdBefore, "重复 show 同一页不重建");
        check(pager->getCurrentView() == currentBefore, "重复 show 当前页不变");
        check(!pager->isPageTransitioning(), "重复 show 也不起转场");
        check(g_livePages == 1, "重复 show 之后还是只活 1 个页面");

        // 来回切：每次只建新页、删旧页
        pager->show("a");
        pager->finishPageTransition();
        check(g_livePages == 1, "来回切页落位后仍然只有 1 个页面活着");
        check(g_createdPages == 3, "每次切页都重新建页面（页面不缓存）");

        delete pager; // Pager 必须先于视图树析构（它拥有当前页）
        check(g_livePages == 0, "Pager 析构释放当前页");

        input.setRoot(nullptr);
        resetInput();
        delete root;
    }

    std::printf("\n== 2. 预加载：只拉数据、可取消 ==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 300.0f);
        ScrollingFrame* slot = new ScrollingFrame();
        slot->setDimensions(400.0f, 200.0f);
        root->addView(slot);
        layoutTree(root, 400.0f, 300.0f);
        input.setRoot(root);

        Pager* pager = new Pager();
        pager->setSlot(slot);
        pager->setPageFactory("a", [](PageContext&) -> View* { return makePage(); });
        pager->setPageFactory("b", [](PageContext&) -> View* { return makePage(); });
        pager->setPageFactory("c", [](PageContext&) -> View* { return makePage(); });

        int preloadCalls = 0;
        pager->setPreloader("a", [&preloadCalls](PageContext&) { preloadCalls++; });
        pager->setPreloader("b", [&preloadCalls](PageContext&) { preloadCalls++; });
        pager->setPreloader("c", [&preloadCalls](PageContext&) { preloadCalls++; });

        g_createdPages = 0;

        pager->preload("a");
        check(preloadCalls == 1, "预加载回调被调用");
        check(g_createdPages == 0, "预加载不建视图");
        check(pager->isPreloadPending("a"), "预加载处于 pending");
        check(pager->applyPreload("a"), "数据到齐：接受这次预加载");
        check(pager->isPagePreloaded("a"), "标记为已预加载");
        check(!pager->isPreloadPending("a"), "接受后不再 pending");

        // 取消之后到达的回调必须被丢弃
        pager->preload("b");
        pager->cancelPreload("b");
        check(!pager->isPreloadPending("b"), "取消后不再 pending");
        check(!pager->applyPreload("b"), "取消后到达的回调被丢弃");
        check(!pager->isPagePreloaded("b"), "被丢弃的预加载不标记成已就绪");

        // 切页会抢走别的页正在飞的预加载
        pager->preload("c");
        pager->show("a");
        check(!pager->isPreloadPending("c"), "切页抢走别的页的 pending 预加载");
        check(!pager->applyPreload("c"), "被抢走的预加载回调被丢弃");
        check(!pager->isPagePreloaded("c"), "被抢走的预加载不标记成已就绪");

        // 没注册预加载器的页：什么都不做，不崩
        pager->setPageFactory("d", [](PageContext&) -> View* { return makePage(); });
        pager->preload("d");
        check(!pager->isPreloadPending("d"), "没注册预加载器时不会 pending");

        delete pager;
        input.setRoot(nullptr);
        resetInput();
        delete root;
    }

    std::printf("\n== 3. 失败与 loading 占位 ==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 300.0f);
        ScrollingFrame* slot = new ScrollingFrame();
        slot->setDimensions(400.0f, 200.0f);
        root->addView(slot);
        layoutTree(root, 400.0f, 300.0f);
        input.setRoot(root);

        Pager* pager = new Pager();
        pager->setSlot(slot);

        int loadingFactoryCalls = 0;
        int errorFactoryCalls   = 0;
        pager->setLoadingViewFactory([&loadingFactoryCalls] {
            loadingFactoryCalls++;
            return makePage();
        });
        pager->setErrorViewFactory([&errorFactoryCalls](const std::string&) {
            errorFactoryCalls++;
            return makePage();
        });

        // 数据未就绪 → loading 占位
        pager->setPageFactory("slow", [](PageContext& ctx) -> View* {
            ctx.result = PageBuildResult::LOADING;
            return nullptr;
        });
        pager->show("slow");
        check(loadingFactoryCalls == 1, "数据未就绪 → 用 loading 占位视图");
        check(errorFactoryCalls == 0, "数据未就绪不算失败");
        check(pager->isPlaceholderShown(), "当前显示的是占位而不是真页面");
        check(pager->getCurrentView() != nullptr, "占位也有视图（不白屏）");
        check(slot->getContentView() == pager->getCurrentView(), "占位也挂在槽里");

        // 构建失败 → 错误占位
        pager->setPageFactory("bad", [](PageContext& ctx) -> View* {
            ctx.result = PageBuildResult::FAILED;
            return nullptr;
        });
        pager->show("bad");
        check(errorFactoryCalls == 1, "失败 → 用错误占位视图");
        check(pager->isShowing("bad"), "失败页也是「当前页」（状态明确）");

        // 没注册工厂 → 按失败处理，不崩
        pager->show("missing");
        check(errorFactoryCalls == 2, "未注册工厂走失败路径");
        check(pager->getCurrentView() != nullptr, "未注册工厂也不白屏");

        // 不设占位工厂时的内置兜底（走 getStr("pager/*")）
        Pager* plain = new Pager();
        plain->setSlot(slot);
        plain->show("nope");
        check(plain->getCurrentView() != nullptr, "内置占位兜底：有视图");
        check(plain->isPlaceholderShown(), "内置占位兜底：标记为占位");
        delete plain;

        delete pager;
        input.setRoot(nullptr);
        resetInput();
        delete root;
    }

    std::printf("\n== 4. 页面级记忆：焦点 + 滚动位置 ==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 300.0f);
        ScrollingFrame* slot = new ScrollingFrame();
        slot->setDimensions(400.0f, 200.0f);
        root->addView(slot);
        layoutTree(root, 400.0f, 300.0f);
        input.setRoot(root);

        Pager* pager = new Pager();
        pager->setSlot(slot);
        pager->setPageFactory("a", [](PageContext&) -> View* { return makePage(); });
        pager->setPageFactory("b", [](PageContext&) -> View* { return makePage(); });

        pager->show("a");
        pager->finishPageTransition();
        layoutTree(root, 400.0f, 300.0f);

        check(slot->getScrollRange() > 0.0f, "页面够高，滚动范围 > 0");

        // 焦点挪到第 2 个控件，并把滚动位置挪到 120
        View* pageA  = pager->getCurrentView();
        View* second = pageA->asBox()->getChildren()[1];
        input.giveFocus(second);
        slot->setContentOffsetY(120.0f, false);
        checkFloat(slot->getContentOffsetY(), 120.0f, "先手动把滚动位置挪到 120");

        pager->show("b");
        pager->finishPageTransition();
        check(pager->isShowing("b"), "切到 b 页");
        layoutTree(root, 400.0f, 300.0f);
        pager->update();
        checkFloat(slot->getContentOffsetY(), 0.0f, "b 页的滚动位置是 0");

        pager->show("a");
        pager->finishPageTransition();
        check(pager->isShowing("a"), "切回 a 页");
        layoutTree(root, 400.0f, 300.0f);
        pager->update(); // 滚动位置要等布局算完才落得下去（见 Pager::update 注释）
        checkFloat(slot->getContentOffsetY(), 120.0f, "回到 a 页：滚动位置还原");

        // 注意：回来的是**新建**的页面（旧页已被 delete），所以要重新取指针
        View* pageA2 = pager->getCurrentView();
        check(input.getCurrentFocus() == pageA2->asBox()->getChildren()[1],
            "回到 a 页：焦点还原到原控件（新页上同一路径的控件）");

        // 没记过的页：滚动位置保持 0，不崩
        pager->show("b");
        pager->finishPageTransition();
        checkFloat(slot->getContentOffsetY(), 0.0f, "b 页没有记忆时偏移为 0");
        check(pager->loadPageState("never-seen") == false, "没记过的页 loadPageState 返回 false");

        delete pager;
        input.setRoot(nullptr);
        resetInput();
        delete root;
    }

    std::printf("\n== 5. 同级换页的转场：旧页出场 → 新页入场 ==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 300.0f);
        ScrollingFrame* slot = new ScrollingFrame();
        slot->setDimensions(400.0f, 200.0f);
        root->addView(slot);
        layoutTree(root, 400.0f, 300.0f);
        input.setRoot(root);

        Pager* pager = new Pager();
        pager->setSlot(slot);
        pager->setPageFactory("a", [](PageContext&) -> View* { return makePage(); });
        pager->setPageFactory("b", [](PageContext&) -> View* { return makePage(); });
        pager->setTransitionDuration(0.2f);

        check(!pager->isPageTransitioning(), "初始没有转场");
        checkEqualInt((int)pager->getEnterTransition(), (int)ContentTransition::SLIDE_FADE,
            "默认入场是 SLIDE_FADE");
        checkEqualInt((int)pager->getExitTransition(), (int)ContentTransition::SLIDE_FADE,
            "默认出场也是 SLIDE_FADE");

        // 首屏：没有旧页可让位 → 不做转场
        pager->show("a");
        check(!pager->isPageTransitioning(), "首屏不产生转场（没有旧页）");
        checkFloat(pager->getCurrentView()->getAlpha(), 1.0f, "首屏不透明");
        layoutTree(root, 400.0f, 300.0f);

        View* oldPage = pager->getCurrentView();

        // ---- 第一段：旧页出场 ----
        pager->show("b");
        View* pageB = pager->getCurrentView();
        check(pager->isExiting(), "切页先让旧页出场");
        check(slot->getContentView() == oldPage, "出场期间槽里还是旧页（所以看得到）");
        check(pageB->getParent() == nullptr, "新页这时还没挂上");
        checkFloat(oldPage->getAlpha(), 1.0f, "出场起点：旧页不透明");

        pager->stepPageTransition(0.1f);
        check(oldPage->getTransform().translateX < 0.0f, "旧页在往左滑");
        check(oldPage->getAlpha() < 1.0f, "旧页在淡出");

        pager->stepPageTransition(0.5f);
        check(!pager->isExiting(), "出场结束");

        // ---- 第二段：换页 + 新页入场 ----
        check(slot->getContentView() == pageB, "出场播完新页才挂上");
        check(pageB->getParent() != nullptr, "新页进了视图树");
        check(pager->isEntering(), "接着新页入场");
        check(pageB->getTransform().translateX > 0.0f, "新页从右侧开始入场");
        check(pageB->getAlpha() < 0.2f, "新页起始接近透明");

        const float startX = pageB->getTransform().translateX;
        pager->stepPageTransition(0.1f);
        check(pageB->getTransform().translateX < startX, "新页在向左走");
        check(pageB->getAlpha() > 0.0f, "新页在变清晰");

        pager->stepPageTransition(0.5f);
        check(!pager->isPageTransitioning(), "两段都播完");
        checkFloat(pageB->getTransform().translateX, 0.0f, "落位：位移归零");
        checkFloat(pageB->getAlpha(), 1.0f, "落位：完全不透明");
        checkEqualSize(g_livePages, 1, "落位后只活 1 个页面");

        // 上一段没播完就再切：先把上一段落位，再起新的一段（不留幽灵页）
        pager->show("a");
        check(pager->isPageTransitioning(), "又起了一段转场");
        pager->show("b");
        pager->finishPageTransition();
        check(pager->isShowing("b"), "连续快速切页最终停在最后请求的那一页");
        checkEqualSize(g_livePages, 1, "快速连按落位后也只活 1 个页面");
        checkFloat(pager->getCurrentView()->getAlpha(), 1.0f, "落位后新页不透明");

        // FADE：只淡不滑（入场/出场分开配也成立）
        pager->setEnterTransition(ContentTransition::FADE);
        pager->setExitTransition(ContentTransition::FADE);
        pager->show("a");
        checkFloat(pager->getCurrentView()->getAlpha(), 1.0f, "出场阶段新页还没入场");
        checkFloat(slot->getContentView()->getTransform().translateX, 0.0f, "FADE 出场不位移");
        pager->stepPageTransition(0.5f);
        check(pager->isEntering(), "出场完接着入场");
        checkFloat(pager->getCurrentView()->getTransform().translateX, 0.0f, "FADE 入场不位移");
        check(pager->getCurrentView()->getAlpha() < 0.2f, "FADE 入场有淡入");

        // NONE：直接落位
        pager->setEnterTransition(ContentTransition::NONE);
        pager->setExitTransition(ContentTransition::NONE);
        pager->finishPageTransition();
        pager->show("b");
        check(!pager->isPageTransitioning(), "NONE 不产生转场");
        checkFloat(pager->getCurrentView()->getAlpha(), 1.0f, "NONE 直接不透明");
        checkFloat(pager->getCurrentView()->getTransform().translateX, 0.0f, "NONE 不位移");
        check(slot->getContentView() == pager->getCurrentView(), "NONE 时槽里直接是新页");
        checkEqualSize(g_livePages, 1, "NONE 时也立刻只活 1 个页面");

        delete pager;
        input.setRoot(nullptr);
        resetInput();
        delete root;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

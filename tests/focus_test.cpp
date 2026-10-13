// 无窗口测试：第 3 步 · 焦点管理（方案 docs/shell-implementation-plan.md 3.2 / 附录 B / 附录 C）。
//
// 覆盖：默认焦点三级、顺序索引导航 + 父链冒泡、跨 box 三档策略、导航循环、
// 撞边反馈（方向 + 节流）、模态遮挡、焦点记忆（跨层 / 同层换页）、ensureFocusVisible、
// 高亮抖动不进布局、焦点变化复用既有 FocusChangedEvent。
//
// 布局手工驱动：YGNodeCalculateLayout → 读 getX / getY / getWidth / getHeight，
// 纯计算，不需要窗口或图形后端（和 tests/widget_test.cpp 的滚动那节同一套路）。

#include <cstdio>
#include <string>
#include <vector>

#include <yoga/Yoga.h>

#include "bklv/core/box.hpp"
#include "bklv/core/event.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/view.hpp"
#include "bklv/views/scrolling_frame.hpp"
#include "bklv/widgets/button.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 最小可聚焦视图：Box 本身不可聚焦（isFocusable 默认 false），
// 这里只把"能不能聚焦"这一个钩子打开，方便造导航结构。
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

FocusBox* makeFocus(float width = 100.0f, float height = 40.0f)
{
    FocusBox* view = new FocusBox();
    view->setDimensions(width, height);
    return view;
}

void layoutTree(View* root, float width, float height)
{
    YGNodeCalculateLayout(root->getYGNode(), width, height, YGDirectionLTR);
    root->notifyLayout();
}

// 每个小节开始时把输入层恢复干净：焦点、模态栈、回调、节流都复位。
void resetInput()
{
    InputManager& input = getInputManager();
    input.resetFocusState();
    input.setRoot(nullptr);
    input.setOnFocusEdgeHit(nullptr);
    input.setEdgeHitMinInterval(0.12f);
}

} // namespace

int main()
{
    bklv::initResources();
    setTheme("default");

    InputManager& input = getInputManager();

    std::printf("\n== 1. 记住 → 恢复：焦点回原控件 ==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 300.0f);
        FocusBox* b1 = makeFocus();
        FocusBox* b2 = makeFocus();
        FocusBox* b3 = makeFocus();
        root->addView(b1);
        root->addView(b2);
        root->addView(b3);
        layoutTree(root, 400.0f, 300.0f);

        input.setRoot(root);
        input.giveFocus(b2);
        check(input.getCurrentFocus() == b2, "焦点在 b2");

        input.remember();
        input.giveFocus(b3);
        check(input.getCurrentFocus() == b3, "焦点先移到 b3");

        check(input.loadFocus(), "loadFocus() 报告恢复成功");
        check(input.getCurrentFocus() == b2, "焦点回到记下的原控件 b2");

        // 焦点变化复用既有的 FocusChangedEvent（不新增并行事件）
        std::vector<View*> seen;
        {
            Subscription handle = getEventBus().subscribe<FocusChangedEvent>(
                [&seen](const FocusChangedEvent& event) { seen.push_back(event.current); });

            input.giveFocus(b1);
            check(!seen.empty() && seen.back() == b1, "焦点变化发既有的 FocusChangedEvent");
            check(seen.size() == 1, "一次焦点变化只发一次");
        }

        // 记了但没动过时也要能恢复
        input.remember();
        check(input.loadFocus(), "没移动过时 loadFocus() 也算成功");

        input.setRoot(nullptr);
        input.resetFocusState();
        delete root;
    }

    std::printf("\n== 2. 原控件已销毁 → 退该范围的默认焦点（不崩、不悬空）==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 300.0f);

        Box* pageA = new Box(Axis::COLUMN);
        pageA->setDimensions(300.0f, 200.0f);
        FocusBox* a1 = makeFocus();
        FocusBox* a2 = makeFocus();
        pageA->addView(a1);
        pageA->addView(a2);
        root->addView(pageA);

        Box* pageB = new Box(Axis::COLUMN);
        pageB->setDimensions(300.0f, 100.0f);
        FocusBox* b1 = makeFocus();
        pageB->addView(b1);
        root->addView(pageB);

        layoutTree(root, 400.0f, 300.0f);
        input.setRoot(root);

        input.giveFocus(a2);
        input.remember(); // 路径 = "0/1"

        // 整个页销毁：a2 也跟着没了
        root->removeView(pageA, true);
        check(input.getCurrentFocus() == b1, "被销毁的焦点自动换到范围内的默认焦点（不悬空）");

        check(!input.loadFocus(), "目标不在了：loadFocus() 返回 false");
        check(input.getCurrentFocus() == b1, "恢复失败时退默认焦点，而不是把焦点置空");

        input.setRoot(nullptr);
        input.resetFocusState();
        delete root;
    }

    std::printf("\n== 3. 默认焦点三级（自身 / 上次记忆 / 指定索引 / 顺序第一个）==\n");
    {
        resetInput();

        // ① 自身可聚焦 → 自己
        FocusBox* self = makeFocus();
        FocusBox* inner = makeFocus();
        self->addView(inner);
        check(self->getDefaultFocus() == self, "① 自己可聚焦时默认焦点是自己");

        // ② 上次焦点记忆
        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 400.0f);
        Box* innerA = new Box(Axis::COLUMN);
        innerA->setDimensions(300.0f, 200.0f);
        FocusBox* a1 = makeFocus();
        FocusBox* a2 = makeFocus();
        FocusBox* a3 = makeFocus();
        innerA->addView(a1);
        innerA->addView(a2);
        innerA->addView(a3);

        Box* innerB = new Box(Axis::COLUMN);
        innerB->setDimensions(300.0f, 100.0f);
        FocusBox* b1 = makeFocus();
        innerB->addView(b1);

        root->addView(innerA);
        root->addView(innerB);
        layoutTree(root, 400.0f, 400.0f);
        input.setRoot(root);

        check(innerA->getDefaultFocus() == a1, "没有记忆时默认焦点是顺序第一个");
        input.giveFocus(a3);
        check(innerA->getLastFocusedView() == a3, "焦点变化把「哪一支」写进路由路径上的 Box");
        input.giveFocus(b1);
        check(innerA->getDefaultFocus() == a3, "② 上次焦点记忆优先于顺序第一个");

        // ③ 指定索引
        innerA->setDefaultFocusedIndex(2);
        input.giveFocus(b1);      // 不动 innerA 的记忆（焦点不在它里面）
        check(innerA->getDefaultFocusedIndex() == 2, "指定索引记下来了");
        check(innerA->getDefaultFocus() == a3, "记忆还在时，指定索引不抢（记忆优先）");

        Box* fresh = new Box(Axis::COLUMN);
        fresh->setDimensions(300.0f, 200.0f);
        FocusBox* f0 = makeFocus();
        FocusBox* f1 = makeFocus();
        FocusBox* f2 = makeFocus();
        fresh->addView(f0);
        fresh->addView(f1);
        fresh->addView(f2);
        layoutTree(fresh, 300.0f, 200.0f);

        fresh->setDefaultFocusedIndex(2);
        check(fresh->getDefaultFocus() == f2, "③ 没有记忆时用 setDefaultFocusedIndex 指定的索引");

        fresh->setDefaultFocusedIndex(-1);
        check(fresh->getDefaultFocus() == f0, "④ -1 = 没指定，顺序扫第一个可聚焦子节点");

        // 索引越界/指向不可聚焦的节点时，退回顺序扫描
        Box* skipped = new Box(Axis::COLUMN);
        skipped->setDimensions(300.0f, 200.0f);
        Box* plain = new Box(Axis::COLUMN); // 不可聚焦的容器
        plain->setDimensions(100.0f, 40.0f);
        FocusBox* s1 = makeFocus();
        skipped->addView(plain);
        skipped->addView(s1);
        layoutTree(skipped, 300.0f, 200.0f);
        skipped->setDefaultFocusedIndex(5);
        check(skipped->getDefaultFocus() == s1, "指定索引越界时退回顺序扫描");

        // 禁用的控件不参与默认焦点
        Box* withDisabled = new Box(Axis::COLUMN);
        withDisabled->setDimensions(300.0f, 200.0f);
        Button* disabled = new Button("禁用");
        Button* enabled = new Button("可用");
        disabled->setEnabled(false);
        withDisabled->addView(disabled);
        withDisabled->addView(enabled);
        layoutTree(withDisabled, 300.0f, 200.0f);
        check(withDisabled->getDefaultFocus() == enabled, "禁用的控件被跳过（isDisabled）");

        input.setRoot(nullptr);
        input.resetFocusState();
        delete root;
        delete fresh;
        delete skipped;
        delete withDisabled;
        delete self;
    }

    std::printf("\n== 4. 顺序导航 + 轴向不符上交 + 越界冒泡 ==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(600.0f, 400.0f);

        Box* rowBox = new Box(Axis::ROW);
        rowBox->setDimensions(600.0f, 60.0f);
        FocusBox* r1 = makeFocus();
        FocusBox* r2 = makeFocus();
        rowBox->addView(r1);
        rowBox->addView(r2);

        Box* colBox = new Box(Axis::COLUMN);
        colBox->setDimensions(600.0f, 200.0f);
        FocusBox* c1 = makeFocus();
        FocusBox* c2 = makeFocus();
        FocusBox* c3 = makeFocus();
        colBox->addView(c1);
        colBox->addView(c2);
        colBox->addView(c3);

        Box* tail = new Box(Axis::COLUMN);
        tail->setDimensions(600.0f, 100.0f);
        FocusBox* t1 = makeFocus();
        tail->addView(t1);

        root->addView(rowBox);
        root->addView(colBox);
        root->addView(tail);
        layoutTree(root, 600.0f, 400.0f);
        input.setRoot(root);

        input.giveFocus(r1);
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == c1,
            "row 里按 DOWN：轴向不符立刻上交父容器，落到下一个 box 的默认焦点");

        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == c2, "同容器内 DOWN 走子索引 +1 的下一个");

        input.giveFocus(c3);
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == t1, "本层越界 → 冒泡到父容器继续找");

        input.giveFocus(r1);
        input.navigate(FocusDirection::UP);
        check(input.getCurrentFocus() == r1, "到头了（无处可去）：焦点停在原处");

        input.giveFocus(c2);
        input.navigate(FocusDirection::UP);
        check(input.getCurrentFocus() == c1, "同容器内 UP 走子索引 -1");

        // 顺序扫描 ≠ 几何最近邻：用绝对定位把下标 0 的节点摆到屏幕下方
        Box* vert = new Box(Axis::COLUMN);
        vert->setDimensions(300.0f, 300.0f);
        FocusBox* low = makeFocus();
        low->setPositionType(PositionType::ABSOLUTE);
        low->setPositionTop(200.0f);
        FocusBox* high = makeFocus();
        vert->addView(low);
        vert->addView(high);
        layoutTree(vert, 300.0f, 300.0f);

        check(low->getY() > high->getY(), "布局确认：下标 0 的节点被摆在屏幕下方");
        check(low->getNextFocus(FocusDirection::DOWN, low) == high,
            "DOWN 取子索引 +1（几何最近邻在上方，取不到）");

        input.setRoot(nullptr);
        input.resetFocusState();
        delete root;
        delete vert;
    }

    std::printf("\n== 5. 导航循环：默认不循环，setNavigationWrap 后首尾相接 ==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 300.0f);
        Box* box = new Box(Axis::COLUMN);
        box->setDimensions(300.0f, 240.0f);
        FocusBox* x1 = makeFocus();
        FocusBox* x2 = makeFocus();
        FocusBox* x3 = makeFocus();
        box->addView(x1);
        box->addView(x2);
        box->addView(x3);
        root->addView(box);
        layoutTree(root, 400.0f, 300.0f);
        input.setRoot(root);

        input.giveFocus(x1);
        input.navigate(FocusDirection::UP);
        check(input.getCurrentFocus() == x1, "默认不循环：首项按 UP 停住");

        input.giveFocus(x3);
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == x3, "默认不循环：末项按 DOWN 停住");

        input.setNavigationWrap(box, true);
        check(box->isNavigationWrap(), "setNavigationWrap(box, true) 落在 box 上");

        input.giveFocus(x1);
        input.navigate(FocusDirection::UP);
        check(input.getCurrentFocus() == x3, "开循环后首项 UP 绕到末项");

        input.giveFocus(x3);
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == x1, "开循环后末项 DOWN 绕回首项");

        input.giveFocus(x2);
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == x3, "开循环不影响正常的相邻移动");

        // 作用域：影响子树、内层可覆盖
        Box* outer = new Box(Axis::COLUMN);
        outer->setDimensions(300.0f, 200.0f);
        Box* innerA = new Box(Axis::COLUMN);
        innerA->setDimensions(200.0f, 200.0f);
        FocusBox* a1 = makeFocus();
        FocusBox* a2 = makeFocus();
        innerA->addView(a1);
        innerA->addView(a2);
        outer->addView(innerA);

        input.setNavigationWrap(outer, true);
        check(innerA->isNavigationWrap(), "wrap 作用到子树（内层继承）");
        input.setNavigationWrap(innerA, false);
        check(!innerA->isNavigationWrap(), "内层 setNavigationWrap(false) 覆盖外层");

        check(box->isNavigationWrap(), "box 自己的设置不受别的树影响");

        // wrap 优先于跨 box 策略：本 box 内能绕回候选时，NONE 也拦不住
        input.setCrossBoxPolicy(box, CrossBoxPolicy::NONE);
        input.giveFocus(x1);
        input.navigate(FocusDirection::UP);
        check(input.getCurrentFocus() == x3, "wrap 优先于跨 box 策略（NONE 也绕得回去）");
        input.setCrossBoxPolicy(box, CrossBoxPolicy::ALLOW);

        input.setRoot(nullptr);
        input.resetFocusState();
        delete root;
        delete outer;
    }

    std::printf("\n== 6. 撞边反馈：方向、只触发一次、节流、焦点为空不触发 ==\n");
    {
        resetInput();

        int hits = 0;
        FocusDirection lastDirection = FocusDirection::NONE;
        View* lastView = nullptr;
        input.setOnFocusEdgeHit([&](FocusDirection direction, View* current) {
            hits++;
            lastDirection = direction;
            lastView      = current;
        });

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 300.0f);
        Box* box = new Box(Axis::COLUMN);
        box->setDimensions(300.0f, 240.0f);
        FocusBox* x1 = makeFocus();
        FocusBox* x2 = makeFocus();
        box->addView(x1);
        box->addView(x2);
        root->addView(box);
        layoutTree(root, 400.0f, 300.0f);
        input.setRoot(root);

        input.giveFocus(x1);
        input.navigate(FocusDirection::DOWN);
        check(hits == 0, "能移动时不触发撞边");
        check(input.getCurrentFocus() == x2, "（确实移动了）");

        input.giveFocus(x1);
        input.navigate(FocusDirection::UP);
        check(hits == 1, "确实走不动时触发一次");
        check(lastDirection == FocusDirection::UP && lastView == x1,
            "回调带正确的按键方向和当前焦点");
        check(x1->isHighlightShaking(), "撞边时焦点框开始抖（库自己的视觉反馈）");

        input.navigate(FocusDirection::UP);
        check(hits == 1, "节流：最小间隔内的第二次不触发");

        input.advanceClock(0.2f);
        input.navigate(FocusDirection::UP);
        check(hits == 2, "过了最小间隔后再次触发（长按到边界不狂响）");

        const Rect before = x1->getFrame();
        input.shakeHighlightOnEdge(FocusDirection::DOWN);
        const Rect after = x1->getFrame();
        checkFloat(after.x, before.x, "抖动不进布局（x 不变）");
        checkFloat(after.y, before.y, "抖动不进布局（y 不变）");

        input.resetFocusState();
        input.navigate(FocusDirection::DOWN);
        check(hits == 2, "焦点为空时不触发撞边（只挑默认焦点）");
        check(input.getCurrentFocus() == x1, "焦点为空时按方向键先选默认焦点");

        input.setRoot(nullptr);
        input.resetFocusState();
        delete root;
    }

    std::printf("\n== 7. 跨 box 三档：ALLOW / SAME_PARENT_ONLY / NONE ==\n");
    {
        resetInput();

        int hits = 0;
        input.setOnFocusEdgeHit([&](FocusDirection, View*) { hits++; });

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 400.0f);

        Box* topBox = new Box(Axis::COLUMN);
        topBox->setDimensions(300.0f, 200.0f);
        Box* group = new Box(Axis::COLUMN);
        group->setDimensions(200.0f, 200.0f);
        FocusBox* g1 = makeFocus();
        FocusBox* g2 = makeFocus();
        group->addView(g1);
        group->addView(g2);
        topBox->addView(group);

        Box* bottomBox = new Box(Axis::COLUMN);
        bottomBox->setDimensions(300.0f, 100.0f);
        FocusBox* b1 = makeFocus();
        bottomBox->addView(b1);

        root->addView(topBox);
        root->addView(bottomBox);
        layoutTree(root, 400.0f, 400.0f);
        input.setRoot(root);

        check(group->getCrossBoxPolicy() == CrossBoxPolicy::ALLOW, "默认策略是 ALLOW");

        input.giveFocus(g2);
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == b1, "ALLOW：越界后冒泡出去，落到 bottomBox");

        input.setCrossBoxPolicy(group, CrossBoxPolicy::SAME_PARENT_ONLY);
        input.giveFocus(g2);
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == g2, "SAME_PARENT_ONLY：同父容器里没有别的兄弟 → 出不去");

        input.setCrossBoxPolicy(group, CrossBoxPolicy::ALLOW);
        input.giveFocus(g2);
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == b1, "改回 ALLOW 又能出去");

        input.setCrossBoxPolicy(group, CrossBoxPolicy::NONE);
        input.giveFocus(g2);
        hits = 0;
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == g2, "NONE：停在边界，焦点不动");
        check(hits == 0, "被策略主动拦住不算撞边（不触发回调）");

        // SAME_PARENT_ONLY 的"允许"那一半：同父容器内的兄弟 box 可以进
        Box* top2 = new Box(Axis::COLUMN);
        top2->setDimensions(400.0f, 300.0f);
        Box* group2 = new Box(Axis::COLUMN);
        group2->setDimensions(200.0f, 200.0f);
        FocusBox* h1 = makeFocus();
        FocusBox* h2 = makeFocus();
        group2->addView(h1);
        group2->addView(h2);
        Box* sibling = new Box(Axis::COLUMN);
        sibling->setDimensions(200.0f, 100.0f);
        FocusBox* s1 = makeFocus();
        sibling->addView(s1);
        top2->addView(group2);
        top2->addView(sibling);
        layoutTree(top2, 400.0f, 300.0f);

        input.setRoot(top2);
        input.setCrossBoxPolicy(group2, CrossBoxPolicy::SAME_PARENT_ONLY);
        input.giveFocus(h2);
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == s1, "SAME_PARENT_ONLY 允许换到同父容器的兄弟 box");

        // 外层 NONE + 内层显式 ALLOW：内层能换兄弟，但到外层的边界还是被拦住
        input.setCrossBoxPolicy(top2, CrossBoxPolicy::NONE);
        input.setCrossBoxPolicy(group2, CrossBoxPolicy::ALLOW);
        input.giveFocus(h2);
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == s1, "内层 ALLOW 覆盖：能换到同父兄弟");
        hits = 0;
        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == s1, "到了外层 NONE 的边界就停下");
        check(hits == 0, "外层 NONE 拦住时同样不触发撞边");

        input.setRoot(nullptr);
        input.resetFocusState();
        delete root;
        delete top2;
    }

    std::printf("\n== 8. 模态遮挡：焦点不跑到模态外 ==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 400.0f);

        Box* content = new Box(Axis::COLUMN);
        content->setDimensions(300.0f, 100.0f);
        FocusBox* c1 = makeFocus();
        content->addView(c1);

        Box* modalBox = new Box(Axis::COLUMN);
        modalBox->setDimensions(300.0f, 100.0f);
        FocusBox* m1 = makeFocus();
        modalBox->addView(m1);

        Box* hud = new Box(Axis::COLUMN);
        hud->setDimensions(300.0f, 100.0f);
        FocusBox* h1 = makeFocus();
        hud->addView(h1);

        root->addView(content);
        root->addView(modalBox);
        root->addView(hud);
        layoutTree(root, 400.0f, 400.0f);
        input.setRoot(root);

        input.giveFocus(c1);
        input.pushModal(modalBox);

        check(input.getCurrentFocus() == m1, "模态打开后焦点进模态里的默认焦点");
        check(input.canFocus(m1), "canFocus：模态内可聚焦");
        check(!input.canFocus(h1), "canFocus：模态外不可聚焦（isInsideModal 为假）");
        check(input.canFocus(c1) == false, "模态下面的内容层也拿不到焦点");

        input.navigate(FocusDirection::DOWN);
        check(input.getCurrentFocus() == m1, "方向键不会把焦点交到模态外（候选被丢掉）");

        input.popModal();
        check(!input.hasModal(), "关掉模态后模态栈空了");
        check(input.getCurrentFocus() == c1, "关掉模态后焦点回到原处");

        input.setRoot(nullptr);
        input.resetFocusState();
        delete root;
    }

    std::printf("\n== 9. 同层换页记忆：rememberFor / loadFocusFor ==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 400.0f);
        Box* host = new Box(Axis::COLUMN);
        host->setDimensions(300.0f, 300.0f);
        root->addView(host);
        layoutTree(root, 400.0f, 400.0f);
        input.setRoot(root);

        Box* pageA = new Box(Axis::COLUMN);
        pageA->setDimensions(300.0f, 200.0f);
        FocusBox* a1 = makeFocus();
        FocusBox* a2 = makeFocus();
        pageA->addView(a1);
        pageA->addView(a2);

        Box* pageB = new Box(Axis::COLUMN);
        pageB->setDimensions(300.0f, 200.0f);
        FocusBox* b1 = makeFocus();
        FocusBox* b2 = makeFocus();
        pageB->addView(b1);
        pageB->addView(b2);

        // 第 1 页：记住 a2
        host->addView(pageA);
        layoutTree(root, 400.0f, 400.0f);
        input.giveFocus(a2);
        input.rememberFor("pageA");
        check(input.getCurrentFocus() == a2, "pageA 上记住的是 a2");

        // 换到第 2 页（pageA 摘下但不销毁）
        host->removeAllViews();
        host->addView(pageB);
        layoutTree(root, 400.0f, 400.0f);
        input.giveFocus(b1);
        check(input.getCurrentFocus() == b1, "换页后焦点在新页上");

        // 换回 pageA，再按 key 恢复
        host->removeAllViews();
        host->addView(pageA);
        layoutTree(root, 400.0f, 400.0f);

        check(input.loadFocusFor("pageA"), "loadFocusFor(\"pageA\") 成功");
        check(input.getCurrentFocus() == a2, "回到 pageA 的原控件 a2（按记录的路径）");
        check(!input.loadFocusFor("pageZ"), "没记过的 key 返回 false");
        check(input.getCurrentFocus() == a2, "未知 key 不改动当前焦点");

        input.setRoot(nullptr);
        input.resetFocusState();
        delete root;
        delete pageB; // pageA 随 root 一起销毁
    }

    std::printf("\n== 10. ensureFocusVisible：把焦点元素滚回可视区 ==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 300.0f);

        ScrollingFrame* scroller = new ScrollingFrame();
        scroller->setGrow(1.0f);

        Box* content = new Box(Axis::COLUMN);
        content->setDimensions(400.0f, 900.0f);
        FocusBox* items[9] = { nullptr };
        for (int i = 0; i < 9; i++)
        {
            items[i] = makeFocus(100.0f, 100.0f);
            content->addView(items[i]);
        }

        scroller->setContentView(content);
        root->addView(scroller);
        layoutTree(root, 400.0f, 300.0f);
        input.setRoot(root);

        FocusBox* target = items[7]; // y = 700，远在 300 高的可视区之外
        input.giveFocus(target);

        scroller->update();                        // 消费一次"焦点跟随"
        scroller->setContentOffsetY(0.0f, false);  // 手动滚回顶部
        scroller->update();

        checkFloat(scroller->getContentOffsetY(), 0.0f, "先把内容滚回顶部");
        check(target->getY() > scroller->getY() + scroller->getHeight(),
            "焦点元素确实在可视区外");
        check(scroller->isScrollable(), "滚动容器认得出自己是滚动容器");

        input.ensureFocusVisible();
        for (int i = 0; i < 80; i++)
            scroller->update(); // 缓动跑完

        check(scroller->getContentOffsetY() > 100.0f, "ensureFocusVisible() 把内容滚上去了");

        const Rect frame  = scroller->getFrame();
        const Rect target2 = target->getFrame();
        check(target2.y >= frame.y && target2.getMaxY() <= frame.getMaxY(),
            "焦点元素回到可视区内");

        // 没焦点时调它什么都不做（不崩）
        input.resetFocusState();
        input.ensureFocusVisible();
        check(input.getCurrentFocus() == nullptr, "焦点为空时 ensureFocusVisible() 安全");

        input.setRoot(nullptr);
        input.resetFocusState();
        delete root;
    }

    std::printf("\n== 11. 滚动容器：走不动时「返回 this」—— 焦点不动，只滚动 ==\n");
    {
        resetInput();

        int hits = 0;
        input.setOnFocusEdgeHit([&](FocusDirection, View*) { hits++; });

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(400.0f, 300.0f);

        Box* top = new Box(Axis::COLUMN);
        top->setDimensions(400.0f, 40.0f);
        FocusBox* t1 = makeFocus(100.0f, 40.0f);
        top->addView(t1);

        ScrollingFrame* scroller = new ScrollingFrame();
        scroller->setGrow(1.0f);
        Box* content = new Box(Axis::COLUMN);
        content->setDimensions(400.0f, 900.0f);
        FocusBox* items[9] = { nullptr };
        for (int i = 0; i < 9; i++)
        {
            items[i] = makeFocus(100.0f, 100.0f);
            content->addView(items[i]);
        }
        scroller->setContentView(content);

        Box* bottom = new Box(Axis::COLUMN);
        bottom->setDimensions(400.0f, 40.0f);
        FocusBox* b1 = makeFocus(100.0f, 40.0f);
        bottom->addView(b1);

        root->addView(top);
        root->addView(scroller);
        root->addView(bottom);
        layoutTree(root, 400.0f, 300.0f);
        input.setRoot(root);

        // 焦点在第一项，但手动把内容滚下去 50px，让它只露出一半
        input.giveFocus(items[0]);
        scroller->update();                      // 消费一次"焦点跟随"
        scroller->setContentOffsetY(50.0f, false);
        scroller->update();

        checkFloat(scroller->getContentOffsetY(), 50.0f, "焦点元素被滚到只露一半");

        // UP：内容层扫不到候选，滚动容器接管（返回 this）→ 焦点不动、把它滚回来
        input.navigate(FocusDirection::UP);
        for (int i = 0; i < 40; i++)
            scroller->update();

        check(input.getCurrentFocus() == items[0], "走不动时焦点留在原地（没有跑出滚动器）");
        check(hits == 0, "容器接管不算撞边（不触发反馈）");
        check(scroller->getContentOffsetY() < 50.0f, "容器把焦点元素滚回了可视区");

        const Rect frame = scroller->getFrame();
        const Rect item0 = items[0]->getFrame();
        check(item0.y >= frame.y && item0.getMaxY() <= frame.getMaxY(), "焦点元素完全露出来了");

        input.setRoot(nullptr);
        input.resetFocusState();
        delete root;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

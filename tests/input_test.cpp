// 无窗口测试：输入处理与响应发生在**同一步**里。
//
// 为什么专门钉这条：`App::frame()` 的次序是
//   ① 布局（命中判定的前提）→ ② `InputManager::update()`（读布局结果，改焦点/派发动作）
//   → ③ `root->frame()`（绘制）
// 也就是说"输入处理里已经改完状态"就等于"同一帧末尾画出来"。这条耦合是隐性的：
// 谁把 update() 挪到绘制之后，或者把焦点/动作改成"下一帧再生效"，界面就会慢一帧，
// 而所有既有测试仍然全绿（评审 §4.2 P1「输入读上一帧命中结果」记过）。
// 这里在**不建 App、不开窗口**的前提下，把"同一步内完成"这件事钉住：
// 每个断言后面都紧跟"没有再调用一次 update 就已经是这个结果"。
//
// 不做的事：真机上的鼠标命中要经过 `ImGui::GetIO()`（无窗口时没有上下文），
// 所以这里用 `dispatchPointerClick()` 直接喂坐标 —— 它读的就是已经算好的布局。

#include <cstdio>
#include <string>
#include <vector>

#include <yoga/Yoga.h>

#include "bklv/core/box.hpp"
#include "bklv/core/event.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/view.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 最小可聚焦视图：和 focus_test 里同一套路（Box 默认不可聚焦）
class FocusBox : public Box
{
  public:
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

void resetInput()
{
    InputManager& input = getInputManager();
    input.resetFocusState();
    input.setRoot(nullptr);
    input.setOnFocusEdgeHit(nullptr);
}

} // namespace

int main()
{
    bklv::initResources();
    setTheme("default");

    InputManager& input = getInputManager();

    std::printf("\n== 1. 导航：同一次调用里焦点就变了（不拖到下一帧）==\n");
    {
        resetInput();

        Box* root = new Box(Axis::ROW);
        root->setDimensions(400.0f, 100.0f);
        FocusBox* left  = makeFocus();
        FocusBox* right = makeFocus();
        root->addView(left);
        root->addView(right);
        layoutTree(root, 400.0f, 100.0f);

        input.setRoot(root);
        input.giveFocus(left);

        // 焦点变化事件也必须在这一次调用里同步派发（同一步 = 同一帧）
        std::vector<View*> seen;
        Subscription handle = getEventBus().subscribe<FocusChangedEvent>(
            [&seen](const FocusChangedEvent& event) { seen.push_back(event.current); });

        input.navigate(FocusDirection::RIGHT);
        check(input.getCurrentFocus() == right, "navigate() 返回时焦点已经在 right 上");
        checkEqualInt((int)seen.size(), 1, "焦点变化事件在这同一步里就派发了");
        if (!seen.empty())
            check(seen.back() == right, "事件带的是新焦点");

        // 反向同理：没有"下一帧才生效"的中间态
        input.navigate(FocusDirection::LEFT);
        check(input.getCurrentFocus() == left, "再往左立刻回到 left");

        input.setRoot(nullptr);
        resetInput();
    }

    std::printf("\n== 2. 指针命中读的是已经算好的布局（同一步，不重排）==\n");
    {
        resetInput();

        Box* root = new Box(Axis::ROW);
        root->setDimensions(400.0f, 100.0f);
        FocusBox* left  = makeFocus();
        FocusBox* right = makeFocus();
        root->addView(left);
        root->addView(right);
        layoutTree(root, 400.0f, 100.0f);

        input.setRoot(root);
        input.giveFocus(left);

        // right 的中心（布局算出来的位置），不重新布局直接喂进去
        const Point center(right->getX() + right->getWidth() * 0.5f,
            right->getY() + right->getHeight() * 0.5f);
        check(input.dispatchPointerClick(center), "这次点击被处理了");
        check(input.getCurrentFocus() == right, "点一下焦点就到 right —— 命中用的是本次布局");

        const Point blank(root->getX() - 50.0f, root->getY() - 50.0f);
        check(!input.dispatchPointerClick(blank), "点到空白不命中（返回 false）");
        check(input.getCurrentFocus() == right, "点空白不会把焦点弄丢");

        input.setRoot(nullptr);
        resetInput();
    }

    std::printf("\n== 3. 动作：监听器在同一次派发里执行完（不排队到下一帧）==\n");
    {
        resetInput();

        Box* root = new Box(Axis::COLUMN);
        root->setDimensions(200.0f, 200.0f);
        FocusBox* button = makeFocus();
        root->addView(button);
        layoutTree(root, 200.0f, 200.0f);

        input.setRoot(root);
        input.giveFocus(button);

        int fires = 0;
        button->registerAction(ControllerButton::A, "确认",
            [&fires](View*) { fires++; });

        check(input.dispatchAction(ControllerButton::A), "A 被处理");
        checkEqualInt(fires, 1, "监听器在 dispatchAction() 返回前就执行了");
        check(input.dispatchAction(ControllerButton::A), "再按一次还能处理");
        checkEqualInt(fires, 2, "每次派发各算一次（没有被缓存成\"下一帧只响应一次\"）");

        // 没有焦点时不能凭空派发
        input.giveFocus(nullptr);
        check(!input.dispatchAction(ControllerButton::A), "没有焦点时不派发动作");
        checkEqualInt(fires, 2, "不派发也就不该触发监听器");

        input.setRoot(nullptr);
        resetInput();
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

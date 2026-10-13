// 无窗口测试：第 4 步剩余部分（弹窗交互 + 跨层焦点 + 系统键互斥）。
//
// 覆盖（对策见 docs/shell-implementation-plan.md 的 v14 / v15）：
//   1. closeOnDimClick：点弹窗矩形外关闭（F1(b)：不动 hitTest/focusFromPointer 的焦点语义）
//   2. closeOnBack：读取点是模态登记的 onUnhandled 兜底 → popDialog（弹窗栈/遮罩栈不脱节）
//   3. B 键**限定子树派发**（F2）：弹窗内没人处理时不许冒泡到外壳默认动作
//   4. enterLayer / leaveLayer（F4：void leaveLayer）
//   5. 焦点记忆是**栈**（F5）：弹窗套弹窗逐级还原
//   6. 系统键与 handleActions 同帧双响应互斥（isActionConsumedThisFrame）
//
// 布局手工驱动（YGNodeCalculateLayout → getX/getY/getWidth/getHeight），纯计算，不需要窗口。
// 点击走设备无关入口 dispatchPointerClick（v15：handlePointer 直接读 ImGui::GetIO()，
// 无窗口测试驱动不了）。

#include <cstdio>

#include <yoga/Yoga.h>

#include "bklv/core/appearance.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/view.hpp"
#include "bklv/widgets/shell.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

// 最小可聚焦视图（Box 默认不可聚焦，只打开这一个钩子）
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

void resetInput()
{
    InputManager& input = getInputManager();
    input.resetFocusState();
    input.setRoot(nullptr);
    input.setOnFocusEdgeHit(nullptr);
    input.setEdgeHitMinInterval(0.12f);
}

// 外壳 + 内容层一个可聚焦控件
struct Scene
{
    Shell* shell      = nullptr;
    FocusBox* content = nullptr;
};

Scene makeScene()
{
    Scene scene;

    scene.shell = new Shell();
    scene.shell->setDimensions(800.0f, 600.0f);

    scene.content = makeFocus(200.0f, 60.0f);
    scene.shell->getContentLayer()->addView(scene.content);

    getInputManager().setRoot(scene.shell);
    layoutTree(scene.shell, 800.0f, 600.0f);
    getInputManager().giveFocus(scene.content);

    return scene;
}

// 造一个"左上角固定在 (100,100)、300x200 的弹窗 + 里面一个可聚焦控件"，
// 压进外壳并重排。返回非空时弹窗归外壳（popDialog 会把 free 的默认值当 true）。
Box* pushCenteredDialog(Shell* shell, FocusBox*& innerOut,
    const DialogOptions& options = DialogOptions())
{
    Box* dialog = new Box(Axis::COLUMN);
    dialog->setDimensions(300.0f, 200.0f);
    dialog->setPositionType(PositionType::ABSOLUTE);
    dialog->setPositionTop(100.0f);
    dialog->setPositionLeft(100.0f);

    FocusBox* inner = makeFocus(120.0f, 40.0f);
    dialog->addView(inner);
    innerOut = inner;

    shell->pushDialog(dialog, options);
    layoutTree(shell, 800.0f, 600.0f);
    return dialog;
}

// 收尾：把输入层恢复干净再删外壳（resetFocusState 必须在删视图之前，否则模态栈是野指针）
void teardown(Scene& scene)
{
    getInputManager().setRoot(nullptr);
    resetInput();
    delete scene.shell;
    scene.shell = nullptr;
}

} // namespace

int main()
{
    bklv::initResources();
    setTheme("default");

    InputManager& input = getInputManager();

    std::printf("\n== 1. closeOnDimClick：点弹窗矩形外关闭 ==\n");
    {
        resetInput();
        Scene scene = makeScene();

        FocusBox* inner = nullptr;
        Box* dialog     = pushCenteredDialog(scene.shell, inner);

        check(input.hasModal(), "pushDialog 后进入模态");
        checkFloat(dialog->getFrame().x, 100.0f, "弹窗矩形 x（绝对坐标）");
        checkFloat(dialog->getFrame().width, 300.0f, "弹窗矩形宽度");

        // 点在弹窗**内部**的空白处：不算"外面"，不关
        input.dispatchPointerClick(Point(250.0f, 200.0f));
        check(scene.shell->hasDialog(), "点在弹窗内（空白处）不关闭");

        // 点在弹窗矩形外（遮罩上）：关闭
        input.dispatchPointerClick(Point(700.0f, 500.0f));
        check(!scene.shell->hasDialog(), "点在弹窗之外 → 弹窗关闭");
        check(!input.hasModal(), "关闭后退出模态");
        check(scene.shell->getDimLayer() == nullptr, "遮罩跟着一起摘掉");

        // closeOnDimClick=false：点外面不关
        DialogOptions noClick;
        noClick.closeOnDimClick = false;
        FocusBox* inner2 = nullptr;
        Box* dialog2     = pushCenteredDialog(scene.shell, inner2, noClick);
        input.dispatchPointerClick(Point(700.0f, 500.0f));
        check(scene.shell->hasDialog(), "closeOnDimClick=false 时点外面不关");
        scene.shell->popDialog(dialog2, false); // free=false：这个视图在测试里自己删
        delete dialog2;

        teardown(scene);
    }

    std::printf("\n== 2. closeOnBack：B 关闭弹窗（读取点已接上）==\n");
    {
        resetInput();
        Scene scene = makeScene();

        FocusBox* inner = nullptr;
        pushCenteredDialog(scene.shell, inner);
        check(input.getCurrentFocus() == inner, "模态打开后焦点进弹窗默认焦点");

        check(input.dispatchAction(ControllerButton::B), "B 被消费（closeOnBack 生效）");
        check(!scene.shell->hasDialog(), "B 把弹窗关掉了");
        check(!input.hasModal(), "B 关闭后退出模态");
        check(input.getCurrentFocus() == scene.content, "关闭后焦点回到打开前的控件");
        check(scene.shell->getDimLayer() == nullptr, "B 关闭时遮罩也摘掉");

        // closeOnBack=false：弹窗内没人处理 B 时什么都不做
        DialogOptions noBack;
        noBack.closeOnBack = false;
        FocusBox* inner2 = nullptr;
        Box* dialog2     = pushCenteredDialog(scene.shell, inner2, noBack);
        check(!input.dispatchAction(ControllerButton::B), "closeOnBack=false 时 B 未被消费");
        check(scene.shell->hasDialog(), "closeOnBack=false 时弹窗不关");
        scene.shell->popDialog(dialog2, false);
        delete dialog2;

        teardown(scene);
    }

    std::printf("\n== 3. B 键限定子树派发（F2：不冒泡到外壳默认动作）==\n");
    {
        resetInput();
        Scene scene = makeScene();

        bool shellBackCalled = false;
        scene.shell->setOnBackRequested([&shellBackCalled] {
            shellBackCalled = true;
            return true;
        });
        check(scene.shell->hasBackRequestedHandler(), "外壳记录了 B 默认动作");

        // 非模态：焦点在内容层控件上按 B → 沿父链冒泡到外壳
        input.giveFocus(scene.content);
        check(input.dispatchAction(ControllerButton::B), "非模态时 B 冒泡到外壳并被消费");
        check(shellBackCalled, "外壳的 B 默认动作被执行");

        // 弹窗打开：没人处理 B，且 closeOnBack=false → **不许**冒泡到外壳
        DialogOptions noBack;
        noBack.closeOnBack = false;
        FocusBox* inner = nullptr;
        Box* dialog     = pushCenteredDialog(scene.shell, inner, noBack);
        shellBackCalled = false;
        check(!input.dispatchAction(ControllerButton::B), "弹窗内没人处理 B → 未消费");
        check(!shellBackCalled, "B 没有冒泡到外壳默认动作（限定子树派发）");
        check(scene.shell->hasDialog(), "弹窗还在");

        // 弹窗内自己注册 B 动作 → 消费掉，弹窗不关
        bool dialogBackCalled = false;
        inner->registerAction(ControllerButton::B, "取消",
            [&dialogBackCalled](View*) { dialogBackCalled = true; }, true);
        check(input.dispatchAction(ControllerButton::B), "弹窗内的 B 动作被消费");
        check(dialogBackCalled, "弹窗内的 B 回调被执行");
        check(scene.shell->hasDialog(), "弹窗自己处理 B 时不关闭");
        check(!shellBackCalled, "弹窗处理 B 后仍然不冒泡到外壳");

        scene.shell->popDialog(dialog, false);
        delete dialog;

        teardown(scene);
    }

    std::printf("\n== 4. enterLayer / leaveLayer（F4：void leaveLayer）==\n");
    {
        resetInput();
        Scene scene = makeScene();

        // 两个"层"要真的挂在外壳里：焦点记忆靠**从范围根出发的下标路径**定位，
        // 不挂进树的话路径根本建不出来（这本身也是个值得记住的约束）。
        Box* layerA  = new Box(Axis::COLUMN);
        layerA->setDimensions(100.0f, 100.0f);
        FocusBox* a1 = makeFocus();
        layerA->addView(a1);
        scene.shell->getContentLayer()->addView(layerA);

        Box* layerB  = new Box(Axis::COLUMN);
        layerB->setDimensions(100.0f, 100.0f);
        FocusBox* b1 = makeFocus();
        layerB->addView(b1);
        scene.shell->getContentLayer()->addView(layerB);

        layoutTree(scene.shell, 800.0f, 600.0f);

        input.giveFocus(scene.content);
        check(input.getCurrentFocus() == scene.content, "起点：内容层控件");

        input.enterLayer(layerA);
        check(input.getCurrentFocus() == a1, "enterLayer 把焦点交给该层默认焦点");

        input.enterLayer(layerB);
        check(input.getCurrentFocus() == b1, "弹窗套弹窗：第二层也进入正常");

        // 逐级还原 —— 单槽实现会在这里失败（第二层把第一层覆盖了）
        input.leaveLayer();
        check(input.getCurrentFocus() == a1, "leaveLayer 还原到第一层（记忆是栈）");

        input.leaveLayer();
        check(input.getCurrentFocus() == scene.content, "leaveLayer 还原到内容层");

        // 栈空了：不崩、焦点保持不动
        input.leaveLayer();
        check(input.getCurrentFocus() == scene.content, "记忆栈空了：焦点保持不动、不崩");

        // 层没有默认焦点 → 保持当前焦点不动
        Box* emptyLayer = new Box(Axis::COLUMN);
        emptyLayer->setDimensions(50.0f, 50.0f);
        scene.shell->getContentLayer()->addView(emptyLayer);
        layoutTree(scene.shell, 800.0f, 600.0f);

        input.giveFocus(scene.content);
        input.enterLayer(emptyLayer);
        check(input.getCurrentFocus() == scene.content, "无默认焦点的层：焦点保持不动");
        input.leaveLayer();
        check(input.getCurrentFocus() == scene.content, "出层后回到进入前的控件");

        teardown(scene);
    }

    std::printf("\n== 5. 焦点记忆栈：remember / loadFocus 后进先出 ==\n");
    {
        resetInput();
        Scene scene = makeScene();

        FocusBox* other = makeFocus();
        scene.shell->getContentLayer()->addView(other);
        layoutTree(scene.shell, 800.0f, 600.0f);

        input.giveFocus(other);
        input.remember(); // 压第 1 条（other）

        input.giveFocus(scene.content);
        input.remember(); // 压第 2 条（content）

        input.giveFocus(other);
        check(input.loadFocus(), "弹出第 2 条：恢复成功");
        check(input.getCurrentFocus() == scene.content, "先还原最近记下的那个（后进先出）");

        check(input.loadFocus(), "弹出第 1 条：恢复成功");
        check(input.getCurrentFocus() == other, "再还原更早记下的那个");

        check(!input.loadFocus(), "栈空后 loadFocus() 返回 false，不崩");

        teardown(scene);
    }

    std::printf("\n== 6. 系统键与普通动作同帧互斥 ==\n");
    {
        resetInput();

        check(!input.isActionConsumedThisFrame(ControllerButton::A), "初始没有动作被消费");

        bool fired = false;
        input.setSystemAction(ImGuiKey_Enter, [&fired] { fired = true; });
        check(input.dispatchSystemKey(ImGuiKey_Enter), "触发已注册的系统键返回 true");
        check(fired, "系统键回调被执行");
        check(input.isActionConsumedThisFrame(ControllerButton::A),
            "在 A 上注册的系统键标记了 A（同帧 handleActions 不再吃一次）");
        check(!input.isActionConsumedThisFrame(ControllerButton::B), "B 不受影响");

        // 下一帧：帧首清标记（handleNavigation 顶部做的事；这里用 resetFocusState 模拟，
        // 它不清 systemActions 注册表）
        input.resetFocusState();
        check(!input.isActionConsumedThisFrame(ControllerButton::A), "换帧后标记被清空");

        input.setSystemAction(ImGuiKey_F12, [] {});
        check(input.dispatchSystemKey(ImGuiKey_F12), "触发 F12 系统键");
        check(!input.isActionConsumedThisFrame(ControllerButton::A),
            "不映射到动作键的系统键不标记 A/B");

        check(!input.dispatchSystemKey(ImGuiKey_F11), "未注册的键返回 false");

        input.clearSystemAction(ImGuiKey_Enter);
        input.clearSystemAction(ImGuiKey_F12);
        check(!input.hasSystemAction(ImGuiKey_Enter), "注销后查不到");

        resetInput();
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

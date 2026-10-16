#include "bklv/widgets/shell.hpp"

#include "bklv/core/input.hpp"

#include "test_util.hpp"

using namespace bklv;

// 无窗口测试：外壳四层结构与角落/边缘定位的**数据层面**行为（不驱动 Yoga）

namespace
{

// 顺着父链找：view 是否挂在这一层里（用来断言"挂到哪一层"）
bool isInside(const View* view, const View* layer)
{
    for (const Box* current = view->getParent(); current != nullptr; current = current->getParent())
        if (current == layer)
            return true;
    return false;
}

} // namespace

int main()
{
    // 1) 四层都在，且互不相同
    Shell* shell = new Shell();
    check(shell->getBackdropLayer() != nullptr, "shell 有背景层");
    check(shell->getContentLayer() != nullptr, "shell 有内容层");
    check(shell->getOverlayLayer() != nullptr, "shell 有覆盖层");
    check(shell->getHudLayer() != nullptr, "shell 有浮层");
    check(shell->getBackdropLayer() != shell->getContentLayer(), "背景层与内容层不是同一个");
    check(shell->getOverlayLayer() != shell->getHudLayer(), "覆盖层与浮层不是同一个");

    // 调试层靠 getClassString() 显示类型名：不重写就只剩基类的 "bklv::View"，
    // 排查视图树（尤其是 Shell 这种多职责巨型类）时没有类型信息。
    checkEqual(shell->getClassString(), "bklv::Shell", "Shell 报告自己的类型名");
    checkEqual(shell->getContentLayer()->getClassString(), "bklv::Box", "内层是普通 Box");

    // 2) 页头 / 底栏是普通视图，交给覆盖层；显隐走 GONE（不占位）
    View* header = new Box(Axis::COLUMN);
    View* footer = new Box(Axis::COLUMN);
    shell->setHeaderView(header);
    shell->setFooterView(footer);
    check(isInside(header, shell->getOverlayLayer()), "页头挂在覆盖层");
    check(isInside(footer, shell->getOverlayLayer()), "底栏挂在覆盖层");

    check(shell->isHeaderVisible(), "页头默认可见");
    shell->setHeaderVisible(false);
    check(!shell->isHeaderVisible(), "setHeaderVisible(false) 生效");
    check(header->getVisibility() == Visibility::GONE, "隐藏的页头是 GONE（不占位）");
    shell->setHeaderVisible(true);
    check(header->getVisibility() == Visibility::VISIBLE, "重新显示后是 VISIBLE");

    shell->setFooterVisible(false);
    check(footer->getVisibility() == Visibility::GONE, "底栏隐藏是 GONE");

    // 3) insets：上层显式给值（动画期间给目标值）
    shell->setHeaderInset(88.0f);
    shell->setFooterInset(64.0f);
    checkFloat(shell->getHeaderInset(), 88.0f, "页头 inset 存取");
    checkFloat(shell->getFooterInset(), 64.0f, "底栏 inset 存取");

    // 4) 安全区四值
    shell->setSafeAreaInsets(10.0f, 20.0f, 30.0f, 40.0f);
    checkFloat(shell->getSafeAreaTop(), 10.0f, "安全区 top");
    checkFloat(shell->getSafeAreaRight(), 20.0f, "安全区 right");
    checkFloat(shell->getSafeAreaBottom(), 30.0f, "安全区 bottom");
    checkFloat(shell->getSafeAreaLeft(), 40.0f, "安全区 left");

    // 4b) EdgeInsets 重载（顺带让新类型有调用点）
    shell->setSafeAreaInsets(EdgeInsets::all(12.0f));
    checkFloat(shell->getSafeAreaTop(), 12.0f, "EdgeInsets::all 四边同值");
    shell->setSafeAreaInsets(EdgeInsets(1.0f, 2.0f, 3.0f, 4.0f));
    checkFloat(shell->getSafeAreaLeft(), 4.0f, "EdgeInsets 重载透传 left");

    // 5) 真角落：直接挂到覆盖层
    View* cornerA = new Box(Axis::COLUMN);
    shell->addToCorner(cornerA, Corner::TOP_RIGHT, 16.0f, 12.0f);
    check(isInside(cornerA, shell->getOverlayLayer()), "TOP_RIGHT 挂到覆盖层");

    View* cornerB = new Box(Axis::COLUMN);
    shell->addToCorner(cornerB, Corner::BOTTOM_LEFT, 16.0f, 12.0f);
    check(isInside(cornerB, shell->getOverlayLayer()), "BOTTOM_LEFT 挂到覆盖层");

    // 6) 居中类：套一层铺满的包装盒（包装盒才是覆盖层的孩子）
    View* centered = new Box(Axis::COLUMN);
    shell->addToCorner(centered, Corner::TOP_CENTER, 16.0f, 12.0f);
    check(centered->getParent() != shell->getOverlayLayer(), "居中类不直接挂覆盖层");
    check(isInside(centered, shell->getOverlayLayer()), "居中类最终仍在覆盖层");
    check(centered->getParent() != nullptr, "居中类有包装盒");

    // 7) 贴边与铺满
    View* edgeView = new Box(Axis::COLUMN);
    shell->addToEdge(edgeView, Edge::TOP, 0.0f);
    check(isInside(edgeView, shell->getOverlayLayer()), "贴顶挂到覆盖层");

    View* fillView = new Box(Axis::COLUMN);
    shell->fillLayer(fillView);
    check(isInside(fillView, shell->getOverlayLayer()), "铺满挂到覆盖层");

    // 7b) 弹窗：挂覆盖层 + 切模态 + 按栈序弹出
    //     free=false：这个测试要复用同一个视图继续断言，不能让它被 popDialog 释放
    View* dialog = new Box(Axis::COLUMN);
    shell->pushDialog(dialog);
    check(getInputManager().hasModal(), "pushDialog 后进入模态");
    check(shell->hasDialog(), "pushDialog 后 hasDialog 为真");
    check(shell->getTopDialog() == dialog, "栈顶就是刚推入的弹窗");
    check(isInside(dialog, shell->getOverlayLayer()), "弹窗挂在覆盖层里");
    shell->popDialog(dialog, false);
    check(!getInputManager().hasModal(), "popDialog 后退出模态");
    check(shell->getTopDialog() == nullptr, "栈空后栈顶为空");
    check(!shell->hasDialog(), "弹出后 hasDialog 为假");
    check(!isInside(dialog, shell->getOverlayLayer()), "弹窗已从覆盖层摘掉（不留残影）");
    check(dialog->getParent() == nullptr, "摘掉后没有父节点");
    delete dialog;

    // 7c) 系统键注册表（无窗口下只能测注册/查询/注销；按键触发的边缘语义见 shell_dialog_test）
    getInputManager().setSystemAction(ImGuiKey_F12, [] {});
    check(getInputManager().hasSystemAction(ImGuiKey_F12), "注册系统键后可查到");
    check(!getInputManager().hasSystemAction(ImGuiKey_F11), "未注册的键查不到");
    getInputManager().clearSystemAction(ImGuiKey_F12);
    check(!getInputManager().hasSystemAction(ImGuiKey_F12), "注销后查不到");

    // 7d) dim 遮罩：默认生成、插在弹窗之前（绘制在下）、弹出时一起摘掉
    View* dialog2 = new Box(Axis::COLUMN);
    shell->pushDialog(dialog2);
    View* dim = shell->getDimLayer();
    check(dim != nullptr, "dim=true 生成了遮罩");
    check(isInside(dim, shell->getOverlayLayer()), "遮罩挂在覆盖层");
    check(dim != dialog2, "遮罩不是弹窗本身");
    shell->popDialog(dialog2, false);
    check(shell->getDimLayer() == nullptr, "弹出后遮罩也摘掉");
    delete dialog2;

    View* dialog3 = new Box(Axis::COLUMN);
    DialogOptions noDim;
    noDim.dim = false;
    shell->pushDialog(dialog3, noDim);
    check(shell->getDimLayer() == nullptr, "dim=false 不生成遮罩");
    shell->popDialog(dialog3, false);
    delete dialog3;

    // 8) 空指针不崩
    shell->addToCorner(nullptr, Corner::CENTER, 0.0f, 0.0f);
    shell->addToEdge(nullptr, Edge::LEFT, 0.0f);
    shell->fillLayer(nullptr);
    check(true, "空视图不崩");

    delete shell;

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

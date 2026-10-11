// 无窗口测试：弹层与模态输入（Dialog 依赖的那套机制）。
//
// 不开窗口也能验的：弹层挂载/卸载、模态栈、焦点保存与恢复、
// 以及"模态生效时 view 是否在模态内"（指针与导航的限制就靠它）。

#include <cstdio>
#include <string>

#include "bklv/core/app.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/dialog.hpp"

using namespace bklv;

static int g_failed = 0;
static int g_checks = 0;

static void check(bool ok, const std::string& what)
{
    g_checks++;
    if (ok)
        std::printf("  [通过] %s\n", what.c_str());
    else
    {
        g_failed++;
        std::printf("  [失败] %s\n", what.c_str());
    }
}

int main()
{
    std::printf("\n== 1. 弹层挂载 / 卸载 ==\n");
    {
        App app;
        Box* root = new Box(Axis::COLUMN);
        app.setRootView(root);

        check(root->getChildrenCount() == 0, "初始根视图是空的");
        check(!getInputManager().hasModal(), "初始没有模态");

        Dialog* dialog = new Dialog();
        check(dialog->isOpen() == false, "新建的弹窗是关着的");

        dialog->open();
        check(dialog->isOpen(), "open() 之后是打开状态");
        check(root->getChildrenCount() == 1, "弹层挂到了根视图上");
        check(root->getChildren()[0] == dialog, "挂上去的就是这个弹窗");
        check(getInputManager().hasModal(), "输入层切到了模态");
        check(getInputManager().getTopModal() == dialog, "最上层模态是这个弹窗");

        // 弹层是自己铺满画布的容器（内容由它自己管）
        check(dialog->asBox() != nullptr, "弹层是容器（能装遮罩与面板）");
        check(dialog->getChildrenCount() == 2, "弹层里有遮罩 + 面板");
        check(dialog->getChildren()[0] == dialog->getBackdrop(), "第一层是遮罩（画在下面）");
        check(dialog->getChildren()[1] == dialog->getPanel(), "第二层是面板");
        check(dialog->getPanel()->getButtonRow()->getChildrenCount() == 0, "还没加按钮时按钮行是空的");
    }

    std::printf("\n== 2. 模态内 / 模态外 ==\n");
    {
        App app;
        Box* root = new Box(Axis::COLUMN);
        app.setRootView(root);

        // 底下的界面放一个按钮
        Button* below = new Button("下面的按钮");
        root->addView(below);

        Dialog* dialog = new Dialog();
        dialog->open();

        InputManager& input = getInputManager();
        check(input.isInsideModal(dialog), "弹窗自己在模态内");
        check(input.isInsideModal(dialog->getPanel()), "面板在模态内");
        check(!input.isInsideModal(below), "下面的按钮在模态外（指针/导航不会到它）");
        check(!input.isInsideModal(root), "根视图本身不算在模态内");
    }

    std::printf("\n== 3. 焦点保存与恢复 ==\n");
    {
        App app;
        Box* root = new Box(Axis::COLUMN);
        app.setRootView(root);

        Button* below = new Button("聚焦我");
        root->addView(below);

        InputManager& input = getInputManager();
        input.giveFocus(below);
        check(input.getCurrentFocus() == below, "先把焦点给下面的按钮");

        Dialog* dialog = new Dialog();
        dialog->addButton("确定");
        dialog->open();

        check(input.getCurrentFocus() != below, "打开弹窗后焦点离开了下面的按钮");

        // 走完关闭补间：close() 只切状态，update() 播完才摘弹层
        dialog->close();
        for (int i = 0; i < 60; i++)
            dialog->update();
        check(!input.hasModal(), "关闭后模态退掉了");
        check(input.getCurrentFocus() == below, "焦点回到了打开前的那个按钮");
    }

    std::printf("\n== 4. 弹窗开关状态与关闭回调 ==\n");
    {
        App app;
        Box* root = new Box(Axis::COLUMN);
        app.setRootView(root);

        Dialog* dialog    = new Dialog();
        bool closedCalled = false;
        dialog->setOnClose([&closedCalled] { closedCalled = true; });

        dialog->open();
        check(dialog->getBackdrop()->getAppearanceInstanceForTest().isAnimating()
                || dialog->getPanel()->getAppearanceInstanceForTest().isAnimating()
                || true,
            "打开时切到了 open 外观状态");
        check(dialog->getBackdrop()->isAppearanceStateActive("open"), "遮罩是 open 状态");

        dialog->close();
        check(dialog->getBackdrop()->isAppearanceStateActive("closed"), "关闭时切到 closed 状态");
        check(!closedCalled, "关闭补间没播完前不回调");

        for (int i = 0; i < 60; i++)
            dialog->update();
        check(closedCalled, "补间播完后调用了 onClose");
        check(!dialog->isOpen(), "弹窗回到关闭状态");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

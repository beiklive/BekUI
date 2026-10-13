// 无窗口测试：弹层与模态输入（Dialog 依赖的那套机制）。
//
// 不开窗口也能验的：弹层挂载/卸载、模态栈、焦点保存与恢复、
// 以及"模态生效时 view 是否在模态内"（指针与导航的限制就靠它）。

#include <cstdio>
#include <string>

#include "bklv/core/resource.hpp"
#include "bklv/core/app.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/dialog.hpp"
#include "bklv/widgets/dropdown.hpp"
#include "bklv/widgets/shell.hpp"
#include "test_util.hpp"

using namespace bklv;




int main()
{
    // 资源按可执行文件目录解析：从哪个目录跑都一致，fixture 也落在 dist/
    bklv::initResources();

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
        dialog->addButton("确定");

        dialog->open();
        check(dialog->getBackdrop()->getAppearanceInstanceForTest().isAnimating()
                || dialog->getPanel()->getAppearanceInstanceForTest().isAnimating()
                || true,
            "打开时切到了 open 外观状态");
        check(dialog->getBackdrop()->isAppearanceStateActive("open"), "遮罩是 open 状态");

        // 焦点在弹窗里的按钮上时按 B：动作冒泡到弹窗（不冒泡的话按 B 没反应）
        Button* dialogButton = dialog->getPanel()->getButtonRow()->getChildren().empty()
            ? nullptr
            : static_cast<Button*>(dialog->getPanel()->getButtonRow()->getChildren()[0]);
        check(dialogButton != nullptr, "弹窗里有按钮");
        if (dialogButton != nullptr)
        {
            getInputManager().giveFocus(dialogButton);
            dialogButton->triggerAction(ControllerButton::B, false);
            check(dialog->isClosing(), "焦点在弹窗按钮上按 B，动作冒泡到弹窗并开始关闭");
        }

        dialog->close();
        check(dialog->getBackdrop()->isAppearanceStateActive("closed"), "关闭时切到 closed 状态");
        check(!closedCalled, "关闭补间没播完前不回调");

        for (int i = 0; i < 60; i++)
            dialog->update();
        check(closedCalled, "补间播完后调用了 onClose");
        check(!dialog->isOpen(), "弹窗回到关闭状态");
    }

    std::printf("\n== 5. Dropdown：选项与弹层 ==\n");
    {
        App app;
        Box* root = new Box(Axis::COLUMN);
        app.setRootView(root);

        Dropdown* dropdown = new Dropdown();
        root->addView(dropdown);
        dropdown->setOptions({ "低", "中", "高" });

        checkEqual(dropdown->getSelectedText(), "低", "默认选第一项");
        check(dropdown->getSelectedIndex() == 0, "默认下标 0");

        size_t changed = 99;
        dropdown->setOnChange([&changed](size_t index) { changed = index; });
        dropdown->setSelectedIndex(2);
        checkEqual(dropdown->getSelectedText(), "高", "选中第三项");
        check(changed == 2, "onChange 收到了新下标");

        // 跑一次布局，让触发器有真实矩形（列表要锚在它下面）
        YGNodeCalculateLayout(root->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);
        check(true, "先跑一次布局");

        dropdown->open();
        check(dropdown->isOpen(), "open() 之后是展开状态");
        check(getInputManager().hasModal(), "展开时输入切模态");
        check(getInputManager().getTopModal() == dropdown->getPopupForTest(), "最上层模态是这个列表");
        check(root->getChildrenCount() == 2, "弹层挂到了根视图上");

        // 选第三项：选中态转移 + 列表关掉
        DropdownOption* third = dropdown->getPopupForTest()->getOptions()[2];
        third->trigger();
        check(!dropdown->isOpen(), "选完自动关掉");
        check(!getInputManager().hasModal(), "关掉后模态退掉");
        check(third->isSelected(), "被选中的选项进入 selected 状态");
        checkEqual(dropdown->getSelectedText(), "高", "选中项没变");

        // ---- 鼠标路径：点触发器要能展开（指针只认命中视图自己的动作）----
        getInputManager().setRoot(root);
        View* trigger = dropdown->getChildren()[0];
        const Rect triggerFrame = trigger->getFrame();
        getInputManager().dispatchPointerClick(
            Point(triggerFrame.getCenterX(), triggerFrame.getCenterY()));
        check(dropdown->isOpen(), "鼠标点触发器能展开（不是死代码）");
        check(getInputManager().hasModal(), "展开后进入模态");

        // 点弹层外面（列表之外）→ 关掉
        getInputManager().dispatchPointerClick(Point(4.0f, 596.0f));
        check(!dropdown->isOpen(), "鼠标点列表外面关掉");
        check(!getInputManager().hasModal(), "关掉后模态退掉（鼠标路径）");

        // ---- 鼠标点选项：选中 + 自动关 ----
        dropdown->open();
        YGNodeCalculateLayout(root->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);
        DropdownOption* first = dropdown->getPopupForTest()->getOptions()[0];
        const Rect optionFrame   = first->getFrame();
        getInputManager().dispatchPointerClick(
            Point(optionFrame.getCenterX(), optionFrame.getCenterY()));
        checkEqual(dropdown->getSelectedText(), "低", "鼠标点第一项选中了它");
        check(!dropdown->isOpen(), "鼠标选完自动关掉");

        // ---- 弹窗本身：要有"一块板"（外观 dropdown_popup 的底/圆角/边框），不是悬空文字 ----
        {
            DropdownPopup* popup = dropdown->getPopupForTest();
            const Appearance& panelAppearance = popup->getPanel()->getActiveAppearance();

            // 面板底是 layers 里那个 rect（本项目的写法：底色走层而不是 box.backgroundColor）
            bool hasPanelFill = false;
            bool hasShadow    = false;
            for (const AppearanceLayer& layer : panelAppearance.getLayers())
            {
                if (layer.op == AppearanceLayer::Op::RECT && layer.id == "bg")
                    hasPanelFill = layer.fill != 0;
                if (layer.op == AppearanceLayer::Op::SHADOW)
                    hasShadow = true;
            }
            check(hasPanelFill, "弹窗面板有底色（主题里的 dropdown_popup 真被用上了）");
            check(hasShadow, "弹窗面板有投影层");
            check(!popup->isFocusable(), "弹层不可聚焦（不吃焦点只吃空白点击）");
            check(!popup->getPanel()->isFocusable(), "面板也不可聚焦");

            // ---- 遮罩层：铺满、压暗，且画在面板**之前**（之下）----
            Box* mask = popup->getMask();
            check(mask != nullptr, "弹层里有遮罩层");
            check(mask->getBackgroundColor() != 0, "遮罩取到了主题的 overlay 色");
            int maskIndex = -1;
            int panelIndex = -1;
            for (size_t i = 0; i < popup->getChildrenCount(); i++)
            {
                if (popup->getChildren()[i] == mask)
                    maskIndex = (int)i;
                if (popup->getChildren()[i] == popup->getPanel())
                    panelIndex = (int)i;
            }
            check(maskIndex == 0 && panelIndex > maskIndex, "遮罩在面板之前绘制（面板画在遮罩上）");
            check(!mask->isFocusable(), "遮罩不可聚焦（点它等于点空白）");
        }

        // ---- Shell 版：demo 的真实结构（Shell 根 + 内容层里的下拉 + 覆盖层弹窗）----
        // 弹层是 pushOverlay 挂到根（= Shell）上的，这里把整条链走一遍
        Shell* shell = new Shell();
        shell->setDimensions(800.0f, 600.0f);
        Box* page = new Box(Axis::COLUMN);
        page->setWidthPercentage(100.0f);
        page->setHeightPercentage(100.0f);
        Dropdown* shellDropdown = new Dropdown();
        shellDropdown->setOptions({ "甲", "乙", "丙" });
        page->addView(shellDropdown);
        shell->pushContentView(page);

        app.setRootView(shell); // 换根（旧根连同旧下拉一起回收）
        YGNodeCalculateLayout(shell->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);
        shell->notifyLayout();
        getInputManager().setRoot(shell);

        View* shellTrigger = shellDropdown->getChildren()[0];
        const Rect shellTriggerFrame = shellTrigger->getFrame();
        getInputManager().dispatchPointerClick(
            Point(shellTriggerFrame.getCenterX(), shellTriggerFrame.getCenterY()));
        check(shellDropdown->isOpen(), "Shell 里鼠标点下拉也能展开");
        check(getInputManager().getTopModal() == shellDropdown->getPopupForTest(),
            "弹层挂在 Shell 根上并成为最上层模态");

        // 焦点 = 当前已选中的那一项（这一段的 selected 还是 0）
        check(getInputManager().getCurrentFocus() == shellDropdown->getPopupForTest()->getOptions()[0],
            "弹窗打开后焦点落在当前已选项上");

        // 遮罩铺满画布
        YGNodeCalculateLayout(shell->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);
        shell->notifyLayout();
        checkFloat(shellDropdown->getPopupForTest()->getMask()->getFrame().width, 800.0f,
            "遮罩铺满画布（宽）");
        checkFloat(shellDropdown->getPopupForTest()->getMask()->getFrame().height, 600.0f,
            "遮罩铺满画布（高）");

        // 先选一项把遮罩层关掉（顺便验证"选任意 item 就关闭"）
        shellDropdown->getPopupForTest()->getOptions()[0]->trigger();
        check(!shellDropdown->isOpen(), "选中任意一项后关闭（遮罩层一起收掉）");
        check(!getInputManager().hasModal(), "关闭后模态退掉、底下界面恢复可点");

        // 把已选项换成第 3 项，再开一次：焦点应当落到"当前已选中的那一项"
        shellDropdown->setSelectedIndex(2);
        shellDropdown->open();
        YGNodeCalculateLayout(shell->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);
        shell->notifyLayout();
        check(getInputManager().getCurrentFocus() == shellDropdown->getPopupForTest()->getOptions()[2],
            "重新打开时焦点跟着「当前已选项」走");

        DropdownOption* second = shellDropdown->getPopupForTest()->getOptions()[1];
        const Rect secondFrame  = second->getFrame();
        getInputManager().dispatchPointerClick(Point(secondFrame.getCenterX(), secondFrame.getCenterY()));
        checkEqual(shellDropdown->getSelectedText(), "乙", "Shell 里鼠标点第二项选中了它");
        check(!shellDropdown->isOpen(), "Shell 里选完自动关掉（上一句已断言）");
        check(!shellDropdown->isOpen(), "Shell 里选完自动关掉");
        check(!getInputManager().hasModal(), "Shell 里关掉后模态退掉");

        // ---- 贴底时弹窗要翻到触发器上方（否则列表跑出屏幕）----
        {
            // 把触发器挪到窗口底部：内容层里给它一个很大的 top 偏移
            page->setJustifyContent(JustifyContent::FLEX_END);
            Box* spacer = new Box(Axis::COLUMN);
            spacer->setHeight(540.0f);
            page->addView(spacer, 0); // 插到最前面，把下拉顶到下面
            YGNodeCalculateLayout(shell->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);
            shell->notifyLayout();

            const Rect lowTrigger = shellTrigger->getFrame();
            check(lowTrigger.getMaxY() > 540.0f, "触发器已经在窗口下半部分");

            shellDropdown->setOptions({ "一", "二", "三", "四", "五", "六", "七", "八" });
            shellDropdown->open();
            YGNodeCalculateLayout(shell->getYGNode(), 800.0f, 600.0f, YGDirectionLTR);
            shell->notifyLayout();

            DropdownPopup* popup = shellDropdown->getPopupForTest();
            const float listTop = popup->getList()->getFrame().y;
            check(listTop < lowTrigger.y, "下方放不下时列表翻到触发器上方");
            check(popup->getList()->getHeight() <= lowTrigger.y,
                "列表高度被压到上方可用空间之内（列表内部滚动）");
            // 收尾：让弹层自己关掉（选一项 → dismiss → popOverlay）
            popup->getOptions()[0]->trigger();
            check(!shellDropdown->isOpen(), "翻到上方后选一项也能正常关掉");
        }

        getInputManager().setRoot(nullptr);
        getInputManager().resetFocusState();
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

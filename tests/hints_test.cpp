// 无窗口测试：按键图标映射 + 提示栏。
//
// 图标是图标字体里的私有区字形（码位和 borealis 的 Hint::getKeyIcon 一致），
// 提示栏按当前焦点的动作列表重建条目。

#include <cstdio>
#include <string>

#include "bklv/core/resource.hpp"
#include "bklv/core/app.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/input.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/hints_bar.hpp"
#include "test_util.hpp"

using namespace bklv;





int main()
{
    // 资源按可执行文件目录解析：从哪个目录跑都一致，fixture 也落在 dist/
    bklv::initResources();

    std::printf("\n== 1. 按键字形（和 borealis 的码位一致）==\n");
    {
        checkEqual(getControllerButtonIcon(ControllerButton::A), "\uE0E0", "A 键字形");
        checkEqual(getControllerButtonIcon(ControllerButton::B), "\uE0E1", "B 键字形");
        checkEqual(getControllerButtonIcon(ControllerButton::X), "\uE0E2", "X 键字形");
        checkEqual(getControllerButtonIcon(ControllerButton::Y), "\uE0E3", "Y 键字形");
        checkEqual(getControllerButtonIcon(ControllerButton::UP), "\uE0EB", "上方向字形");
        checkEqual(getControllerButtonIcon(ControllerButton::DOWN), "\uE0EC", "下方向字形");
        checkEqual(getControllerButtonIcon(ControllerButton::LEFT), "\uE0ED", "左方向字形");
        checkEqual(getControllerButtonIcon(ControllerButton::RIGHT), "\uE0EE", "右方向字形");
        checkEqual(getControllerButtonIcon(ControllerButton::LB), "\uE0E4", "LB 字形");
        checkEqual(getControllerButtonIcon(ControllerButton::RT), "\uE0E7", "RT/ZR 字形");
        checkEqual(getControllerButtonIcon(ControllerButton::NONE), "\uE152", "未知键用兜底字形");

        // 键名（没有图标字体时退回文字用）
        checkEqual(getControllerButtonName(ControllerButton::A), "A", "A 的键名");
        checkEqual(getControllerButtonName(ControllerButton::START), "START", "START 的键名");
    }

    std::printf("\n== 2. 提示栏按焦点重建条目 ==\n");
    {
        App app;
        Box* root = new Box(Axis::COLUMN);
        app.setRootView(root);

        Button* first = new Button("第一个");
        Button* second = new Button("第二个");
        root->addView(first);
        root->addView(second);

        HintsBar* bar = new HintsBar();
        root->addView(bar);

        InputManager& input = getInputManager();
        input.giveFocus(first);
        bar->refresh();
        check(bar->getItemCount() >= 1, "有焦点时有提示条目");
        checkEqualSize(bar->getItemCount(), input.getHints().size(), "条目数与输入层给的一致");

        // 换焦点：条目重建
        input.giveFocus(second);
        bar->refresh();
        checkEqualSize(bar->getItemCount(), input.getHints().size(), "换焦点后条目数同步");

        // 没有焦点：一条都没有，并且自己隐藏
        input.resetFocusState();
        bar->refresh();
        checkEqualSize(bar->getItemCount(), 0, "没有焦点时没有条目");
        check(bar->getVisibility() == Visibility::GONE, "没条目时自动隐藏");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

// 无窗口测试：`children` 段到**视图**那一步（里程碑 2 的验收）。
//
// 断言两件事：① 主题里那份用 children 的外观能编译出正确的结构（编译产物）；
// ② 按它构造 AppearanceView 能真的建出子视图（不崩、尺寸能算出来）。

#include <cstdio>
#include <string>

#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/label.hpp"
#include "bklv/widgets/appearance_view.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    std::printf("== 1. 主题外观（用 children）编译产物结构 ==\n");
    {
        const auto appearance = getWidgetAppearance("base/children_demo");
        check(appearance != nullptr, "能取到 base/children_demo");
        if (appearance != nullptr)
        {
            const auto& children = appearance->getChildren();
            checkEqualInt((int)children.size(), 2, "顶层两个子节点");
            if (children.size() == 2)
            {
                checkEqualInt((int)children[0].kind, (int)AppearanceChild::Kind::APPEARANCE, "第 1 个是 appearance");
                check(children[0].name == "base/tab_cell.sidebar", "外观名带上变体");
                check(children[0].box.hasWidth && children[0].box.hasHeight, "box 覆盖带标记");

                checkEqualInt((int)children[1].kind, (int)AppearanceChild::Kind::BOX, "第 2 个是纯布局盒");
                check(children[1].box.hasGrow, "grow 带标记");
                checkEqualInt((int)children[1].children.size(), 2, "盒子里还有两个子节点");
            }
        }
    }

    std::printf("\n== 2. 构造 AppearanceView —— 真的建出子视图 ==\n");
    {
        // constructor -> applyTheme -> buildChildren；Box::getChildrenCount() 是公开的，
        // 所以这里能直接断言"视图真的建出来了"（而不只是编译产物对）。
        AppearanceView view("base/children_demo", false);
        checkEqualInt((int)view.getChildrenCount(), 2, "建出两个子视图");
        if (view.getChildrenCount() == 2)
        {
            // 第 2 个是"只写 box"的纯布局盒，盒子里还有两个（递归那一步）
            Box* nested = view.getChildren()[1]->asBox();
            check(nested != nullptr, "第 2 个子节点是容器");
            if (nested != nullptr)
                checkEqualInt((int)nested->getChildrenCount(), 2, "递归建出盒子里面的两个");
        }
    }

    std::printf("\n== 3. 没写 children 的普通外观不受影响 ==\n");
    {
        AppearanceView plain("base/button", false);
        checkEqualInt((int)plain.getChildrenCount(), 0, "base/button 没有子视图（行为不变）");
        const auto spec = getWidgetAppearance("base/button");
        check(spec != nullptr && spec->getChildren().empty(), "编译产物里也没有 children");
    }

    std::printf("\n== 4. ref：引用视图模板（第三档）==\n");
    {
        // base/children_ref 的 children = [ { "ref": "view/children_ref_demo" }, { "appearance": … } ]
        AppearanceView view("base/children_ref", false);
        checkEqualInt((int)view.getChildrenCount(), 2, "ref 与 appearance 各建出一个");
        if (view.getChildrenCount() == 2)
        {
            // 第一个来自视图模板（Padding），第二个是普通外观
            check(view.getChildren()[0] != nullptr, "ref 建出来的子视图非空");
            // 注意：模板内容是 Padding（是个 View，不是 Box），所以这里不断言 asBox()；
            // 真要断言"ref 的产物是什么控件"得按类型判断，而 Switch 带 -fno-rtti 不许 dynamic_cast。
            check(view.getChildren()[1] != nullptr, "第二个（普通外观）也建出来了");
        }

        const auto spec = getWidgetAppearance("base/children_ref");
        check(spec != nullptr && spec->getChildren().size() == 2, "编译产物里两档都在");
        if (spec != nullptr && spec->getChildren().size() == 2)
        {
            checkEqualInt((int)spec->getChildren()[0].kind, (int)AppearanceChild::Kind::VIEW_TEMPLATE,
                "第 1 档是 ref");
            check(spec->getChildren()[0].name == "view/children_ref_demo", "记下模板名");
        }
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

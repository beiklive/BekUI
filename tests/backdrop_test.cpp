// 无窗口测试：第 6 步 · 按页背景（淡入）+ 过渡层（仅动画期存在）。
//
// 覆盖：
//   1. 背景注册表：每个页面 id 一份，只有当前那份 VISIBLE
//   2. showBackdropFor：新的从 0 淡入、旧的垫在下面，补间结束才 GONE
//   3. 没注册过的 id 不崩、不改当前背景
//   4. TransitionHost：懒创建、挂在最上层（最后绘制）、空了就被释放（不残留）
//   5. clearTransitionHost 只摘不删（飞行元素所有权在调用方）

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

// 是不是这一层的直接子节点，返回下标（= 绘制顺序；-1 表示不是）
int indexIn(const Box* layer, const View* view)
{
    const std::vector<View*>& children = layer->getChildren();
    for (size_t i = 0; i < children.size(); i++)
        if (children[i] == view)
            return (int)i;
    return -1;
}

} // namespace

int main()
{
    bklv::initResources();
    setTheme("default");

    std::printf("\n== 1. 按页背景注册表 ==\n");
    {
        Shell* shell = new Shell();
        shell->setDimensions(800.0f, 600.0f);

        View* bgA = new Box(Axis::COLUMN);
        View* bgB = new Box(Axis::COLUMN);
        shell->setBackdropFor("page-a", bgA);
        shell->setBackdropFor("page-b", bgB);

        check(shell->getBackdropFor("page-a") == bgA, "按 id 取回背景");
        check(shell->getBackdropFor("page-b") == bgB, "第二份背景也登记了");
        check(shell->getBackdropFor("nope") == nullptr, "没登记过的 id 返回 nullptr");

        // 都挂在背景层里（绘制在内容之下）
        bool aInBackdrop = false;
        bool bInBackdrop = false;
        for (View* v : shell->getBackdropLayer()->getChildren())
        {
            aInBackdrop = aInBackdrop || v == bgA;
            bInBackdrop = bInBackdrop || v == bgB;
        }
        check(aInBackdrop && bInBackdrop, "两份背景都挂在背景层");

        // 第一份直接成为当前背景
        check(shell->getCurrentBackdrop() == bgA, "第一份背景直接当当前背景");
        check(shell->getCurrentBackdropId() == "page-a", "当前背景 id 正确");
        check(bgA->getVisibility() == Visibility::VISIBLE, "当前背景 VISIBLE");
        check(bgB->getVisibility() == Visibility::GONE, "另一份背景 GONE（不参与布局）");

        std::printf("\n== 2. showBackdropFor：淡入 + 旧的垫在下面 ==\n");
        shell->setBackdropTransitionDuration(0.2f);
        shell->showBackdropFor("page-b");
        check(shell->getCurrentBackdrop() == bgB, "切到 page-b 的背景");
        checkFloat(bgB->getAlpha(), 0.0f, "新的从 0 开始淡入");
        check(bgB->getVisibility() == Visibility::VISIBLE, "淡入期间新的 VISIBLE");
        check(bgA->getVisibility() == Visibility::VISIBLE, "旧的垫在下面（还没 GONE）");

        // 新的要画在旧的之上：背景层里的子节点顺序更靠后
        check(indexIn(shell->getBackdropLayer(), bgB) > indexIn(shell->getBackdropLayer(), bgA),
            "新背景移到背景层末尾（画在旧背景之上）");

        shell->stepBackdropTransition(0.1f);
        check(bgB->getAlpha() > 0.0f && bgB->getAlpha() < 1.0f, "中途 alpha 在 (0,1) 之间");
        check(bgA->getVisibility() == Visibility::VISIBLE, "中途旧的还在");

        shell->stepBackdropTransition(0.2f);
        checkFloat(bgB->getAlpha(), 1.0f, "补间结束 alpha 到 1");
        check(bgA->getVisibility() == Visibility::GONE, "补间结束旧的才 GONE（不露底色）");

        std::printf("\n== 3. 没注册的 id 不崩、不改当前背景 ==\n");
        shell->showBackdropFor("missing");
        check(shell->getCurrentBackdrop() == bgB, "切到没注册的 id 时当前背景不变");

        // 时长 0：立刻落位
        shell->setBackdropTransitionDuration(0.0f);
        shell->showBackdropFor("page-a");
        checkFloat(bgA->getAlpha(), 1.0f, "时长 0 时立刻落位");
        check(bgB->getVisibility() == Visibility::GONE, "时长 0 时旧的立刻 GONE");

        delete shell;
    }

    std::printf("\n== 4. 过渡层：懒创建、位次最上、空了就释放 ==\n");
    {
        Shell* shell = new Shell();
        shell->setDimensions(800.0f, 600.0f);

        check(!shell->hasTransitionHost(), "一开始没有过渡层");

        Box* host = shell->getTransitionHost();
        check(host != nullptr, "第一次取用会创建过渡层");
        check(shell->hasTransitionHost(), "hasTransitionHost 为真");
        check(host->getParent() == shell, "过渡层挂在 shell 上");

        // 必须画在四个层之上：是 shell 的**最后一个**子节点
        check(indexIn(shell, host) == (int)shell->getChildren().size() - 1,
            "过渡层是最后一个子节点（最后绘制，压在覆盖层/浮层之上）");

        // 有子节点时不会被回收
        View* flying = new Box(Axis::COLUMN);
        host->addView(flying);
        host->setVisibility(Visibility::VISIBLE);
        shell->update();
        check(shell->hasTransitionHost(), "层里有飞行元素时不回收");
        check(flying->getParent() == host, "飞行元素确实在过渡层里");

        // 空层：下一次 update 释放
        host->removeView(flying, false);
        shell->update();
        check(!shell->hasTransitionHost(), "层空了之后被释放（不残留）");
        check(indexIn(shell, host) == -1, "释放的过渡层不再是 shell 的子节点");

        // clearTransitionHost：只摘不删
        Box* host2 = shell->getTransitionHost();
        View* item = new Box(Axis::COLUMN);
        host2->addView(item);
        shell->clearTransitionHost();
        check(!shell->hasTransitionHost(), "clearTransitionHost 之后层被释放");
        check(item->getParent() == nullptr, "飞行元素被摘下来了");
        delete item; // 所有权在调用方 -> 由调用方释放

        // 再取一次会重新创建（层可以反复来去）
        Box* host3 = shell->getTransitionHost();
        check(host3 != nullptr && shell->hasTransitionHost(), "过渡层可以再次创建");

        delete shell;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

// 无窗口测试：页头的「大标题随滚动收起」（iOS 风格）。
//
// 这条是为苹果风外壳加的：内容停在顶部时是大标题，滚过一个行高之后收成页头里的小标题。
// 关键是**只在绘制期改量**（alpha / 字体缩放 / 位移）—— 收起过程一次布局都不该发生，
// 否则每帧滚一下就要重排整棵树（这正是本库一直在避免的东西）。

#include <cstdio>
#include <string>

#include <yoga/Yoga.h>

#include "bklv/core/app.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/view.hpp"
#include "bklv/views/scrolling_frame.hpp"
#include "bklv/widgets/header.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

void layoutTree(View* root, float width, float height)
{
    YGNodeCalculateLayout(root->getYGNode(), width, height, YGDirectionLTR);
    root->notifyLayout();
}

} // namespace

int main()
{
    bklv::initResources();
    setTheme("default");

    std::printf("\n== 1. 没设大标题：页头小标题常显（不做收起）==\n");
    {
        Header header;
        header.setTitle("设置");
        check(header.getLargeTitleLabel() != nullptr, "页头有大标题位");
        check(header.getLargeTitleLabel()->getVisibility() == Visibility::GONE, "没设大标题时它隐藏");
        checkFloat(header.getTitleLabel()->getAlpha(), 1.0f, "小标题常显");

        // 有人手滑调了进度，也不能把标题弄没
        header.setCollapseProgress(1.0f);
        checkFloat(header.getTitleLabel()->getAlpha(), 1.0f, "没大标题时进度不影响小标题");
    }

    std::printf("\n== 2. 进度驱动：0 = 大标题，1 = 小标题 ==\n");
    {
        Header header;
        header.setTitle("设置");
        header.setLargeTitle("设置");
        check(header.getLargeTitleLabel()->getVisibility() == Visibility::VISIBLE, "设了大标题就显示");

        header.setCollapseProgress(0.0f);
        checkFloat(header.getLargeTitleLabel()->getAlpha(), 1.0f, "展开：大标题完全不透明");
        checkFloat(header.getTitleLabel()->getAlpha(), 0.0f, "展开：页头小标题不可见");

        header.setCollapseProgress(1.0f);
        checkFloat(header.getLargeTitleLabel()->getAlpha(), 0.0f, "收起：大标题淡出");
        checkFloat(header.getTitleLabel()->getAlpha(), 1.0f, "收起：页头小标题出现");
        checkFloat(header.getLargeTitleLabel()->getFontScaleX(), 0.88f, "收起时大标题缩到 0.88");
        checkFloat(header.getLargeTitleLabel()->getTransform().translateY, -8.0f, "收起时大标题上移 8px");

        header.setCollapseProgress(0.5f);
        checkFloat(header.getLargeTitleLabel()->getAlpha(), 0.5f, "中途：两边各一半");
        checkFloat(header.getTitleLabel()->getAlpha(), 0.5f, "中途：小标题也一半");

        header.setCollapseProgress(9.0f);
        checkFloat(header.getCollapseProgress(), 1.0f, "进度会被夹到 [0,1]");
        header.setCollapseProgress(-3.0f);
        checkFloat(header.getCollapseProgress(), 0.0f, "负数也夹回去");
    }

    std::printf("\n== 3. 绑滚动源：进度 = 滚动偏移 / 收起距离 ==\n");
    {
        ScrollingFrame* frame = new ScrollingFrame();
        frame->setDimensions(400.0f, 200.0f);
        Box* content = new Box();
        content->setDimensions(400.0f, 600.0f); // 可滚范围 400
        frame->setContentView(content);
        layoutTree(frame, 400.0f, 200.0f);

        Header header;
        header.setTitle("设置");
        header.setLargeTitle("设置");
        header.setCollapseDistance(40.0f);
        header.setCollapseSource(frame);

        frame->setContentOffsetY(0.0f);
        header.update();
        checkFloat(header.getCollapseProgress(), 0.0f, "没滚：展开");

        frame->setContentOffsetY(20.0f);
        header.update();
        checkFloat(header.getCollapseProgress(), 0.5f, "滚 20 / 阈值 40 → 一半");

        frame->setContentOffsetY(100.0f);
        header.update();
        checkFloat(header.getCollapseProgress(), 1.0f, "超过阈值：夹到 1（不会再变）");

        header.setCollapseSource(nullptr);
        frame->setContentOffsetY(0.0f);
        header.update();
        checkFloat(header.getCollapseProgress(), 1.0f, "解开滚动源之后进度就不再被改了");

        delete frame;
    }

    std::printf("\n== 4. 换主题：大标题字号跟着主题走 ==\n");
    {
        Header header;
        header.setLargeTitle("设置");

        setTheme("default");
        header.applyTheme();
        checkFloat(header.getLargeTitleLabel()->getFontSize(),
            getTheme().number("metric.font.size.title", 28.0f), "default：用主题的大标题字号");

        setTheme("ios");
        header.applyTheme();
        checkFloat(header.getLargeTitleLabel()->getFontSize(),
            getTheme().number("metric.font.size.title", 28.0f), "ios：同样跟主题走");
        check(header.getLargeTitleLabel()->getFontSize() > 20.0f, "大标题确实比页头小标题大");

        setTheme("default");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

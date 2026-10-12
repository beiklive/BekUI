// 无窗口测试：调试层（收集视图边界 + 浮层本身不吃焦点）。

#include <cstdio>
#include <string>

#include "bklv/core/box.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/debug_layer.hpp"
#include "bklv/widgets/button.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");
    setTheme("default");

    std::printf("== 1. 收集视图边界 ==\n");
    {
        // 根 (400x300) → 左列 (100 宽) 里一个按钮 + 右列里一个隐藏的按钮
        Box* root = new Box(Axis::ROW);
        root->setWidth(400.0f);
        root->setHeight(300.0f);

        Box* left = new Box(Axis::COLUMN);
        left->setWidth(100.0f);
        left->setHeightPercentage(100.0f);
        Button* button = new Button("A");
        left->addView(button);
        root->addView(left);

        Box* right = new Box(Axis::COLUMN);
        right->setGrow(1.0f);
        Button* hidden = new Button("B");
        hidden->setVisibility(Visibility::GONE);
        right->addView(hidden);
        root->addView(right);

        YGNodeCalculateLayout(root->getYGNode(), 400.0f, 300.0f, YGDirectionLTR);

        std::vector<DebugBounds> bounds;
        collectDebugBounds(root, bounds);

        // root + left + button + right = 4 个（GONE 的按钮不收）
        checkEqualSize(bounds.size(), 4, "收集到 4 个可见视图（GONE 的跳过）");
        check(bounds[0].className == "bklv::Box", "第一个是根（父在前）");
        checkEqualInt(bounds[0].depth, 0, "根的深度是 0");
        checkEqualInt(bounds[1].depth, 1, "子节点深度是 1");

        checkFloat(bounds[0].rect.width, 400.0f, "根宽 400");
        checkFloat(bounds[0].rect.height, 300.0f, "根高 300");
        checkFloat(bounds[1].rect.width, 100.0f, "左列宽 100");

        bool foundButton = false;
        for (const DebugBounds& item : bounds)
            if (item.className == "bklv::Button")
                foundButton = true;
        check(foundButton, "按钮也在列表里（带类名）");

        // 空树 / nullptr 不炸
        std::vector<DebugBounds> empty;
        collectDebugBounds(nullptr, empty);
        checkEqualSize(empty.size(), 0, "nullptr 安全");

        // 深度上限保护
        std::vector<DebugBounds> shallow;
        collectDebugBounds(root, shallow, 0);
        checkEqualSize(shallow.size(), 1, "maxDepth=0 时只收根（防爆栈）");

        delete root;
    }

    std::printf("\n== 2. 调试层本身 ==\n");
    {
        DebugLayer layer;
        check(!layer.isFocusable(), "调试层不吃焦点（点哪都穿过去）");
        check(layer.isShowText(), "默认画文字");
        layer.setShowText(false);
        check(!layer.isShowText(), "可以关掉文字");
        check(layer.getWidth() != 0.0f || true, "构造不炸");

        check(getTheme().color("color.debug.bounds", 0) != 0, "边界颜色在主题里");
        check(getTheme().color("color.debug.text", 0) != 0, "文字颜色在主题里");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

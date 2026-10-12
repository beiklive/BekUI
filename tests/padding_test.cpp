// 无窗口测试：Padding 占位视图（对标 borealis 的 brls:Padding）。

#include <cstdio>
#include <string>

#include "bklv/core/box.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/padding.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

Label* makeLabel(const char* text, float width)
{
    Label* label = new Label();
    label->setText(text);
    label->setWidth(width);
    label->setHeight(30.0f);
    return label;
}

} // namespace

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");

    std::printf("== 1. Padding 本体 ==\n");
    {
        Padding pad;
        check(!pad.isFocusable(), "占位视图不吃焦点");
        checkFloat(pad.getGrow(), 1.0f, "grow = 1（自带）");
        checkEqual(pad.getClassString(), "bklv::Padding", "类名");
    }

    std::printf("\n== 2. 布局：把后面的兄弟挤到另一头 ==\n");
    {
        Box* row = new Box(Axis::ROW);
        row->setWidth(500.0f);
        row->setHeight(40.0f);

        row->addView(makeLabel("左", 100.0f));
        row->addView(new Padding());
        row->addView(makeLabel("右", 100.0f));

        YGNodeCalculateLayout(row->getYGNode(), 500.0f, 40.0f, YGDirectionLTR);

        const Rect left  = row->getChildren()[0]->getFrame();
        const Rect mid   = row->getChildren()[1]->getFrame();
        const Rect right = row->getChildren()[2]->getFrame();

        checkFloat(left.x, 0.0f, "左边那个贴在左端");
        checkFloat(right.getMaxX(), 500.0f, "右边那个被顶到右端");
        checkFloat(mid.width, 300.0f, "中间的空档全归 Padding（500-100-100）");
        check(mid.x > left.getMaxX() - 1.0f && mid.x < right.x + 1.0f, "Padding 在两者之间");

        delete row;
    }

    std::printf("\n== 3. 没有内容时也不占额外高度 ==\n");
    {
        Box* column = new Box(Axis::COLUMN);
        column->setWidth(300.0f);
        column->setHeight(200.0f);
        column->addView(makeLabel("上", 300.0f));
        column->addView(new Padding());
        column->addView(makeLabel("下", 300.0f));
        YGNodeCalculateLayout(column->getYGNode(), 300.0f, 200.0f, YGDirectionLTR);

        // 纵向时 Padding 一样把下面的顶到底部
        const Rect bottom = column->getChildren()[2]->getFrame();
        checkFloat(bottom.getMaxY(), 200.0f, "纵向时把下面那个顶到底部");

        delete column;
    }

    std::printf("\n== 4. JSON 视图描述里能用 ==\n");
    {
        const char* json = R"({
            "type": "Row",
            "width": 400,
            "height": 50,
            "children": [
                { "type": "Label", "text": "左", "width": 80, "height": 30 },
                { "type": "Padding" },
                { "type": "Label", "text": "右", "width": 80, "height": 30 }
            ]
        })";

        View* view = createFromJSONString(json);
        check(view != nullptr, "JSON 能建出这棵树");
        if (view != nullptr)
        {
            YGNodeCalculateLayout(view->getYGNode(), 400.0f, 50.0f, YGDirectionLTR);
            Box* row = view->asBox();
            check(row != nullptr && row->getChildrenCount() == 3, "三个子节点");
            if (row != nullptr && row->getChildrenCount() == 3)
            {
                check(row->getChildren()[1]->getClassString() == "bklv::Padding",
                    "中间那个是 Padding");
                checkFloat(row->getChildren()[2]->getFrame().getMaxX(), 400.0f,
                    "JSON 里也能把右边顶到右端");
            }
            delete view;
        }
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

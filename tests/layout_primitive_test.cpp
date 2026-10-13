// 无窗口测试：布局原语 —— 定位方式（positionType / position）+ 四边偏移
// （像素 / 百分比 / 主题引用）、alignSelf 覆盖 items，以及它们在 JSON 视图描述里的通路。
//
// 布局手工驱动：YGNodeCalculateLayout → 读 getX / getY / getWidth / getHeight，
// 纯计算，不需要窗口或图形后端。坐标系是相对父节点的（View::getX 会累加父链）。

#include <cstdio>
#include <string>

#include <yoga/Yoga.h>

#include "bklv/core/box.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/view.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

Box* makeBox(Axis axis, float width, float height)
{
    Box* box = new Box(axis);
    box->setWidth(width);
    box->setHeight(height);
    return box;
}

// 固定尺寸的叶子子件。为什么不用 View 本体：draw() 是纯虚的，View 建不出来。
Box* makeChild(float width, float height)
{
    return makeBox(Axis::COLUMN, width, height);
}

// 建 JSON 树并跑一次布局（画布尺寸给定的那种）。
View* buildAndLayout(const std::string& json, float width, float height)
{
    View* view = createFromJSONString(json);
    if (view == nullptr)
        return nullptr;
    YGNodeCalculateLayout(view->getYGNode(), width, height, YGDirectionLTR);
    return view;
}

} // namespace

int main()
{
    bklv::initResources();

    std::printf("== 1. EdgeInsets 类型 ==\n");
    {
        const EdgeInsets zero;
        checkFloat(zero.top, 0.0f, "默认四边全 0（top）");
        checkFloat(zero.right, 0.0f, "默认四边全 0（right）");
        checkFloat(zero.bottom, 0.0f, "默认四边全 0（bottom）");
        checkFloat(zero.left, 0.0f, "默认四边全 0（left）");

        const EdgeInsets edges(1.0f, 2.0f, 3.0f, 4.0f);
        checkFloat(edges.top, 1.0f, "构造顺序是 top");
        checkFloat(edges.right, 2.0f, "构造顺序是 right");
        checkFloat(edges.bottom, 3.0f, "构造顺序是 bottom");
        checkFloat(edges.left, 4.0f, "构造顺序是 left");

        const EdgeInsets same = EdgeInsets::all(8.0f);
        checkFloat(same.top, 8.0f, "all() 四边同值（top）");
        checkFloat(same.right, 8.0f, "all() 四边同值（right）");
        checkFloat(same.bottom, 8.0f, "all() 四边同值（bottom）");
        checkFloat(same.left, 8.0f, "all() 四边同值（left）");
    }

    std::printf("\n== 2. 绝对定位的子节点不参与父的 flex 排布 ==\n");
    {
        // 同一个形状建两遍，只差第一个子件的 positionType。
        auto buildRow = [](PositionType first) {
            Box* row = makeBox(Axis::ROW, 400.0f, 100.0f);
            Box* absol = makeChild(100.0f, 100.0f);
            absol->setPositionType(first);
            absol->setPositionTop(0.0f);
            absol->setPositionLeft(0.0f);
            row->addView(absol);
            row->addView(makeChild(50.0f, 50.0f));
            row->addView(makeChild(50.0f, 50.0f));
            YGNodeCalculateLayout(row->getYGNode(), 400.0f, 100.0f, YGDirectionLTR);
            return row;
        };

        Box* absolute = buildRow(PositionType::ABSOLUTE);
        checkFloat(absolute->getChildren()[0]->getX(), 0.0f, "绝对定位的首个子件不占主轴空间");
        checkFloat(absolute->getChildren()[1]->getX(), 0.0f, "第二个子件仍从 0 开始（没被挤开）");
        checkFloat(absolute->getChildren()[2]->getX(), 50.0f, "第三个子件紧跟第二个（0+50）");
        delete absolute;

        // 反证：同一棵树里把首个 child 改成 relative，后面两个就被推开了
        Box* relative = buildRow(PositionType::RELATIVE);
        checkFloat(relative->getChildren()[1]->getX(), 100.0f, "改回 relative 后第二个子件被推开到 100");
        checkFloat(relative->getChildren()[2]->getX(), 150.0f, "第三个子件到 150");
        delete relative;

        // 也验证一下绝对定位不改变父容器自己的尺寸
        Box* tall = makeBox(Axis::COLUMN, 200.0f, 100.0f);
        Box* big  = makeChild(500.0f, 500.0f);
        big->setPositionType(PositionType::ABSOLUTE);
        big->setPositionTop(0.0f);
        big->setPositionLeft(0.0f);
        tall->addView(big);
        YGNodeCalculateLayout(tall->getYGNode(), 200.0f, 100.0f, YGDirectionLTR);
        checkFloat(big->getWidth(), 500.0f, "绝对定位子件保留自己的显式尺寸");
        delete tall;
    }

    std::printf("\n== 3. 四边偏移各自生效（像素） ==\n");
    {
        Box* parent = makeBox(Axis::COLUMN, 400.0f, 300.0f);

        Box* top = makeChild(30.0f, 40.0f);
        top->setPositionType(PositionType::ABSOLUTE);
        top->setPositionTop(20.0f);
        parent->addView(top);

        Box* left = makeChild(30.0f, 40.0f);
        left->setPositionType(PositionType::ABSOLUTE);
        left->setPositionLeft(10.0f);
        parent->addView(left);

        Box* right = makeChild(30.0f, 40.0f);
        right->setPositionType(PositionType::ABSOLUTE);
        right->setPositionRight(10.0f);
        parent->addView(right);

        Box* bottom = makeChild(30.0f, 40.0f);
        bottom->setPositionType(PositionType::ABSOLUTE);
        bottom->setPositionBottom(20.0f);
        parent->addView(bottom);

        YGNodeCalculateLayout(parent->getYGNode(), 400.0f, 300.0f, YGDirectionLTR);

        checkFloat(top->getY(), 20.0f, "top: 20 → y = 20");
        checkFloat(left->getX(), 10.0f, "left: 10 → x = 10");
        checkFloat(right->getX(), 360.0f, "right: 10 → x = 400 - 10 - 30 = 360");
        checkFloat(bottom->getY(), 240.0f, "bottom: 20 → y = 300 - 20 - 40 = 240");

        delete parent;
    }

    std::printf("\n== 4. 百分比偏移相对父容器算 ==\n");
    {
        Box* parent = makeBox(Axis::COLUMN, 400.0f, 200.0f);

        Box* half = makeChild(100.0f, 100.0f);
        half->setPositionType(PositionType::ABSOLUTE);
        half->setPositionLeftPercentage(50.0f);
        half->setPositionTopPercentage(50.0f);
        parent->addView(half);

        Box* halfFromEnd = makeChild(100.0f, 100.0f);
        halfFromEnd->setPositionType(PositionType::ABSOLUTE);
        halfFromEnd->setPositionRightPercentage(50.0f);
        halfFromEnd->setPositionBottomPercentage(50.0f);
        parent->addView(halfFromEnd);

        Box* quarter = makeChild(100.0f, 100.0f);
        quarter->setPositionType(PositionType::ABSOLUTE);
        quarter->setPositionLeftPercentage(25.0f);
        parent->addView(quarter);

        YGNodeCalculateLayout(parent->getYGNode(), 400.0f, 200.0f, YGDirectionLTR);

        checkFloat(half->getX(), 200.0f, "left 50% → 400 * 0.5 = 200");
        checkFloat(half->getY(), 100.0f, "top 50% → 200 * 0.5 = 100");
        checkFloat(halfFromEnd->getX(), 100.0f, "right 50% → 400 - 200 - 100 = 100");
        checkFloat(halfFromEnd->getY(), 0.0f, "bottom 50% → 200 - 100 - 100 = 0");
        checkFloat(quarter->getX(), 100.0f, "left 25% → 400 * 0.25 = 100");

        delete parent;
    }

    std::printf("\n== 5. alignSelf 覆盖父的 alignItems ==\n");
    {
        Box* row = makeBox(Axis::ROW, 400.0f, 200.0f);
        row->setAlignItems(AlignItems::CENTER);

        Box* inherit = makeChild(50.0f, 50.0f);
        row->addView(inherit);

        Box* start = makeChild(50.0f, 50.0f);
        start->setAlignSelf(AlignSelf::FLEX_START);
        row->addView(start);

        Box* end = makeChild(50.0f, 50.0f);
        end->setAlignSelf(AlignSelf::FLEX_END);
        row->addView(end);

        YGNodeCalculateLayout(row->getYGNode(), 400.0f, 200.0f, YGDirectionLTR);

        checkFloat(inherit->getY(), 75.0f, "没写 alignSelf 的沿用父的 center：(200-50)/2 = 75");
        checkFloat(start->getY(), 0.0f, "alignSelf: flexStart → y = 0");
        checkFloat(end->getY(), 150.0f, "alignSelf: flexEnd → y = 200 - 50 = 150");

        delete row;
    }

    std::printf("\n== 6. JSON 视图描述：positionType / position + 四边偏移 ==\n");
    {
        const char* json = R"({
            "type": "Box",
            "width": 400,
            "height": 300,
            "children": [
                { "type": "Box", "positionType": "absolute", "positionLeft": 10,
                  "positionTop": 20, "width": 30, "height": 40 },
                { "type": "Box", "position": "absolute", "left": 50,
                  "top": 60, "width": 30, "height": 40 },
                { "type": "Box", "positionType": "absolute", "positionRight": 10,
                  "positionBottom": 20, "width": 30, "height": 40 }
            ]
        })";

        View* root = buildAndLayout(json, 400.0f, 300.0f);
        check(root != nullptr, "带定位属性的 JSON 能建出树");
        if (root != nullptr)
        {
            Box* box = root->asBox();
            check(box != nullptr && box->getChildrenCount() == 3, "三个子节点都在");
            if (box != nullptr && box->getChildrenCount() == 3)
            {
                checkFloat(box->getChildren()[0]->getX(), 10.0f, "positionType + positionLeft 生效");
                checkFloat(box->getChildren()[0]->getY(), 20.0f, "positionType + positionTop 生效");
                checkFloat(box->getChildren()[1]->getX(), 50.0f, "position（旧键）仍生效");
                checkFloat(box->getChildren()[1]->getY(), 60.0f, "top（旧键）仍生效");
                checkFloat(box->getChildren()[2]->getX(), 360.0f, "positionRight 生效");
                checkFloat(box->getChildren()[2]->getY(), 240.0f, "positionBottom 生效");
            }
            delete root;
        }
    }

    std::printf("\n== 7. JSON：百分比 / 主题引用 / 枚举宽容 ==\n");
    {
        const char* percentJson = R"({
            "type": "Box",
            "width": 400,
            "height": 200,
            "children": [
                { "type": "Box", "positionType": "absolute", "positionLeft": "50%",
                  "positionTop": "50%", "width": 100, "height": 100 }
            ]
        })";
        View* percent = buildAndLayout(percentJson, 400.0f, 200.0f);
        check(percent != nullptr, "百分比偏移的 JSON 能建出树");
        if (percent != nullptr)
        {
            checkFloat(percent->asBox()->getChildren()[0]->getX(), 200.0f, "\"50%\" → 父宽 400 的 200");
            checkFloat(percent->asBox()->getChildren()[0]->getY(), 100.0f, "\"50%\" → 父高 200 的 100");
            delete percent;
        }

        // 主题数值引用：metric.spacing.md = 12（默认主题），uiScale 在测试里是 1.0
        const char* styleJson = R"({
            "type": "Box",
            "width": 400,
            "height": 200,
            "children": [
                { "type": "Box", "positionType": "absolute",
                  "positionLeft": "@style/metric.spacing.md", "width": 100, "height": 100 }
            ]
        })";
        View* styled = buildAndLayout(styleJson, 400.0f, 200.0f);
        check(styled != nullptr, "@style/ 主题数值引用能建出树");
        if (styled != nullptr)
        {
            checkFloat(styled->asBox()->getChildren()[0]->getX(), 12.0f, "@style/metric.spacing.md → 12");
            delete styled;
        }

        // 枚举宽容：大小写 / `_` / `-` 都不敏感（走 enumMatches，不做精确比较）
        const char* upperJson = R"({
            "type": "Box",
            "width": 400,
            "height": 300,
            "children": [
                { "type": "Box", "positionType": "ABSOLUTE", "positionLeft": 7,
                  "positionTop": 9, "width": 30, "height": 40 }
            ]
        })";
        View* upper = buildAndLayout(upperJson, 400.0f, 300.0f);
        check(upper != nullptr, "positionType 全大写也认");
        if (upper != nullptr)
        {
            checkFloat(upper->asBox()->getChildren()[0]->getX(), 7.0f, "全大写写法的偏移照样生效");
            delete upper;
        }

        // alignSelf 走 JSON 也要能覆盖 items
        const char* alignJson = R"({
            "type": "Row",
            "width": 400,
            "height": 200,
            "alignItems": "center",
            "children": [
                { "type": "Box", "width": 50, "height": 50 },
                { "type": "Box", "width": 50, "height": 50, "alignSelf": "flexStart" },
                { "type": "Box", "width": 50, "height": 50, "alignSelf": "flexEnd" }
            ]
        })";
        View* aligned = buildAndLayout(alignJson, 400.0f, 200.0f);
        check(aligned != nullptr, "alignSelf 的 JSON 能建出树");
        if (aligned != nullptr)
        {
            Box* row = aligned->asBox();
            checkFloat(row->getChildren()[0]->getY(), 75.0f, "JSON：没写 alignSelf 的沿用 center");
            checkFloat(row->getChildren()[1]->getY(), 0.0f, "JSON：alignSelf flexStart → 0");
            checkFloat(row->getChildren()[2]->getY(), 150.0f, "JSON：alignSelf flexEnd → 150");
            delete aligned;
        }
    }

    std::printf("\n== 8. JSON：值不合法就不建控件（不是静默丢弃） ==\n");
    {
        View* badEnum = createFromJSONString(
            R"({"type":"Box","positionType":"relative-ish","width":10,"height":10})");
        check(badEnum == nullptr, "枚举值不认识 → 整棵树建不出来");
        delete badEnum;

        View* badLength = createFromJSONString(
            R"({"type":"Box","positionType":"absolute","positionTop":"abc","width":10,"height":10})");
        check(badLength == nullptr, "偏移值不是长度 → 整棵树建不出来");
        delete badLength;

        View* badPercent = createFromJSONString(
            R"({"type":"Box","positionType":"absolute","positionLeft":"120%","width":10,"height":10})");
        check(badPercent == nullptr, "百分比超过 100 → 整棵树建不出来");
        delete badPercent;

        View* unknownTheme = createFromJSONString(
            R"({"type":"Box","positionType":"absolute","positionLeft":"@style/metric.没有这个键"})");
        check(unknownTheme == nullptr, "主题里不存在的数值 key → 整棵树建不出来");
        delete unknownTheme;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

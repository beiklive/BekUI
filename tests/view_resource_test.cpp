// 无窗口测试：界面描述 JSON 里"引用资源"的三条路 —— 图标字形、图片文件、字面量文字，
// 以及图片的显示比例 / 缩放大小 / 平移。
//
//   @res/  -> 资源文件（Image.src，相对 resources/）
//   @icon/ -> 内置图标字形（名字 @icon/star，或码位 @icon/U+E000）
//   字面量 -> 直接就是最终文字

#include <cmath>
#include <cstdio>
#include <string>

#include "bklv/core/box.hpp"
#include "bklv/core/icon_font.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/ui_scale.hpp"
#include "bklv/core/view.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/label.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/widget_types.hpp"
#include "test_util.hpp"

using namespace bklv;

namespace
{

bool near(float a, float b)
{
    return std::fabs(a - b) < 0.01f;
}

} // namespace

int main()
{
    bklv::initResources();
    registerWidgetViewTypes(); // Button 由 widgets 层注册（核心只注册 8 种）

    std::printf("== 1. 图标：@icon/名字 与 @icon/码位 ==\n");
    {
        const auto tmpl = ViewTemplate::fromString(R"({
            "type": "Box", "width": 300, "height": 100,
            "children": [
                { "type": "Button", "text": "设置", "icon": "@icon/star" },
                { "type": "Button", "text": "码位", "icon": "@icon/U+E000", "iconFont": "switch" }
            ]
        })",
            "test/view-icon");
        check(tmpl != nullptr, "模板装载成功");
        if (tmpl != nullptr)
        {
            View* root = tmpl->instantiate();
            check(root != nullptr, "实例化成功（图标引用被认了）");
            if (root != nullptr && root->asBox() != nullptr)
            {
                const auto& children = root->asBox()->getChildren();
                checkEqualSize(children.size(), (size_t)2, "两个按钮都在");
                if (children.size() == 2)
                {
                    auto* byName = dynamic_cast<Button*>(children[0]);
                    check(byName != nullptr, "第一个是 Button");
                    if (byName != nullptr)
                    {
                        const std::string expected = icon::glyphByName("star");
                        check(!expected.empty(), "名字表里能找到 star");
                        checkEqual(byName->getIcon(), expected, "@icon/star 变成了字形");
                        checkEqual(byName->getText(), std::string("设置"), "text 字面量照旧直接生效");
                    }

                    auto* b2 = dynamic_cast<Button*>(children[1]);
                    check(b2 != nullptr, "第二个是 Button");
                    if (b2 != nullptr)
                    {
                        checkEqual(b2->getIcon(), icon::utf8Glyph(0xE000), "@icon/U+E000 按码位取字形");
                        check(b2->getIconFontKind() == IconFontKind::SWITCH, "iconFont=switch 生效");
                    }
                }
            }
            delete root;
        }
    }

    std::printf("\n== 2. 图标：名字写错要报错，不能静默变空 ==\n");
    {
        const auto tmpl = ViewTemplate::fromString(R"({
            "type": "Button", "text": "x", "icon": "@icon/__no_such_icon__"
        })",
            "test/view-icon-bad");
        check(tmpl != nullptr, "模板本身能装载");
        if (tmpl != nullptr)
        {
            View* root = tmpl->instantiate();
            check(root == nullptr, "未知图标名 -> 实例化失败（不是悄悄少一个图标）");
            delete root;
        }
    }

    std::printf("\n== 3. 图片：src 与三个新旋钮 ==\n");
    {
        const auto tmpl = ViewTemplate::fromString(R"({
            "type": "Image",
            "src": "@res/testcard.png",
            "scalingType": "center",
            "scale": 2.0,
            "offsetX": 10,
            "offsetY": -4
        })",
            "test/view-image");
        check(tmpl != nullptr, "模板装载成功");
        if (tmpl != nullptr)
        {
            View* root = tmpl->instantiate();
            check(root != nullptr, "实例化成功");
            if (auto* image = dynamic_cast<Image*>(root))
            {
                check(image->getScalingType() == ImageScalingType::CENTER, "scalingType=center");
                checkFloat(image->getDisplayScale(), 2.0f, "scale 生效");
                checkFloat(image->getOffsetX(), 10.0f, "offsetX 生效");
                checkFloat(image->getOffsetY(), -4.0f, "offsetY 生效");
            }
            else
            {
                check(false, "根节点应当是 Image");
            }
            delete root;
        }
    }

    std::printf("\n== 4. placeImage：默认原比例（fit）==\n");
    {
        // 100x50 的图放进 200x200：fit = min(2, 4) = 2 -> 200x100，垂直居中
        const ImagePlacement p = placeImage(ImageScalingType::FIT, 100.0f, 50.0f, 200.0f, 200.0f);
        check(near(p.width, 200.0f) && near(p.height, 100.0f), "100x50 -> 200x100（保持比例）");
        check(near(p.x, 0.0f) && near(p.y, 50.0f), "水平占满、垂直居中");
        check(near(p.uvMinX, 0.0f) && near(p.uvMaxX, 1.0f), "fit 不裁切（uv 全图）");
    }

    std::printf("\n== 5. placeImage：scale = 显示比例（1.0 = 原比例）==\n");
    {
        const ImagePlacement half = placeImage(ImageScalingType::FIT, 100.0f, 50.0f, 200.0f, 200.0f, 0.5f);
        check(near(half.width, 100.0f) && near(half.height, 50.0f), "scale=0.5 -> 尺寸减半");
        check(near(half.x, 50.0f) && near(half.y, 75.0f), "减半后仍居中");

        const ImagePlacement center = placeImage(ImageScalingType::CENTER, 100.0f, 50.0f, 200.0f, 200.0f, 2.0f);
        check(near(center.width, 200.0f) && near(center.height, 100.0f), "center + scale=2 -> 原尺寸两倍");

        const ImagePlacement stretch
            = placeImage(ImageScalingType::STRETCH, 100.0f, 50.0f, 200.0f, 200.0f, 0.5f);
        check(near(stretch.width, 100.0f) && near(stretch.height, 100.0f), "stretch + scale=0.5");

        const ImagePlacement zero = placeImage(ImageScalingType::FIT, 100.0f, 50.0f, 200.0f, 200.0f, 0.0f);
        check(near(zero.width, 200.0f), "scale 非正数按 1.0 处理（图不会凭空消失）");
    }

    std::printf("\n== 6. placeImage：偏移 xy ==\n");
    {
        const ImagePlacement p
            = placeImage(ImageScalingType::CENTER, 100.0f, 50.0f, 200.0f, 200.0f, 1.0f, 10.0f, -4.0f);
        check(near(p.x, 60.0f) && near(p.y, 71.0f), "center 的居中位置 + 偏移（50+10 / 75-4）");
    }

    std::printf("\n== 7. placeImage：fill 模式忽略 scale / offset ==\n");
    {
        const ImagePlacement base = placeImage(ImageScalingType::FILL, 100.0f, 50.0f, 200.0f, 200.0f);
        check(near(base.width, 200.0f) && near(base.height, 200.0f), "fill 铺满盒子");
        check(near(base.uvMinX, 0.25f) && near(base.uvMaxX, 0.75f), "fill 按比例裁切（uv 收窄）");

        const ImagePlacement withKnobs
            = placeImage(ImageScalingType::FILL, 100.0f, 50.0f, 200.0f, 200.0f, 2.0f, 30.0f, 30.0f);
        check(near(withKnobs.width, base.width) && near(withKnobs.height, base.height),
            "fill 下 scale 被忽略");
        check(near(withKnobs.x, base.x) && near(withKnobs.y, base.y), "fill 下 offset 被忽略");
        check(near(withKnobs.uvMinX, base.uvMinX) && near(withKnobs.uvMaxY, base.uvMaxY),
            "fill 下 uv 也不受影响");
    }

    std::printf("\n== 8. 退化输入：没有图片尺寸时返回空矩形（不崩）==\n");
    {
        const ImagePlacement p = placeImage(ImageScalingType::FIT, 0.0f, 0.0f, 200.0f, 200.0f);
        check(near(p.width, 0.0f) && near(p.height, 0.0f), "0x0 图 -> 空矩形");
    }

    std::printf("\n== 9. 同一个基准：像素量随 uiScale 缩放，比例/百分比/主题值不重复乘 ==\n");
    {
        setUiScale(2.0f);

        // 字面量像素量：offsetX/offsetY/cornerRadius 都跟着放大
        if (const auto tmpl = ViewTemplate::fromString(R"({
                "type": "Image", "src": "@res/testcard.png", "cornerRadius": 3,
                "offsetX": 10, "offsetY": -4
            })",
                "test/ui-scale"))
        {
            View* root = tmpl->instantiate();
            if (auto* image = dynamic_cast<Image*>(root))
            {
                checkFloat(image->getOffsetX(), 20.0f, "offsetX 10 -> 20（uiScale=2）");
                checkFloat(image->getOffsetY(), -8.0f, "offsetY -4 -> -8");
                checkFloat(image->getImageCornerRadius(), 6.0f, "cornerRadius 3 -> 6");
                checkFloat(image->getDisplayScale(), 1.0f, "显示比例是系数，不缩放（仍为 1）");
            }
            else
            {
                check(false, "根节点应当是 Image");
            }
            delete root;
        }

        // 主题引用：@style/ 已经由 ThemeData::number 乘过缩放，不能再乘一次
        const float metricValue = getTheme().number("metric.spacing.md"); // 14 * 2 = 28
        if (const auto tmpl = ViewTemplate::fromString(R"({
                "type": "Image", "src": "@res/testcard.png", "offsetX": "@style/metric.spacing.md"
            })",
                "test/ui-scale-theme"))
        {
            View* root = tmpl->instantiate();
            if (auto* image = dynamic_cast<Image*>(root))
                checkFloat(image->getOffsetX(), metricValue, "@style 引用不重复缩放");
            else
                check(false, "根节点应当是 Image");
            delete root;
        }

        // 百分比不缩放（它本来就是相对父容器的）。`View::getWidth()` 读的是 Yoga 算完的
        // 布局尺寸（没算过就是 NAN），所以这里用一个**存下来的**量来验：Label 的字号。
        if (const auto tmpl = ViewTemplate::fromString(R"({
                "type": "Box", "width": "50%", "height": 100,
                "children": [ { "type": "Label", "text": "x", "fontSize": 14 } ]
            })",
                "test/ui-scale-percent"))
        {
            View* root = tmpl->instantiate();
            auto* box = root != nullptr ? root->asBox() : nullptr;
            check(box != nullptr, "Box 建出来了");
            if (box != nullptr && !box->getChildren().empty())
            {
                if (auto* label = dynamic_cast<Label*>(box->getChildren().front()))
                    checkFloat(label->getFontSize(), 28.0f, "字号 14 -> 28（同一个基准）");
                else
                    check(false, "子节点应当是 Label");
            }
            check(std::isnan(root != nullptr ? root->getWidth() : 0.0f),
                "50% 保持百分比语义：不预先变成长度（换算交给布局）");
            delete root;
        }

        setUiScale(1.0f);
    }

    std::printf("\n== 10. 布局属性不再被静默忽略：padding / gap / axis / alignItems ==\n");
    {
        setUiScale(1.0f);
        const auto tmpl = ViewTemplate::fromString(R"({
            "type": "Box", "axis": "row", "alignItems": "center", "gap": 12,
            "padding": 10, "paddingTop": 4,
            "children": [ { "type": "Box", "width": 10, "height": 10 } ]
        })",
            "test/layout-attrs");
        check(tmpl != nullptr, "模板装载成功");
        if (tmpl != nullptr)
        {
            View* root = tmpl->instantiate();
            check(root != nullptr, "实例化成功（不再有 未知属性 警告）");
            if (auto* box = root != nullptr ? root->asBox() : nullptr)
            {
                check(box->getAxis() == Axis::ROW, "axis=row 生效");
                check(box->getAlignItems() == AlignItems::CENTER, "alignItems=center 生效");
                checkFloat(box->getGap(), 12.0f, "gap 生效");
                checkFloat(box->getPaddingTop(), 4.0f, "paddingTop 覆盖了 padding 的上边");
                checkFloat(box->getPaddingLeft(), 10.0f, "padding 的其余边保留");
            }
            else
            {
                check(false, "根节点应当是 Box");
            }
            delete root;
        }
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

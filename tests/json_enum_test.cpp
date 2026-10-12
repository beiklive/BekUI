// 无窗口测试：枚举取值的**宽容匹配**。
//
// 视图描述 JSON 传统上写 camelCase（flexEnd / spaceBetween），
// 控件外观 JSON 传统上写 snake_case（flex_end / space_between）。
// 统一后：两套 JSON 都接受任意一种（大小写、`_`、`-` 都不敏感），
// 而且**不认识的值不再静默改成别的值**，只警告并保持原值。

#include <cstdio>
#include <string>

#include "bklv/core/appearance.hpp"
#include "bklv/core/attributes.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/label.hpp"

using namespace bklv;

#include "test_util.hpp"

int main()
{
    bklv::initResources();
    initI18n();
    setLocale("zh-Hans");

    std::printf("== 1. 宽容匹配本身 ==\n");
    {
        check(enumMatches("flexEnd", "flexEnd"), "同一种写法");
        check(enumMatches("flex_end", "flexEnd"), "snake_case == camelCase");
        check(enumMatches("FLEX_END", "flexEnd"), "全大写也认");
        check(enumMatches("Flex-End", "flexEnd"), "连字符与大小写都不敏感");
        check(enumMatches("space_between", "spaceBetween"), "多词同理");
        check(!enumMatches("flexEnding", "flexEnd"), "前缀像但不等价 → 不算匹配");
        check(!enumMatches("", "flexEnd"), "空串不算匹配");
    }

    std::printf("\n== 2. 视图描述 JSON：两种写法落到同一个值 ==\n");
    {
        auto buildRow = [](const char* justify, const char* align) {
            const std::string json = std::string(R"({"type":"Row","justifyContent":")")
                + justify + R"(","alignItems":")" + align + R"("})";
            return createFromJSONString(json);
        };

        View* camel = buildRow("spaceBetween", "flexEnd");
        View* snake = buildRow("space_between", "flex_end");
        View* upper = buildRow("SPACE_BETWEEN", "FLEX_END");

        for (View* v : { camel, snake, upper })
            check(v != nullptr, "能建出 Row");
        if (camel != nullptr && snake != nullptr && upper != nullptr)
        {
            Box* a = camel->asBox();
            Box* b = snake->asBox();
            Box* c = upper->asBox();
            check(a->getJustifyContent() == JustifyContent::SPACE_BETWEEN, "camelCase 生效");
            check(b->getJustifyContent() == a->getJustifyContent(),
                "snake_case 与 camelCase 结果一致（justifyContent）");
            check(c->getJustifyContent() == a->getJustifyContent(),
                "全大写结果也一致（justifyContent）");
            check(b->getAlignItems() == a->getAlignItems(),
                "snake_case 与 camelCase 结果一致（alignItems）");
            check(a->getAlignItems() == AlignItems::FLEX_END, "flexEnd 生效");
        }
        delete camel;
        delete snake;
        delete upper;
    }

    std::printf("\n== 3. 其余视图枚举：大小写/分隔符不敏感 ==\n");
    {
        View* row = createFromJSONString(R"({"type":"Row","axis":"ROW"})");
        check(row != nullptr && row->asBox()->getAxis() == Axis::ROW, "axis 大写 ROW");
        delete row;

        View* hidden = createFromJSONString(R"({"type":"Box","visibility":"GONE"})");
        check(hidden != nullptr && hidden->getVisibility() == Visibility::GONE, "visibility 大写");
        delete hidden;

        View* img = createFromJSONString(
            R"({"type":"Image","src":"@res/img/testcard.png","scalingType":"FILL"})");
        check(img != nullptr && static_cast<Image*>(img)->getScalingType() == ImageScalingType::FILL,
            "scalingType 大写 FILL");
        delete img;
    }

    std::printf("\n== 4. 控件外观 JSON：box 的枚举同样宽容 ==\n");
    {
        const char* appearanceJson = R"({
            "box": { "axis": "ROW", "justifyContent": "space_between", "alignItems": "flexEnd" },
            "layers": []
        })";

        std::string error;
        std::shared_ptr<const Appearance> appearance
            = Appearance::compile(appearanceJson, getTheme(), error);
        check(appearance != nullptr, "外观能编译");
        if (appearance != nullptr)
        {
            Box* box = new Box(Axis::COLUMN);
            applyAppearanceBox(box, appearance->getBox());
            check(box->getAxis() == Axis::ROW, "外观里 axis: \"ROW\" 生效");
            check(box->getJustifyContent() == JustifyContent::SPACE_BETWEEN,
                "外观里 space_between 生效（和视图 JSON 的 spaceBetween 同一个值）");
            check(box->getAlignItems() == AlignItems::FLEX_END, "外观里 flexEnd 生效");
            delete box;
        }
    }

    std::printf("\n== 5. 不认识的值：只警告，不改动原值 ==\n");
    {
        // 外观路径：在已经设过值的控件上套一份拼错的 box，值必须**保持不变**
        // （以前是静默落到默认值，界面莫名其妙变回左对齐）
        const char* typoJson = R"({
            "box": { "justifyContent": "spacebetwen" },
            "layers": []
        })";

        std::string error;
        std::shared_ptr<const Appearance> typo = Appearance::compile(typoJson, getTheme(), error);
        check(typo != nullptr, "拼错值的外观也能编译（只警告，不报错）");
        if (typo != nullptr)
        {
            Box* box = new Box(Axis::COLUMN);
            box->setJustifyContent(JustifyContent::CENTER);
            box->setAlignItems(AlignItems::FLEX_END);

            applyAppearanceBox(box, typo->getBox());

            check(box->getJustifyContent() == JustifyContent::CENTER,
                "justifyContent 拼错：保持原值 CENTER");
            check(box->getAlignItems() == AlignItems::FLEX_END,
                "没写的字段不动（alignItems 还是 FLEX_END）");
            delete box;
        }

        // 视图路径：拼错直接建不出来，但日志里是**可读的**报错（字段名 + 可选值 + 实际值）；
        // 这比静默落到默认值好——加载期就暴露，不会画出个"对齐莫名其妙不对"的界面
        View* row = createFromJSONString(R"({"type":"Row","justifyContent":"spacebetwen"})");
        check(row == nullptr, "视图 JSON 里枚举拼错 → 不建控件（日志有可选值清单）");
        delete row;
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

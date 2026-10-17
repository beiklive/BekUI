// 无窗口测试：JSON 变量块（vars + $color.x / $number.y）。
//
// 约定（用户拍板）：
//   · `@前缀` = 引用别的文件（@theme / @i18n / @res）
//   · `$前缀` = 引用**同文件**的 vars
//   · 一个文件只有一个 vars、必须在顶层；只有 colors / numbers 两个子块
//   · $ 的作用域是整份文档（box / layers / states / children 都能直接用）
//
// 这里钉四件事：展开是否正确、错法是否都报错、外观"用 vars"与"写字面量"是否完全等价、
// 视图 JSON 的 box children 能不能直接用 $。

#include <cstdio>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "bklv/compile/json_util.hpp"
#include "bklv/compile/json_vars.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/core/view.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "test_util.hpp"

using namespace bklv;
using Json = nlohmann::json;

namespace
{

// 展开一份 JSON 文本；失败返回空字符串，error 带原因
Json expand(const std::string& text, std::string& error)
{
    Json doc;
    try
    {
        doc = Json::parse(text);
    }
    catch (const Json::parse_error& e)
    {
        error = std::string("（测试用的 JSON 本身写错了）") + e.what();
        return Json();
    }

    if (!compile::expandJsonVars(doc, error))
        return Json();
    return doc;
}

// 期望失败：返回 false 表示"确实报错了"，并且错误信息里含 needle
bool failsWith(const std::string& text, const std::string& needle)
{
    std::string error;
    const Json doc = expand(text, error);
    if (!doc.is_null())
        return false;
    return error.find(needle) != std::string::npos;
}

} // namespace

int main()
{
    bklv::initResources();

    std::printf("== 1. 颜色/数值：字面量与 @theme 引用都能填进去 ==\n");
    {
        std::string error;
        const Json doc = expand(R"({
            "vars": {
                "colors":  { "ring": "#FF8800CC", "bg": "@theme/color.surface.raised" },
                "numbers": { "row": 44, "gap": "@theme/metric.spacing.md" }
            },
            "layers": [
                { "op": "rect", "fill": "$color.ring", "radius": "$number.gap" },
                { "op": "rect", "fill": "$color.bg",   "inset": [0, 0, 0, "$number.row"] }
            ]
        })",
            error);

        check(doc.is_object(), "能展开（" + error + "）");
        if (doc.is_object())
        {
            checkEqual(doc["layers"][0]["fill"].get<std::string>(), "#FF8800CC", "颜色变量 -> 字面量");
            checkEqual(doc["layers"][1]["fill"].get<std::string>(), "@theme/color.surface.raised",
                "@theme 引用原样保留（交给下游解析）");
            checkEqual(doc["layers"][0]["radius"].get<std::string>(), "@theme/metric.spacing.md",
                "数值变量也把 @theme 引用原样传下去");
        }
    }

    std::printf("\n== 2. 数值变量 + 数组里的引用 + vars 已被删掉 ==\n");
    {
        std::string error;
        const Json doc = expand(R"({
            "vars": { "numbers": { "row": 44 } },
            "box": { "height": "$number.row" },
            "layers": [ { "op": "rect", "inset": [0, 0, 0, "$number.row"] } ]
        })",
            error);

        check(doc.is_object(), "能展开（" + error + "）");
        if (doc.is_object())
        {
            checkFloat(doc["box"]["height"].get<float>(), 44.0f, "box.height 被替换");
            checkFloat(doc["layers"][0]["inset"][3].get<float>(), 44.0f, "数组里的 $ 也被替换");
            check(!doc.contains("vars"), "展开后 vars 被删掉（下游不会当成未知键）");
        }
    }

    std::printf("\n== 3. 链式引用：var 指向 var（≤4 层）==\n");
    {
        std::string error;
        const Json doc = expand(R"({
            "vars": {
                "colors":  { "a": "$color.b", "b": "$color.c", "c": "#112233" },
                "numbers": { "outer": "$number.inner", "inner": 12 }
            },
            "layers": [ { "op": "rect", "fill": "$color.a", "radius": "$number.outer" } ]
        })",
            error);

        check(doc.is_object(), "三链能解析（" + error + "）");
        if (doc.is_object())
        {
            checkEqual(doc["layers"][0]["fill"].get<std::string>(), "#112233", "颜色链式解析到最底");
            checkFloat(doc["layers"][0]["radius"].get<float>(), 12.0f, "数值链式解析到最底");
        }
    }

    std::printf("\n== 4. 写错都要报错（不静默降级）==\n");
    {
        check(failsWith(R"({ "vars": { "colors": { "a": "#112233" } },
                            "layers": [ { "op": "rect", "fill": "$color.missing" } ] })",
                  "$color.missing"),
            "未知变量 -> 报错并点名");

        check(failsWith(R"({ "layers": [ { "op": "rect", "fill": "$bg" } ] })", "$bg"),
            "没有块名的 $写法 -> 报错");

        check(failsWith(R"({ "vars": { "colors": { "a": "$color.b", "b": "$color.a" } },
                            "layers": [ { "op": "rect", "fill": "$color.a" } ] })",
                  "循环"),
            "循环引用 -> 报错");

        check(failsWith(R"({ "vars": { "numbers": {
                              "a": "$number.b", "b": "$number.c", "c": "$number.d",
                              "d": "$number.e", "e": 1 } },
                            "box": { "height": "$number.a" } })",
                  "超过"),
            "引用链超过 4 层 -> 报错");

        check(failsWith(R"({ "vars": { "strings": { "a": "x" } }, "layers": [] })", "colors"),
            "vars 里出现 colors/numbers 以外的子块 -> 报错");

        // 颜色槽里塞数值变量：槽位类型由下游的 resolveColor 负责，这里确认它会给一句人话
        {
            const ThemeData& theme = getTheme();
            std::string error;
            const auto appearance = Appearance::compile(R"({
                "vars": { "numbers": { "a": 1 } },
                "layers": [ { "op": "rect", "fill": "$number.a" } ]
            })",
                theme, error);
            check(appearance == nullptr, "颜色槽里放数值变量 -> 编译失败");
            check(error.find("颜色") != std::string::npos,
                "失败信息点明期望颜色（实际：" + error + "）");
        }

        check(failsWith(R"({ "vars": { "colors": { "a": 1 } }, "layers": [] })", "必须是字符串"),
            "颜色变量不是字符串 -> 报错");

        check(failsWith(R"({ "vars": { "colors": { "a": "$number.x" }, "numbers": { "x": 1 } },
                            "layers": [] })",
                  "不能引用"),
            "颜色变量指向数值变量 -> 报错");

        check(failsWith(R"({ "vars": { "colors": { "a": "#112233" } },
                            "box": { "vars": { "numbers": { "x": 1 } } } })",
                  "顶层"),
            "嵌套的 vars -> 报错");

        check(failsWith(R"({ "vars": 3, "layers": [] })", "必须是对象"),
            "vars 不是对象 -> 报错");
    }

    std::printf("\n== 5. 外观：用 vars 与写字面量编译结果完全等价 ==\n");
    {
        const ThemeData& theme = getTheme();

        const std::string literal = R"({
            "box": { "height": "@theme/metric.control.height" },
            "layers": [
                { "op": "rect", "id": "bg", "fill": "@theme/color.surface.raised",
                  "radius": "@theme/metric.radius.md" },
                { "op": "text", "id": "text", "source": "text", "color": "@theme/color.text.primary" }
            ]
        })";

        const std::string withVars = R"({
            "vars": {
                "colors":  { "surface": "@theme/color.surface.raised",
                             "label":   "@theme/color.text.primary" },
                "numbers": { "height": "@theme/metric.control.height",
                             "radius": "@theme/metric.radius.md" }
            },
            "box": { "height": "$number.height" },
            "layers": [
                { "op": "rect", "id": "bg", "fill": "$color.surface", "radius": "$number.radius" },
                { "op": "text", "id": "text", "source": "text", "color": "$color.label" }
            ]
        })";

        std::string errorA;
        std::string errorB;
        const auto plain = Appearance::compile(literal, theme, errorA);
        const auto vars  = Appearance::compile(withVars, theme, errorB);

        check(plain != nullptr, "字面量外观编译通过（" + errorA + "）");
        check(vars != nullptr, "vars 外观编译通过（" + errorB + "）");

        if (plain != nullptr && vars != nullptr)
        {
            checkFloat(vars->getBox().height, plain->getBox().height, "box.height 一致");
            const auto& varsLayers  = vars->getLayers();
            const auto& plainLayers = plain->getLayers();
            checkEqualSize(varsLayers.size(), plainLayers.size(), "层数一致");
            if (varsLayers.size() == plainLayers.size())
            {
                for (size_t i = 0; i < plainLayers.size(); i++)
                {
                    const std::string label = "第 " + std::to_string(i + 1) + " 层";
                    checkColor(varsLayers[i].fill, plainLayers[i].fill, label + " 填充色一致");
                    checkFloat(varsLayers[i].radius, plainLayers[i].radius, label + " 圆角一致");
                    checkColor(varsLayers[i].textColor, plainLayers[i].textColor,
                        label + " 文字颜色一致");
                }
            }
        }
    }

    std::printf("\n== 6. 视图描述：box 的 children 直接用 $，不用再写一次 vars ==\n");
    {
        std::string error;
        const std::string json = R"({
            "vars": {
                "colors":  { "accent": "@theme/color.accent.base" },
                "numbers": { "row": 40, "gap": 8 }
            },
            "type": "Box", "axis": "column", "width": 200, "gap": "$number.gap",
            "children": [
                { "type": "Box", "height": "$number.row", "width": 100 },
                { "type": "Box", "height": "$number.row", "width": 100 }
            ]
        })";

        Json parsed;
        try
        {
            parsed = Json::parse(json);
        }
        catch (const Json::parse_error& e)
        {
            error = e.what();
        }
        check(error.empty(), "测试用 JSON 合法");
        check(compile::expandJsonVars(parsed, error), "视图 JSON 的 vars 能展开（" + error + "）");
        check(!parsed.contains("vars"), "展开后 vars 从视图 JSON 里删掉");

        const std::shared_ptr<const ViewTemplate> tmpl = ViewTemplate::fromString(json, "vars_test");
        check(tmpl != nullptr, "带 vars 的视图模板能装载");
        if (tmpl != nullptr)
        {
            View* root = tmpl->instantiate();
            check(root != nullptr, "能实例化");
            if (root != nullptr && root->asBox() != nullptr)
            {
                // 布局一次才能读到子节点的尺寸
                YGNodeCalculateLayout(root->getYGNode(), 400.0f, 400.0f, YGDirectionLTR);
                const std::vector<View*>& kids = root->asBox()->getChildren();
                checkEqualSize(kids.size(), (size_t)2, "两个子节点");
                if (kids.size() == 2)
                {
                    checkFloat(kids[0]->getHeight(), 40.0f, "children 里直接写的 $number.row 生效");
                    checkFloat(kids[1]->getHeight(), 40.0f, "第二个子节点同样生效");
                }
            }
            delete root;
        }
    }

    std::printf("\n== 7. 回归：没有 vars 的文件原样通过 ==\n");
    {
        std::string error;
        const Json doc = expand(R"({
            "layers": [ { "op": "rect", "fill": "@theme/color.surface.raised", "radius": 6 } ]
        })",
            error);

        check(doc.is_object(), "无 vars 的文档也能过（" + error + "）");
        if (doc.is_object())
        {
            checkEqual(doc["layers"][0]["fill"].get<std::string>(), "@theme/color.surface.raised",
                "@theme 引用不受影响");
            checkFloat(doc["layers"][0]["radius"].get<float>(), 6.0f, "字面量不受影响");
        }
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

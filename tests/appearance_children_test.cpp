// 无窗口测试：`children` 段的解析（docs/parse-refactor-review.md §12.5 的规则）。
// 只测解析，不建视图 —— 实例化在里程碑 2/3。

#include <cstdio>
#include <string>

#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

std::shared_ptr<const Appearance> compileDoc(const std::string& children, std::string& error)
{
    const std::string json = children.empty()
        ? std::string(R"({ "layers": [ { "op": "rect", "fill": "#00000000" } ] })")
        : std::string(R"({ "layers": [ { "op": "rect", "fill": "#00000000" } ], "children": )")
            + children + " }";
    return Appearance::compile(json, getTheme(), error);
}

} // namespace

int main()
{
    std::printf("== 1. 没写 children = 空、不影响编译（现有主题的行为必须不变）==\n");
    {
        std::string error;
        const auto appearance = compileDoc("", error);
        check(appearance != nullptr, "没写 children 也能编译");
        if (appearance != nullptr)
            check(appearance->getChildren().empty(), "children 为空");
    }

    std::printf("\n== 2. 三档节点都能解析 ==\n");
    {
        std::string error;
        const auto appearance = compileDoc(R"([
            { "appearance": "base/label", "text": "@i18n/x", "box": { "grow": 1 } },
            { "box": { "padding": 12, "gap": 4 } },
            { "ref": "widget/button.icon_text", "icon": "@icon/check" }
        ])", error);
        check(appearance != nullptr, "三档一起写能编译");
        if (appearance != nullptr)
        {
            const auto& children = appearance->getChildren();
            checkEqualInt((int)children.size(), 3, "三个子节点");
            if (children.size() == 3)
            {
                checkEqualInt((int)children[0].kind, (int)AppearanceChild::Kind::APPEARANCE, "第 1 档是 appearance");
                check(children[0].name == "base/label", "记下外观名");
                check(children[0].text == "@i18n/x", "内容槽 text");
                checkFloat(children[0].box.grow, 1.0f, "box 覆盖到 grow");

                checkEqualInt((int)children[1].kind, (int)AppearanceChild::Kind::BOX, "第 2 档是 box");
                checkFloat(children[1].box.padding[0], 12.0f, "box.padding 落到四边");
                checkFloat(children[1].box.gap, 4.0f, "box.gap");

                checkEqualInt((int)children[2].kind, (int)AppearanceChild::Kind::VIEW_TEMPLATE, "第 3 档是 ref");
                check(children[2].name == "widget/button.icon_text", "记下模板名");
                check(children[2].icon == "@icon/check", "内容槽 icon");
            }
        }
    }

    std::printf("\n== 3. 递归 children ==\n");
    {
        std::string error;
        const auto appearance = compileDoc(R"([
            { "box": { "gap": 4 }, "children": [
                { "appearance": "base/label", "text": "a" },
                { "box": {}, "children": [ { "appearance": "base/label", "text": "b" } ] }
            ] }
        ])", error);
        check(appearance != nullptr, "嵌套能编译");
        if (appearance != nullptr && appearance->getChildren().size() == 1)
        {
            const auto& outer = appearance->getChildren()[0];
            checkEqualInt((int)outer.children.size(), 2, "第一层两个子节点");
            if (outer.children.size() == 2)
                checkEqualInt((int)outer.children[1].children.size(), 1, "再深一层还有一个");
        }
    }

    std::printf("\n== 4. 写错必报错（不静默降级）==\n");
    {
        std::string error;
        check(compileDoc(R"([ { "appearance": "base/label", "ref": "x" } ])", error) == nullptr,
            "appearance 与 ref 同时写 -> 失败");
        check(!error.empty(), "错误串非空");

        error.clear();
        check(compileDoc(R"([ { "children": [] } ])", error) == nullptr, "什么都没写 -> 失败");

        error.clear();
        check(compileDoc(R"([ { "ref": "x", "children": [ { "box": {} } ] } ])", error) == nullptr,
            "ref 节点再写 children -> 失败");

        error.clear();
        check(compileDoc(R"([ "不是对象" ])", error) == nullptr, "子节点不是对象 -> 失败");

        error.clear();
        check(compileDoc(R"({ "box": {} })", error) == nullptr, "children 不是数组 -> 失败");
    }

    std::printf("\n== 4b. 已知坑：children 写进 box 里不会生效（只告警）==\n");
    {
        // `box` 里只认 width/height/grow/shrink/gap/padding；这里写了 children 会被忽略，
        // 解析器打一条告警（写这条测试时我自己踩过）。行为钉住，免得以后误判成 bug。
        std::string error;
        const auto appearance = compileDoc(R"([ { "box": { "children": [ { "box": {} } ] } } ])", error);
        check(appearance != nullptr, "box 里写了 children 仍能编译（只是不生效）");
        if (appearance != nullptr && appearance->getChildren().size() == 1)
            check(appearance->getChildren()[0].children.empty(), "box 里的 children 被忽略 -> 空");
    }

    std::printf("\n== 5. 深度上限 16 ==\n");
    {
        // 造 18 层嵌套（每层一个 box 包一个 box）
        std::string deep = R"({ "box": {} })";
        for (int i = 0; i < 17; i++)
            deep = R"({ "box": {}, "children": [ )" + deep + R"( ] })";

        std::string error;
        check(compileDoc("[" + deep + "]", error) == nullptr, "超过 16 层 -> 失败");
        check(error.find("16") != std::string::npos, "错误串点明层数上限");

        // 16 层以内应当通过
        std::string ok = R"({ "box": {} })";
        for (int i = 0; i < 14; i++)
            ok = R"({ "box": {}, "children": [ )" + ok + R"( ] })";
        error.clear();
        check(compileDoc("[" + ok + "]", error) != nullptr, "16 层以内 -> 通过");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

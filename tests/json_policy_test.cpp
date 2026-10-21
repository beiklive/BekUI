// 无窗口测试：JSON 错误策略（STRICT / SKIP_FIELD / FALLBACK_DEFAULT）与诊断。
//
// 三条边界（和 bklv/compile/json_policy.hpp 的头注释一致）：
//   ① 能"跳过"的只有字段级错误；文件级错误（JSON 坏、缺 layers）只能整份回退
//   ② 无论哪种策略都要留诊断：file / path / message，recovered 标明是否已恢复
//   ③ 枚举拼错仍是"警告 + 保持原值"（策略不改它，免得破坏第三方主题）

#include <cstdio>
#include <cstdlib>
#include <string>

#include "bklv/compile/json_policy.hpp"
#include "bklv/compile/json_vars.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/core/globals.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/view.hpp"
#include "test_util.hpp"

using namespace bklv;

namespace
{

// 带一个坏字段的外观：fill 是非法颜色，其余字段合法
const char* kBadFillAppearance = R"({
    "layers": [
        { "op": "rect", "id": "bg", "fill": "#GGGGGG", "radius": 6 },
        { "op": "rect", "id": "top", "fill": "#112233" }
    ]
})";

int countRecovered(bool recovered)
{
    int count = 0;
    for (const compile::JsonDiagnostic& diagnostic : compile::getJsonDiagnostics())
        if (diagnostic.recovered == recovered)
            count++;
    return count;
}

bool anyDiagnosticPathContains(const std::string& needle)
{
    for (const compile::JsonDiagnostic& diagnostic : compile::getJsonDiagnostics())
        if (diagnostic.path.find(needle) != std::string::npos)
            return true;
    return false;
}

} // namespace

int main()
{
    bklv::initResources();
    const ThemeData& theme = getTheme();

    std::printf("== 1. 策略：默认 STRICT，可以切，名字对得上 ==\n");
    {
        // 进程刚起来、没 setenv 也没 setter 时：STRICT
        check(compile::getJsonErrorPolicy() == compile::JsonErrorPolicy::STRICT,
            "默认是 STRICT（没做任何设置时）");
        checkEqual(compile::jsonErrorPolicyName(compile::JsonErrorPolicy::STRICT), "strict", "STRICT 名字");
        checkEqual(compile::jsonErrorPolicyName(compile::JsonErrorPolicy::SKIP_FIELD), "skip",
            "SKIP_FIELD 名字");
        checkEqual(compile::jsonErrorPolicyName(compile::JsonErrorPolicy::FALLBACK_DEFAULT),
            "fallback", "FALLBACK_DEFAULT 名字");

        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::SKIP_FIELD);
        check(compile::getJsonErrorPolicy() == compile::JsonErrorPolicy::SKIP_FIELD, "能切到 SKIP_FIELD");
    }

    std::printf("\n== 2. 环境变量 BKLV_JSON_POLICY：显式设置优先，reset 后才重新读 ==\n");
    {
        // 注意：resetGlobals() 会**清空钩子表**（一轮只 reset 一次），所以这里只 reset 一次。
        unsetenv("BKLV_JSON_POLICY");
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::STRICT);
        check(compile::getJsonErrorPolicy() == compile::JsonErrorPolicy::STRICT, "先手动设成 STRICT");

        setenv("BKLV_JSON_POLICY", "fallback", 1);
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::STRICT);
        check(compile::getJsonErrorPolicy() == compile::JsonErrorPolicy::STRICT,
            "显式设置优先于环境变量");

        globals::resetGlobals(); // 钩子把 envApplied 清掉 -> 下一次 get 重新读环境变量
        check(compile::getJsonErrorPolicy() == compile::JsonErrorPolicy::FALLBACK_DEFAULT,
            "reset 后按环境变量生效（fallback）");

        unsetenv("BKLV_JSON_POLICY");
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::STRICT); // 钩子已用掉，手动复位
        check(compile::getJsonErrorPolicy() == compile::JsonErrorPolicy::STRICT, "手动复位回 STRICT");
    }

    std::printf("\n== 3. STRICT：字段级错误 = 整份失败，诊断标 recovered=false ==\n");
    {
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::STRICT);
        compile::clearJsonDiagnostics();

        std::string error;
        const auto appearance = Appearance::compile(kBadFillAppearance, theme, error, "test/bad-fill");

        check(appearance == nullptr, "非法颜色 -> 编译失败");
        check(error.find("fill") != std::string::npos, "错误信息点到 fill（实际：" + error + "）");
        checkEqualInt(countRecovered(true), 0, "没有已恢复的诊断");
        checkEqualInt(countRecovered(false), 1, "有一条未恢复的诊断");
        check(anyDiagnosticPathContains(".fill"), "诊断路径里带 .fill");
        checkEqual(compile::getJsonDiagnostics()[0].file, std::string("test/bad-fill"),
            "诊断里带来源（source）");
    }

    std::printf("\n== 4. SKIP_FIELD：坏字段跳过、其余照常编译 ==\n");
    {
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::SKIP_FIELD);
        compile::clearJsonDiagnostics();

        std::string error;
        const auto appearance = Appearance::compile(kBadFillAppearance, theme, error, "test/bad-fill");

        check(appearance != nullptr, "SKIP 下仍能编译（" + error + "）");
        if (appearance != nullptr)
        {
            const auto& layers = appearance->getLayers();
            checkEqualSize(layers.size(), (size_t)2, "两层都在（层结构不受影响）");
            if (layers.size() == 2)
            {
                checkColor(layers[0].fill, 0, "坏 fill 保持默认（透明）");
                checkFloat(layers[0].radius, 6.0f, "同一层的其他字段照常生效");
                checkColor(layers[1].fill, IM_COL32(0x11, 0x22, 0x33, 255), "第二层不受影响");
            }
        }
        checkEqualInt(countRecovered(true), 1, "诊断标了已恢复");
        check(anyDiagnosticPathContains(".fill"), "诊断路径里带 .fill");
    }

    std::printf("\n== 5. SKIP_FIELD：多个坏字段 = 多条诊断、互不影响 ==\n");
    {
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::SKIP_FIELD);
        compile::clearJsonDiagnostics();

        std::string error;
        const auto appearance = Appearance::compile(R"({
            "layers": [ { "op": "rect", "id": "bg",
                          "fill": "@theme/color.__nope__",
                          "radius": "@theme/metric.__nope__",
                          "strokeWidth": 2 } ]
        })",
            theme, error, "test/two-bad");

        check(appearance != nullptr, "两个坏字段都跳过、仍然编译成功（" + error + "）");
        checkEqualInt(countRecovered(true), 2, "两条已恢复的诊断");
        if (appearance != nullptr && !appearance->getLayers().empty())
        {
            checkColor(appearance->getLayers()[0].fill, 0, "坏 @theme 颜色保持默认");
            checkFloat(appearance->getLayers()[0].radius, 0.0f, "坏 @theme 数值保持默认");
            checkFloat(appearance->getLayers()[0].strokeWidth, 2.0f, "合法字段照常生效");
        }
    }

    std::printf("\n== 6. FALLBACK_DEFAULT：整份退到 default 主题的同名外观 ==\n");
    {
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::FALLBACK_DEFAULT);
        compile::clearJsonDiagnostics();

        // 用一份语法坏掉的 JSON 去"冒充"base/widget/button.text：应回退到 default 里的真身
        std::string error;
        const auto appearance = Appearance::compileWithFallback(R"({ this is not json )",
            "base/widget/button.text", theme, error);

        check(appearance != nullptr, "坏 JSON -> 回退成功后仍有外观（" + error + "）");
        checkEqualInt(countRecovered(true), 1, "有一条已回退的诊断");
        if (!compile::getJsonDiagnostics().empty())
            check(compile::getJsonDiagnostics()[0].message.find("回退") != std::string::npos,
                "诊断说明是回退");

        // 与直接编译 default 的那份应当一致
        const std::string* plainText = theme.defaultWidgetAppearanceJson("base/widget/button.text");
        check(plainText != nullptr, "default 主题里能找到 widget/button.text");
        if (plainText != nullptr && appearance != nullptr)
        {
            std::string plainError;
            const auto plainAppearance = Appearance::compile(*plainText, theme, plainError);
            check(plainAppearance != nullptr, "直接编译 default 的版本也成功（" + plainError + "）");
            if (plainAppearance != nullptr)
            {
                checkEqualSize(appearance->getLayers().size(), plainAppearance->getLayers().size(),
                    "回退后的层数与 default 一致");
                checkFloat(appearance->getBox().height, plainAppearance->getBox().height,
                    "回退后的 box 与 default 一致");
            }
        }
    }

    std::printf("\n== 7. FALLBACK_DEFAULT：没有可回退的对象时仍失败 ==\n");
    {
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::FALLBACK_DEFAULT);
        compile::clearJsonDiagnostics();

        std::string error;
        const auto appearance = Appearance::compileWithFallback(R"({ this is not json )",
            "base/widget/__no_such_widget__", theme, error);

        check(appearance == nullptr, "default 里也没有 -> 失败");
        check(!error.empty(), "仍然给出错误原因");
    }

    std::printf("\n== 8. 枚举拼错仍是警告：策略不改这条 ==\n");
    {
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::STRICT);
        compile::clearJsonDiagnostics();

        std::string error;
        const auto appearance = Appearance::compile(R"({
            "box": { "alignItems": "middle" },
            "layers": [ { "op": "rect", "id": "bg", "fill": "#112233" } ]
        })",
            theme, error, "test/bad-enum");

        check(appearance != nullptr, "枚举拼错在 STRICT 下仍能编译（" + error + "）");
    }

    std::printf("\n== 9. 视图：未知属性记一条已恢复的诊断 ==\n");
    {
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::STRICT);
        compile::clearJsonDiagnostics();

        const auto tmpl = ViewTemplate::fromString(R"({
            "type": "Box", "width": 100, "height": 40, "noSuchAttribute": 1,
            "children": [ { "type": "Box", "width": 10, "height": 10 } ]
        })",
            "test/view-unknown");
        check(tmpl != nullptr, "带未知属性的视图模板能装载");
        if (tmpl != nullptr)
        {
            View* root = tmpl->instantiate();
            check(root != nullptr, "未知属性不阻断建树");
            checkEqualInt(countRecovered(true), 1, "未知属性记一条已恢复诊断");
            check(anyDiagnosticPathContains("noSuchAttribute"), "诊断点到那个属性名");
            delete root;
        }
    }

    std::printf("\n== 10. 视图：属性类型错 —— STRICT 失败 / SKIP 跳过该字段 ==\n");
    {
        const std::string badView = R"({
            "type": "Box", "width": { "wrong": 1 }, "height": 40
        })";

        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::STRICT);
        compile::clearJsonDiagnostics();
        if (const auto tmpl = ViewTemplate::fromString(badView, "test/view-bad-type"))
        {
            View* root = tmpl->instantiate();
            check(root == nullptr, "STRICT：宽度类型错 -> 建树失败");
            delete root;
        }
        else
        {
            check(false, "模板本身应当能装载（错在属性值）");
        }

        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::SKIP_FIELD);
        compile::clearJsonDiagnostics();
        if (const auto tmpl = ViewTemplate::fromString(badView, "test/view-bad-type"))
        {
            View* root = tmpl->instantiate();
            check(root != nullptr, "SKIP：跳过坏字段后仍能建树");
            checkEqualInt(countRecovered(true), 1, "记了一条已恢复诊断");
            delete root;
        }
    }

    std::printf("\n== 11. 外观顶层键白名单：$ 开头的键放过，拼错的键报出来 ==\n");
    {
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::STRICT);
        compile::clearJsonDiagnostics();

        // $comment（元数据）与已知键组合不报错
        std::string okError;
        const auto okAppearance = Appearance::compile(R"({
            "$comment": "只给注释用",
            "layers": [ { "op": "rect", "id": "bg", "fill": "#112233" } ]
        })",
            theme, okError, "test/comment-key");
        check(okAppearance != nullptr, "$comment 不算未知键（" + okError + "）");

        // 拼错的顶层键（state 少个 s）：STRICT 直接失败，并给拼写提示
        std::string badError;
        const auto badAppearance = Appearance::compile(R"({
            "state": { "focused": {} },
            "layers": [ { "op": "rect", "id": "bg", "fill": "#112233" } ]
        })",
            theme, badError, "test/bad-top-key");
        check(badAppearance == nullptr, "STRICT：未知顶层键 -> 失败");
        check(badError.find("state") != std::string::npos, "错误信息点到那个键（" + badError + "）");
        check(badError.find("states") != std::string::npos, "给出\"是否想写 states\"的提示");

        // SKIP_FIELD：忽略这个键、照常编译，诊断里留一条
        compile::setJsonErrorPolicy(compile::JsonErrorPolicy::SKIP_FIELD);
        compile::clearJsonDiagnostics();
        std::string skipError;
        const auto skipAppearance = Appearance::compile(R"({
            "state": { "focused": {} },
            "layers": [ { "op": "rect", "id": "bg", "fill": "#112233" } ]
        })",
            theme, skipError, "test/bad-top-key");
        check(skipAppearance != nullptr, "SKIP：未知顶层键被忽略（" + skipError + "）");
        checkEqualInt(countRecovered(true), 1, "记了一条已恢复诊断");
        check(anyDiagnosticPathContains("state"), "诊断里带那个键名");
    }

    compile::setJsonErrorPolicy(compile::JsonErrorPolicy::STRICT);
    compile::clearJsonDiagnostics();

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

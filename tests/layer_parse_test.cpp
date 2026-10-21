// 无窗口测试：**图层解析**的行为基线（6 种 op 的键与报错）。
//
// 为什么先写这个：下一步要把 `parseLayers`（382 行）按 op 拆开、并把字段解析迁进
// `compile/layers/<op>.cpp`。那片代码目前是"按字段统一驱动"的，拆之前必须有行为基线，
// 否则拆完只能靠肉眼对比。这些断言就是那道保险。

#include <cstdio>
#include <string>

#include "bklv/compile/layer_register.hpp" // registerLayer<T>() 模板
#include "bklv/compile/layer_registry.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/ui_scale.hpp"

#include "test_contrib.hpp" // 共享的第三方层桩（stripe）

using namespace bklv;

#include "test_util.hpp"

namespace
{

std::shared_ptr<const Appearance> compileLayers(const std::string& layers, std::string& error)
{
    return Appearance::compile("{ \"layers\": [" + layers + "] }", getTheme(), error);
}

const AppearanceLayer* layerOf(const Appearance& appearance, const std::string& id)
{
    for (const AppearanceLayer& layer : appearance.getLayers())
        if (layer.id == id)
            return &layer;
    return nullptr;
}

// "只用注册模板"的层：不写 parse 钩子、不写 kIdPrefix / kHelp —— 全部走模板兜底。
// 定义在文件作用域（不能放在函数里：C++ 不允许局部 struct 有 static constexpr 数据成员）。
struct ParamLayer
{
    static constexpr const char* kName  = "param_test";
    static constexpr const char* kLabel = "参数测试";
    static constexpr bklv::compile::LayerOrder kOrder = bklv::compile::LayerOrder::DECORATION;

    static std::vector<bklv::compile::LayerParam> params()
    {
        using bklv::compile::LayerParam;
        return {
            LayerParam::number("strength", 0.5f, "强度"),
            LayerParam::pixels("lineWidth", 3.0f, "线宽"),
            LayerParam::color("tint", IM_COL32(1, 2, 3, 255), "染色"),
            LayerParam::boolean("mono", true, "单色"),
            LayerParam::enumeration("mode", { "horizontal", "vertical", "diagonal" }, 1, "方向"),
        };
    }
    static void draw(const bklv::compile::LayerDrawContext&) { }
};

} // namespace

int main()
{
    // 扩展 op 必须先注册才能编译（第 7 节要编译一段 stripe 的 JSON）
    test_contrib::registerStripe();
    setTheme("default"); // 层 import 要读主题里的外观文件

    std::printf("== 1. 六种 op 都能解析，且各自的键落到该落的字段 ==\n");
    {
        std::string error;

        const auto rect = compileLayers(R"({ "op": "rect", "id": "bg", "fill": "#112233FF",
            "radius": 8, "stroke": "#445566FF", "strokeWidth": 2 })", error);
        check(rect != nullptr, "rect 能编译");
        if (rect != nullptr)
        {
            const AppearanceLayer* l = layerOf(*rect, "bg");
            check(l != nullptr && l->op == AppearanceLayer::Op::RECT, "op 是 rect");
            if (l != nullptr)
            {
                check((l->fill & 0xFFu) == 0x11, "fill 落在 fill 上");
                checkFloat(l->radius, 8.0f, "radius");
                checkFloat(l->strokeWidth, 2.0f, "strokeWidth");
            }
        }

        error.clear();
        const auto text = compileLayers(R"({ "op": "text", "id": "title", "data_id": "text",
            "color": "#FFFFFFFF", "fontSize": 20, "align": "left" })", error);
        check(text != nullptr, "text 能编译");
        if (text != nullptr)
        {
            const AppearanceLayer* l = layerOf(*text, "title");
            check(l != nullptr && l->op == AppearanceLayer::Op::TEXT, "op 是 text");
            if (l != nullptr)
                check(l->dataId == "text", "data_id 落在 dataId 上");
        }

        error.clear();
        const auto image = compileLayers(R"({ "op": "image", "id": "pic", "data_id": "image",
            "scaling": "fill" })", error);
        check(image != nullptr, "image 能编译");
        if (image != nullptr)
        {
            const AppearanceLayer* l = layerOf(*image, "pic");
            check(l != nullptr && l->op == AppearanceLayer::Op::IMAGE, "op 是 image");
            if (l != nullptr)
                check(l->scaling == "fill", "scaling 落在 scaling 上");
        }

        error.clear();
        const auto shadow = compileLayers(R"({ "op": "shadow", "id": "shadow",
            "fill": "@theme/color.common.shadow", "feather": 16, "spread": 2,
            "offset": [0, 4], "cutout": true })", error);
        check(shadow != nullptr, "shadow 能编译");
        if (shadow != nullptr)
        {
            const AppearanceLayer* l = layerOf(*shadow, "shadow");
            check(l != nullptr && l->op == AppearanceLayer::Op::SHADOW, "op 是 shadow");
            if (l != nullptr)
            {
                checkFloat(l->feather, 16.0f, "feather");
                checkFloat(l->spread, 2.0f, "spread");
                check(l->cutout, "cutout 是 true");
            }
        }

        error.clear();
        const auto glass = compileLayers(R"({ "op": "glass", "id": "glass", "fill": "#FFFFFF33",
            "stroke": "#FFFFFF3D", "strokeWidth": 1 })", error);
        check(glass != nullptr, "glass 能编译");
        if (glass != nullptr)
        {
            const AppearanceLayer* l = layerOf(*glass, "glass");
            check(l != nullptr && l->op == AppearanceLayer::Op::GLASS, "op 是 glass");
            if (l != nullptr)
            {
                check((l->fill >> 24) == 0x33, "glass 的 fill 是染色");
                check((l->stroke >> 24) == 0x3D, "glass 的 stroke 是内缘高光");
            }
        }

        error.clear();
        const auto custom = compileLayers(R"({ "op": "custom", "id": "body", "fill": "#00000000" })", error);
        check(custom != nullptr, "custom 能编译");
        if (custom != nullptr)
        {
            const AppearanceLayer* l = layerOf(*custom, "body");
            check(l != nullptr && l->op == AppearanceLayer::Op::CUSTOM, "op 是 custom");
        }
    }

    std::printf("\n== 2. 没写 id 时按 op 生成默认 id（这是层序校验与 states 寻址的基础）==\n");
    {
        std::string error;
        const auto appearance = compileLayers(R"({ "op": "rect", "fill": "#00000000" },
            { "op": "text", "data_id": "text" })", error);
        check(appearance != nullptr, "不写 id 也能编译");
        if (appearance != nullptr)
        {
            // 实测：默认 id = 「映射表里的名字 + 循环下标（从 0 起）」
            check(appearance->indexOfLayer("rect0") >= 0, "第一层 rect 的默认 id 是 rect0");
            check(appearance->indexOfLayer("text1") >= 0, "第二层 text 的默认 id 是 text1");
        }
    }

    std::printf("\n== 2b. shadow / glass 的默认 id 也走注册表（此前是 quirk：叫 custom<N>）==\n");
    {
        std::string error;
        const auto appearance = compileLayers(R"({ "op": "shadow", "fill": "#00000040" },
            { "op": "glass", "fill": "#FFFFFF33" })", error);
        check(appearance != nullptr, "shadow/glass 不写 id 也能编译");
        if (appearance != nullptr)
        {
            // ①c-2 起：默认 id 前缀来自注册表，所以 shadow/glass 不再落到 custom。
            // 这是一次**有意的行为变化**（此前是 quirk：两者都叫 custom<N>），基线先钉住旧行为、
            // 修完再翻断言 —— 变化显式可见。
            check(appearance->indexOfLayer("shadow0") >= 0, "shadow 的默认 id 是 shadow0");
            check(appearance->indexOfLayer("glass1") >= 0, "glass 的默认 id 是 glass1");
            check(appearance->indexOfLayer("custom0") < 0, "不再落到 custom（quirk 已修）");
        }
    }

    std::printf("\n== 3. 写错要报错（不静默降级）==\n");
    {
        std::string error;
        check(compileLayers(R"({ "op": "不存在的类型" })", error) == nullptr, "op 不认识 -> 失败");
        check(!error.empty(), "错误串非空");

        error.clear();
        check(compileLayers(R"({ "op": "rect", "radius": "abc" })", error) == nullptr, "radius 类型不对 -> 失败");

        error.clear();
        check(compileLayers(R"({ "op": "rect", "fill": "@theme/color.不存在.键" })", error) == nullptr,
            "颜色引用不存在的键 -> 失败");

        error.clear();
        check(compileLayers(R"("不是对象")", error) == nullptr, "层不是对象 -> 失败");
    }

    std::printf("\n== 4. 图层类型注册表（单一事实来源）==\n");
    {
        using namespace bklv::compile;

        check(registeredLayers().size() >= 6, "内置 6 种 op 已注册（惰性注册）");
        for (const char* name : { "rect", "text", "image", "shadow", "glass", "custom" })
        {
            const LayerType* type = findLayer(name);
            check(type != nullptr, name);
            if (type != nullptr)
            {
                check(!type->idPrefix.empty(), std::string(name) + " 有默认 id 前缀");
                check(!type->keys.empty(), std::string(name) + " 有键表");
            }
        }

        // 注册表 vs 解析器：注册表里的每个 op，解析器都必须真的认识
        for (const LayerType& type : registeredLayers())
        {
            std::string error;
            const auto appearance = compileLayers("{ \"op\": \"" + type.name
                    + "\", \"fill\": \"#00000000\", \"source\": \"text\" }",
                error);
            check(appearance != nullptr, "注册表里的 op 解析器都认识：" + type.name);
        }

        // 层序：阴影最先、玻璃紧随其后；**内置层共用同一档**（保持 JSON 书写顺序，
        // 不按"rect 就是 BACKGROUND"重排 —— 那会让 121 份发布外观里的 hlGlow/hlBorder
        // 掉到文字下面，见 layerSortRank 的注释）；DECORATION / OVERLAY 供扩展层往后排。
        check(layerSortRank(LayerOrder::SHADOW) < layerSortRank(LayerOrder::GLASS),
            "阴影在玻璃之前");
        check(layerSortRank(LayerOrder::GLASS) < layerSortRank(LayerOrder::BACKGROUND),
            "玻璃在其余层之前");
        check(layerSortRank(LayerOrder::BACKGROUND) == layerSortRank(LayerOrder::CONTENT),
            "内置各档共用同一 rank（保持 JSON 顺序）");
        check(layerSortRank(LayerOrder::DECORATION) > layerSortRank(LayerOrder::CONTENT),
            "DECORATION 排在其余层之后");
        check(layerSortRank(LayerOrder::OVERLAY) > layerSortRank(LayerOrder::DECORATION),
            "OVERLAY 排在最后");

        // 内置 op 的类型查表：数组索引（绘制分派每层每帧都调它）
        check(builtinLayer(AppearanceLayer::Op::RECT) != nullptr, "builtinLayer(RECT) 有值");
        check(builtinLayer(AppearanceLayer::Op::RECT)->name == "rect", "查回来的是 rect");
        check(builtinLayer(AppearanceLayer::Op::EXTENSION) == nullptr,
            "EXTENSION 没有内置类型（要走 findLayer(extensionType)）");

        // 引擎自己读的键：设计器候选与"参数名撞车"提示都用它
        check(!engineLayerKeys().empty(), "引擎键表非空");

        // 扩展层不许用引擎保留键（踩过的坑，见 layer_registry.hpp 的注释）
        {
            std::string reservedError;
            LayerType bad;
            bad.name     = "stripe_reserved";
            bad.idPrefix = "stripe_reserved";
            bad.keys     = { "angle", "width" }; // width 是层几何键
            check(!registerLayer(std::move(bad), reservedError), "扩展层用保留键 -> 失败");
            check(reservedError.find("保留键") != std::string::npos, "错误串点明是保留键");
        }

        // 重复注册 / 空名字 / 空前缀都要报错
        std::string error;
        LayerType duplicate = *findLayer("rect");
        check(!registerLayer(duplicate, error), "重复注册 -> 失败");
        check(!error.empty(), "重复注册有错误串");

        error.clear();
        LayerType noName = duplicate;
        noName.name.clear();
        check(!registerLayer(noName, error), "空 name -> 失败");

        error.clear();
        LayerType noPrefix = duplicate;
        noPrefix.name = "rect_copy";
        noPrefix.idPrefix.clear();
        check(!registerLayer(noPrefix, error), "空 idPrefix -> 失败");
    }

    std::printf("\n== 5. 扩展 op：注册表里注册过就能用（op 枚举保持封闭）==\n");
    {
        using namespace bklv::compile;

        // 第三方在自己的初始化里注册一个扩展 op（这里模拟：不碰 bklv 任何文件）
        LayerType stripe;
        stripe.name     = "stripe_test";
        stripe.label    = "斜条纹";
        stripe.idPrefix = "stripe";
        stripe.order    = LayerOrder::DECORATION;
        stripe.help     = "测试用的扩展 op";
        stripe.keys     = { "angle", "lineWidth" };
        // parse 钩子必须**真的被调用**，而且它的失败要能变成硬错误
        stripe.parse    = [](const nlohmann::json& node, const bklv::ThemeData& theme,
                              bklv::AppearanceLayer& out, std::string& err, const std::string& where) {
            (void)theme;
            (void)where;
            if (node.contains("fail"))
            {
                err = "样例 parse 钩子主动失败";
                return false;
            }
            out.feather = node.value("angle", 0.0f); // 用自己的键写进层字段
            return true;
        };
        std::string error;
        check(registerLayer(std::move(stripe), error), "扩展 op 注册成功");

        const auto appearance = compileLayers(R"({ "op": "stripe_test", "angle": 45 })", error);
        check(appearance != nullptr, "注册过的扩展 op 能编译");
        if (appearance != nullptr)
        {
            checkEqualInt((int)appearance->getLayers()[0].op, (int)AppearanceLayer::Op::EXTENSION,
                "落到 Op::EXTENSION（枚举保持封闭）");
            check(appearance->getLayers()[0].extensionType == "stripe_test", "extensionType 记下类型名");
            check(appearance->indexOfLayer("stripe0") >= 0, "默认 id 用注册表的 idPrefix");
            // parse 钩子真的被调用了（angle=45 写进了 feather）
            checkFloat(appearance->getLayers()[0].feather, 45.0f, "扩展 op 的 parse 钩子被调用");

            std::string hookError;
            check(compileLayers(R"({ "op": "stripe_test", "fail": true })", hookError) == nullptr,
                "扩展 op 的 parse 钩子失败 -> 整体编译失败");
            check(hookError.find("样例 parse 钩子主动失败") != std::string::npos,
                "错误串来自钩子");
        }

        error.clear();
        check(compileLayers(R"({ "op": "没注册过的类型" })", error) == nullptr,
            "没注册过的 op 仍然报错（不静默放过）");
    }

    std::printf("\n== 6. 参数模式：声明一次，解析 / 缩放 / 校验全部自动 ==\n");
    {
        using namespace bklv::compile;

        // 参数解析要乘 uiScale，这里先把基准钉成 1.0（前面的小节可能改过）
        setUiScale(1.0f);

        // 用的层类型 ParamLayer 定义在文件顶部的匿名命名空间里
        // （C++ 不允许函数内的局部 struct 有 static constexpr 数据成员）
        check(registerLayer<ParamLayer>(), "用模板注册参数层成功");

        const LayerType* type = findLayer("param_test");
        check(type != nullptr, "注册表里能查到");
        if (type != nullptr)
        {
            checkEqual(type->idPrefix, "param_test", "kIdPrefix 没写就用 kName 兜底");
            checkEqual(type->label, "参数测试", "中文名来自 kLabel");
            check(type->help.empty(), "kHelp 没写就是空串");
            check(!type->parse, "没写 parse 钩子就留空（标量参数不需要它）");
            checkEqualSize(type->keys.size(), 5, "keys 由 params 派生（设计器候选自动跟上）");
        }

        // 只写一部分键：其余落到模式里的默认值
        std::string error;
        auto appearance = compileLayers(
            R"({ "op": "param_test", "strength": 0.25, "mono": false })", error);
        check(appearance != nullptr, "只写部分参数也能编译");
        if (appearance != nullptr)
        {
            const AppearanceLayer& layer = appearance->getLayers()[0];
            checkEqualInt((int)layer.paramCount, 5, "参数槽个数 = 声明个数");
            checkFloat(layer.params[0].number, 0.25f, "NUMBER 按 JSON 解析");
            checkFloat(layer.params[1].number, 3.0f, "PIXELS 没写就用默认值（乘 uiScale=1）");
            checkColor(layer.params[2].color, IM_COL32(1, 2, 3, 255), "COLOR 没写就用默认值");
            check(!layer.params[3].boolean, "BOOL 按 JSON 解析");
            checkFloat(layer.params[4].number, 1.0f, "ENUM 没写就用默认下标（第 2 个候选）");
        }

        // ENUM：宽容匹配；拼错只警告并保持默认值
        appearance = compileLayers(R"({ "op": "param_test", "mode": "VERTICAL" })", error);
        check(appearance != nullptr, "ENUM 大小写不敏感");
        if (appearance != nullptr)
            checkFloat(appearance->getLayers()[0].params[4].number, 1.0f, "VERTICAL 命中 vertical");
        appearance = compileLayers(R"({ "op": "param_test", "mode": "斜着" })", error);
        check(appearance != nullptr, "ENUM 拼错不打断编译");
        if (appearance != nullptr)
            checkFloat(appearance->getLayers()[0].params[4].number, 1.0f, "拼错保持默认值");

        // 类型写错是硬错误
        error.clear();
        check(compileLayers(R"({ "op": "param_test", "mono": "yes" })", error) == nullptr,
            "BOOL 给了字符串 -> 编译失败");
        error.clear();
        check(compileLayers(R"({ "op": "param_test", "strength": "#ffffff" })", error) == nullptr,
            "NUMBER 给了颜色串 -> 编译失败");

        // 参数模式本身的注册期校验
        LayerType tooMany;
        tooMany.name     = "too_many";
        tooMany.idPrefix = "too_many";
        for (size_t i = 0; i <= kMaxLayerParams; i++)
            tooMany.params.push_back(LayerParam::number("p" + std::to_string(i), 0.0f));
        check(!registerLayer(std::move(tooMany), error), "参数超过上限 -> 注册失败");

        LayerType reservedParam;
        reservedParam.name     = "reserved_param";
        reservedParam.idPrefix = "reserved_param";
        reservedParam.params.push_back(LayerParam::number("width", 1.0f)); // 引擎几何键
        check(!registerLayer(std::move(reservedParam), error), "参数占用引擎保留键 -> 注册失败");

        LayerType dupParam;
        dupParam.name     = "dup_param";
        dupParam.idPrefix = "dup_param";
        dupParam.params.push_back(LayerParam::number("a", 1.0f));
        dupParam.params.push_back(LayerParam::number("a", 2.0f));
        check(!registerLayer(std::move(dupParam), error), "参数名重复 -> 注册失败");

        LayerType badEnum;
        badEnum.name     = "bad_enum";
        badEnum.idPrefix = "bad_enum";
        badEnum.params.push_back(LayerParam::enumeration("mode", {}, 0));
        check(!registerLayer(std::move(badEnum), error), "ENUM 没有候选值 -> 注册失败");
    }

    std::printf("\n== 7. 实物验收：stripe 样例的参数落到正确的地方 ==\n");
    {
        using namespace bklv::compile;

        // 这一层的注册在 tests/test_contrib.hpp（与已移除的 demo 第三方层样例同构）。
        // 断言的是"主题 JSON 里的值 → 绘制钩子读到的位置"这条链：
        // 角度是 NUMBER（**不缩放**）、间距/线宽是 PIXELS（乘 uiScale）、
        // 而 fill/radius/inset 是引擎字段（和参数槽互不干扰）。
        setUiScale(2.0f); // 故意非 1：才看得出哪个缩放了

        std::string error;
        auto appearance = compileLayers(
            R"({ "op": "stripe", "id": "stripes", "angle": 45, "gap": 16, "lineWidth": 3,
                 "fill": "#112233FF", "radius": 12, "inset": [6, 6, 6, 6] })",
            error);
        check(appearance != nullptr, "stripe 那一段 JSON 能编译");
        if (appearance != nullptr)
        {
            const AppearanceLayer* layer = layerOf(*appearance, "stripes");
            check(layer != nullptr, "按 id 找得到这一层");
            if (layer != nullptr)
            {
                checkEqualInt((int)layer->paramCount, 3, "三个声明参数都在");
                checkFloat(layer->params[0].number, 45.0f, "angle 不缩放（角度不是像素量）");
                checkFloat(layer->params[1].number, 6.0f, "lineWidth 3 × uiScale 2 = 6");
                checkFloat(layer->params[2].number, 32.0f, "gap 16 × uiScale 2 = 32");
                // 引擎字段照常解析（扩展层能直接白拿）
                checkColor(layer->fill, IM_COL32(0x11, 0x22, 0x33, 0xFF), "fill 走引擎字段");
                checkFloat(layer->radius, 24.0f, "radius 走引擎字段（也乘了缩放）");
                checkFloat(layer->inset[0], 12.0f, "inset 走引擎字段（6 × 2）");
            }
        }

        // 参数名撞上引擎会解析的键：注册成功（不致命）但会警告 —— 这里只确认它仍然可用
        LayerType collide;
        collide.name     = "collide";
        collide.idPrefix = "collide";
        collide.params.push_back(LayerParam::color("fill", 0)); // fill 引擎已经解析过
        collide.draw     = [](const LayerDrawContext&) { };
        check(registerLayer(std::move(collide), error), "参数撞引擎键：注册仍成功（只警告）");

        setUiScale(1.0f);
    }

    std::printf("\n== 8. 层 import（把别的外观的 layers 就地展开）==\n");
    {
        std::string error;
        const auto imported = compileLayers(R"({ "import": "base/button/frame" },
            { "op": "rect", "id": "extra", "fill": "#112233FF" })", error);
        check(imported != nullptr, "层 import 能编译");
        if (imported != nullptr)
        {
            check(layerOf(*imported, "bg") != nullptr, "导入的 bg 层在");
            check(layerOf(*imported, "hlBg") != nullptr, "导入的 hlBg 层在");
            check(layerOf(*imported, "extra") != nullptr, "本地层追加在后面");
            check(!imported->getBox().hasHeight, "只导入 layers：box 不会跟着进来");
            checkEqual(imported->getLayers().back().id, "extra", "顺序：导入层在前、本地层在后");
        }

        // 重复 id：导入的 bg 与本地 bg 撞名 → 必须报错（而不是默默取第一个）
        const auto dup = compileLayers(R"({ "import": "base/button/frame" },
            { "op": "rect", "id": "bg", "fill": "#112233FF" })", error);
        check(dup == nullptr, "层 import 与本地层 id 重复：编译失败");
        check(error.find("重复") != std::string::npos, "错误信息说明 id 重复");

        // 不存在的层 bundle
        const auto missing = compileLayers(R"({ "import": "base/nope_not_here" },
            { "op": "rect", "id": "x" })", error);
        check(missing == nullptr, "不存在的层 import：编译失败");
        check(error.find("不存在") != std::string::npos, "错误信息说明外观不存在");

        // 导入的层在 states 里可以用 id 控制（展开后就是普通层）
        const auto withState = compileLayers(R"({ "import": "base/button/frame" },
            { "op": "rect", "id": "extra", "fill": "#112233FF" })", error);
        check(withState != nullptr, "带层 import 的外观能编译");
        if (withState != nullptr)
        {
            std::string stateError;
            const auto stated = Appearance::compile(R"({
                "layers": [
                    { "import": "base/button/frame" },
                    { "op": "rect", "id": "extra", "fill": "#112233FF" }
                ],
                "states": {
                    "focused": { "bg.fill": { "to": "#FF0000", "duration": 120 } },
                    "pressed": { "extra.opacity": { "to": 0.5, "duration": 80 } }
                }
            })", getTheme(), stateError, "test-layer-import-state");
            check(stated != nullptr, "states 能引用导入层的 id");
            if (stated != nullptr)
            {
                checkEqualInt((int)stated->getTransitions("focused").size(), 1,
                    "focused 落在导入的 bg 层上");
                checkEqualInt((int)stated->getTransitions("pressed").size(), 1,
                    "pressed 落在本地 extra 层上");
            }
        }
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

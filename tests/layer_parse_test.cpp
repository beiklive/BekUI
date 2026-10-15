// 无窗口测试：**图层解析**的行为基线（6 种 op 的键与报错）。
//
// 为什么先写这个：下一步要把 `parseLayers`（382 行）按 op 拆开、并把字段解析迁进
// `compile/layers/<op>.cpp`。那片代码目前是"按字段统一驱动"的，拆之前必须有行为基线，
// 否则拆完只能靠肉眼对比。这些断言就是那道保险。

#include <cstdio>
#include <string>

#include "bklv/compile/layer_registry.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"

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

} // namespace

int main()
{
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
        const auto text = compileLayers(R"({ "op": "text", "id": "title", "source": "text",
            "color": "#FFFFFFFF", "fontSize": 20, "align": "left" })", error);
        check(text != nullptr, "text 能编译");
        if (text != nullptr)
        {
            const AppearanceLayer* l = layerOf(*text, "title");
            check(l != nullptr && l->op == AppearanceLayer::Op::TEXT, "op 是 text");
            if (l != nullptr)
                check(l->source == "text", "source 落在 source 上");
        }

        error.clear();
        const auto image = compileLayers(R"({ "op": "image", "id": "pic", "source": "image",
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
            { "op": "text", "source": "text" })", error);
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

        // 层序档位：引擎排的是 阴影 -> 玻璃 -> 底色
        check(layerOrderRank(LayerOrder::SHADOW) < layerOrderRank(LayerOrder::GLASS),
            "阴影在玻璃之前");
        check(layerOrderRank(LayerOrder::GLASS) < layerOrderRank(LayerOrder::BACKGROUND),
            "玻璃在底色之前");

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
        }

        error.clear();
        check(compileLayers(R"({ "op": "没注册过的类型" })", error) == nullptr,
            "没注册过的 op 仍然报错（不静默放过）");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

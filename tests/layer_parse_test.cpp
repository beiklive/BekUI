// 无窗口测试：**图层解析**的行为基线（6 种 op 的键与报错）。
//
// 为什么先写这个：下一步要把 `parseLayers`（382 行）按 op 拆开、并把字段解析迁进
// `compile/layers/<op>.cpp`。那片代码目前是"按字段统一驱动"的，拆之前必须有行为基线，
// 否则拆完只能靠肉眼对比。这些断言就是那道保险。

#include <cstdio>
#include <string>

#include "bklv/core/appearance.hpp"
#include "bklv/core/theme_data.hpp"

using namespace bklv;

#include "test_util.hpp"

namespace
{

std::shared_ptr<const Appearance> compile(const std::string& layers, std::string& error)
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

        const auto rect = compile(R"({ "op": "rect", "id": "bg", "fill": "#112233FF",
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
        const auto text = compile(R"({ "op": "text", "id": "title", "source": "text",
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
        const auto image = compile(R"({ "op": "image", "id": "pic", "source": "image",
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
        const auto shadow = compile(R"({ "op": "shadow", "id": "shadow",
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
        const auto glass = compile(R"({ "op": "glass", "id": "glass", "fill": "#FFFFFF33",
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
        const auto custom = compile(R"({ "op": "custom", "id": "body", "fill": "#00000000" })", error);
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
        const auto appearance = compile(R"({ "op": "rect", "fill": "#00000000" },
            { "op": "text", "source": "text" })", error);
        check(appearance != nullptr, "不写 id 也能编译");
        if (appearance != nullptr)
        {
            // 实测：默认 id = 「映射表里的名字 + 循环下标（从 0 起）」
            check(appearance->indexOfLayer("rect0") >= 0, "第一层 rect 的默认 id 是 rect0");
            check(appearance->indexOfLayer("text1") >= 0, "第二层 text 的默认 id 是 text1");
        }
    }

    std::printf("\n== 2b. 已知 quirk：shadow / glass 没写 id 时会被叫成 custom<N> ==\n");
    {
        std::string error;
        const auto appearance = compile(R"({ "op": "shadow", "fill": "#00000040" },
            { "op": "glass", "fill": "#FFFFFF33" })", error);
        check(appearance != nullptr, "shadow/glass 不写 id 也能编译");
        if (appearance != nullptr)
        {
            // 这张默认 id 映射表只有 rect/text/image，shadow/glass 落到 "custom" 分支。
            // 重构（①c 注册表）会让每种 op 自带默认 id 前缀，这个 quirk 会一并修掉 ——
            // 现在先把它**钉成基线**，修的时候就能看到行为变化是有意的。
            check(appearance->indexOfLayer("custom0") >= 0, "shadow 的默认 id 现在是 custom0（待修）");
            check(appearance->indexOfLayer("custom1") >= 0, "glass 的默认 id 现在是 custom1（待修）");
        }
    }

    std::printf("\n== 3. 写错要报错（不静默降级）==\n");
    {
        std::string error;
        check(compile(R"({ "op": "不存在的类型" })", error) == nullptr, "op 不认识 -> 失败");
        check(!error.empty(), "错误串非空");

        error.clear();
        check(compile(R"({ "op": "rect", "radius": "abc" })", error) == nullptr, "radius 类型不对 -> 失败");

        error.clear();
        check(compile(R"({ "op": "rect", "fill": "@theme/color.不存在.键" })", error) == nullptr,
            "颜色引用不存在的键 -> 失败");

        error.clear();
        check(compile(R"("不是对象")", error) == nullptr, "层不是对象 -> 失败");
    }

    std::printf("\n%d 项检查，%d 项失败\n", g_checks, g_failed);
    return g_failed > 0 ? 1 : 0;
}

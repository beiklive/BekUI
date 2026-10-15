#include "bklv/compile/layer_registry.hpp"

#include <algorithm>

#include "bklv/core/logger.hpp"

namespace bklv
{
namespace compile
{

namespace
{

std::vector<LayerType>& registry()
{
    static std::vector<LayerType> layers;
    return layers;
}

// 内置层惰性注册一次：调用方不用关心初始化顺序，
// 第三方仍然可以在自己的初始化里提前 registerLayer（重复会报错，不会互相覆盖）
void ensureBuiltins()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    registerBuiltinLayers();
}

} // namespace

bool registerLayer(LayerType type, std::string& error)
{
    if (type.name.empty())
    {
        error = "图层类型的 name 不能为空";
        return false;
    }
    if (type.idPrefix.empty())
    {
        error = "图层类型 \"" + type.name + "\" 的 idPrefix 不能为空（没写 id 时要用它生成默认 id）";
        return false;
    }
    if (findLayer(type.name) != nullptr)
    {
        error = "图层类型 \"" + type.name + "\" 重复注册";
        return false;
    }

    registry().push_back(std::move(type));
    return true;
}

const LayerType* findLayer(const std::string& name)
{
    ensureBuiltins();
    const std::vector<LayerType>& layers = registry();
    auto it = std::find_if(layers.begin(), layers.end(),
        [&name](const LayerType& type) { return type.name == name; });
    return it == layers.end() ? nullptr : &(*it);
}

const std::vector<LayerType>& registeredLayers()
{
    ensureBuiltins();
    return registry();
}

void clearLayersForTest()
{
    registry().clear();
}

int layerOrderRank(LayerOrder order)
{
    // 与引擎解析期的排序一致：阴影最先，玻璃紧随其后，其余按档位
    switch (order)
    {
        case LayerOrder::SHADOW: return 0;
        case LayerOrder::GLASS: return 1;
        case LayerOrder::BACKGROUND: return 2;
        case LayerOrder::HIGHLIGHT_BG: return 3;
        case LayerOrder::CONTENT: return 4;
        case LayerOrder::DECORATION: return 5;
        case LayerOrder::OVERLAY: return 6;
        default: return 5;
    }
}

void registerBuiltinLayers()
{
    std::string error;
    auto add = [&error](LayerType type) {
        if (!registerLayer(std::move(type), error))
            BKLV_LOG_ERROR("内置图层注册失败：%s", error.c_str());
    };

    // 用工厂而不是聚合初始化：钩子字段保持默认空值，也不会踩 -Wmissing-field-initializers
    auto make = [](const char* name, const char* label, const char* idPrefix, LayerOrder order,
                    const char* help, std::vector<std::string> keys) {
        LayerType type;
        type.name     = name;
        type.label    = label;
        type.idPrefix = idPrefix;
        type.order    = order;
        type.help     = help;
        type.keys     = std::move(keys);
        return type; // parse / draw 留空：①c-3 才接上
    };

    add(make("rect", "矩形", "rect", LayerOrder::BACKGROUND,
        "矩形：填充/描边/圆角/内缩，可带 gradient（渐变）",
        { "fill", "stroke", "strokeWidth", "radius", "opacity", "inset", "width", "height",
            "align", "valign", "gradient" }));
    add(make("text", "文字", "text", LayerOrder::CONTENT,
        "文字：从内容槽取字（source: text/icon/info），可指定字体、颜色、对齐、内缩",
        { "source", "color", "fontSize", "align", "valign", "inset", "width", "height", "opacity" }));
    add(make("image", "图片", "image", LayerOrder::CONTENT,
        "图片：source: image + scaling(fit/fill/crop...)",
        { "source", "scaling", "tint", "align", "valign", "inset", "width", "height", "radius",
            "opacity" }));
    add(make("shadow", "阴影", "shadow", LayerOrder::SHADOW,
        "柔化投影：fill 浓度 / feather 羽化 / spread 外扩 / offset 位移 / cutout 挖空",
        { "fill", "feather", "spread", "offset", "radius", "cutout", "inset", "width", "height",
            "opacity" }));
    add(make("glass", "玻璃", "glass", LayerOrder::GLASS,
        "玻璃：fill 是染色（也是后端不可用时的降级底），stroke/strokeWidth 是内缘高光",
        { "fill", "stroke", "strokeWidth", "radius", "opacity", "inset", "width", "height" }));
    add(make("custom", "自定义", "custom", LayerOrder::CONTENT,
        "自定义：语义绘制在 C++ 里（控件的 drawCustomLayer），外观只给几何与底色",
        { "fill", "inset", "width", "height", "opacity" }));
}

} // namespace compile
} // namespace bklv

#include "bklv/compile/appearance_layer_fields.hpp"

#include "bklv/core/attributes.hpp"

namespace bklv
{
namespace compile
{

bool parseLayerFields(const Json& node, const ThemeData& theme, AppearanceLayer& layer,
    std::string& error)
{
    // 原来这段内联在图层循环里，用了循环里的 where；抽出来后自己提供一个
    const std::string where = "layer";

    // 颜色 / 数值字段
    struct ColorField
    {
        const char* key;
        ImU32* target;
    };
    const ColorField colorFields[] = {
        { "fill", &layer.fill },
        { "stroke", &layer.stroke },
        { "color", &layer.textColor },
        { "textColor", &layer.textColor },
    };
    for (const ColorField& field : colorFields)
    {
        if (!node.contains(field.key))
            continue;
        if (!resolveColor(node[field.key], theme, *field.target, error, where + "." + field.key))
            return false;
    }

    struct NumberField
    {
        const char* key;
        float* target;
        bool pixels; // 像素量要跟随 720p 缩放；opacity 是系数不缩放
    };
    const NumberField numberFields[] = {
        { "strokeWidth", &layer.strokeWidth, true },
        { "radius", &layer.radius, true },
        { "opacity", &layer.opacity, false },
        { "fontSize", &layer.fontSize, true },
        { "width", &layer.width, true },
        { "height", &layer.height, true },
        { "feather", &layer.feather, true },
        { "spread", &layer.spread, true },
    };
    if (node.contains("cutout") && node["cutout"].is_boolean())
        layer.cutout = node["cutout"].get<bool>();

    for (const NumberField& field : numberFields)
    {
        if (!node.contains(field.key))
            continue;
        if (!resolveNumber(node[field.key], theme, *field.target, error, where + "." + field.key))
            return false;
        if (field.pixels)
            *field.target = scalePixels(*field.target);
    }

    if (node.contains("inset") && node["inset"].is_array() && node["inset"].size() == 4)
        for (size_t k = 0; k < 4; k++)
        {
            if (!resolveNumber(node["inset"][k], theme, layer.inset[k], error,
                    where + ".inset[" + std::to_string(k) + "]"))
                return false;
            layer.inset[k] = scalePixels(layer.inset[k]);
        }

    if (node.contains("offset") && node["offset"].is_array() && node["offset"].size() == 2)
        for (size_t k = 0; k < 2; k++)
        {
            if (!resolveNumber(node["offset"][k], theme, layer.offset[k], error,
                    where + ".offset[" + std::to_string(k) + "]"))
                return false;
            layer.offset[k] = scalePixels(layer.offset[k]);
        }

    // RECT 层的渐变：交给 compile/appearance_gradient.cpp（校验从严，见那边注释）

    return true;
}

} // namespace compile
} // namespace bklv

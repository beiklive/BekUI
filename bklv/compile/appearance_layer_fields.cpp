#include "bklv/compile/appearance_layer_fields.hpp"

#include "bklv/core/attributes.hpp"

namespace bklv
{
namespace compile
{

namespace
{

// 候选值拼成 "a|b|c"：报错与警告里都要用（和手写枚举字段的提示格式一致）
std::string joinEnumValues(const std::vector<std::string>& values)
{
    std::string joined;
    for (size_t i = 0; i < values.size(); i++)
        joined += (i == 0 ? "" : "|") + values[i];
    return joined;
}

} // namespace

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
    if (node.contains("inner") && node["inner"].is_boolean())
        layer.inner = node["inner"].get<bool>();

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

// 扩展层自己声明的参数：按模式解析。五种类型全自动，第三方零代码。
bool parseDeclaredParams(const Json& node, const LayerType& type, const ThemeData& theme,
    AppearanceLayer& layer, std::string& error, const std::string& where)
{
    if (type.params.empty())
        return true; // 内置 6 种：字段是 AppearanceLayer 上的具名成员，不走参数槽

    layer.paramCount = (uint8_t)type.params.size();

    for (size_t i = 0; i < type.params.size(); i++)
    {
        const LayerParam& param   = type.params[i];
        LayerValue& out           = layer.params[i];
        const std::string at      = where + "." + param.name;

        // JSON 里没写：用模式里的默认值（**不是 0** —— 没写和写 0 是两件事）
        if (!node.contains(param.name))
        {
            switch (param.type)
            {
                case LayerParamType::PIXELS:
                    out.number = scalePixels(param.numberDefault);
                    break;
                case LayerParamType::NUMBER:
                case LayerParamType::ENUM: // ENUM 默认值是候选下标
                    out.number = param.numberDefault;
                    break;
                case LayerParamType::COLOR: out.color = param.colorDefault; break;
                case LayerParamType::BOOL: out.boolean = param.boolDefault; break;
            }
            continue;
        }

        const Json& value = node[param.name];
        switch (param.type)
        {
            case LayerParamType::NUMBER:
                if (!resolveNumber(value, theme, out.number, error, at))
                    return false;
                break;

            case LayerParamType::PIXELS:
                if (!resolveNumber(value, theme, out.number, error, at))
                    return false;
                out.number = scalePixels(out.number);
                break;

            case LayerParamType::COLOR:
                if (!resolveColor(value, theme, out.color, error, at))
                    return false;
                break;

            case LayerParamType::BOOL:
                if (!value.is_boolean())
                {
                    error = at + " 需要 true / false";
                    return false;
                }
                out.boolean = value.get<bool>();
                break;

            case LayerParamType::ENUM:
            {
                if (!value.is_string())
                {
                    error = at + " 需要字符串（可选值：" + joinEnumValues(param.enumValues) + "）";
                    return false;
                }
                const std::string text = value.get<std::string>();
                int matched            = -1;
                for (size_t k = 0; k < param.enumValues.size(); k++)
                {
                    if (enumMatches(text, param.enumValues[k].c_str()))
                    {
                        matched = (int)k;
                        break;
                    }
                }
                if (matched < 0)
                {
                    // 拼错只警告、保持默认值（和外观 JSON 里其它枚举字段一致，不打断编译）
                    warnUnknownEnumValue(at.c_str(), text,
                        joinEnumValues(param.enumValues).c_str());
                    out.number = param.numberDefault;
                }
                else
                    out.number = (float)matched;
                break;
            }
        }
    }

    return true;
}

} // namespace compile
} // namespace bklv

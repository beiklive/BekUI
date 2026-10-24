#include "bklv/theme/appearance_gradient.hpp"

#include "bklv/theme/attributes.hpp"
#include "bklv/paint/gradient.hpp" // Gradient

namespace bklv
{
namespace compile
{

bool parseGradient(const Json& node, const ThemeData& theme, AppearanceLayer& layer,
    std::string& error)
{
    // 原来这段是内联在图层循环里的，用了循环里那个 where；抽出来后自己提供一个
    // （错误串因此带上 "gradient." 前缀，和原来的粒度一致）
    const std::string where = "gradient";
if (node.contains("gradient"))
{
    const Json& gradientNode = node["gradient"];
    const std::string gradientWhere = where + ".gradient";
    if (!gradientNode.is_object())
    {
        error = gradientWhere + " 要写成对象：{ type, angle, center, stops }";
        return false;
    }

    Gradient parsed;
    const std::string typeName
        = gradientNode.value("type", std::string("linear"));
    if (!gradientTypeFromName(typeName, parsed.type))
    {
        error = gradientWhere + ".type 只支持 linear / radial（当前：" + typeName + "）";
        return false;
    }

    // angle / center / at 都是**无量纲**，不跟随 720p 缩放
    if (gradientNode.contains("angle")
        && !resolveNumber(gradientNode["angle"], theme, parsed.angle, error,
            gradientWhere + ".angle"))
        return false;

    if (gradientNode.contains("center"))
    {
        const Json& centerNode = gradientNode["center"];
        if (!centerNode.is_array() || centerNode.size() != 2)
        {
            error = gradientWhere + ".center 要写成 [x, y]（0..1 相对层矩形）";
            return false;
        }
        for (size_t k = 0; k < 2; k++)
        {
            float value = 0.0f;
            if (!resolveNumber(centerNode[k], theme, value, error,
                    gradientWhere + ".center[" + std::to_string(k) + "]"))
                return false;
            if (value < 0.0f || value > 1.0f)
            {
                error = gradientWhere + ".center[" + std::to_string(k)
                    + "] 要在 0..1 之内（当前：" + std::to_string(value) + "）";
                return false;
            }
            parsed.center[k] = value;
        }
    }

    if (!gradientNode.contains("stops") || !gradientNode["stops"].is_array())
    {
        error = gradientWhere + ".stops 要写成数组：[{ \"at\": 0, \"color\": \"#RRGGBBAA\" }, ...]";
        return false;
    }

    const Json& stopsNode = gradientNode["stops"];
    if (stopsNode.size() < 2 || (int)stopsNode.size() > kMaxGradientStops)
    {
        error = gradientWhere + ".stops 要 2~" + std::to_string(kMaxGradientStops)
            + " 个（当前 " + std::to_string(stopsNode.size()) + " 个）";
        return false;
    }

    float previousAt = -1.0f;
    for (size_t i = 0; i < stopsNode.size(); i++)
    {
        const Json& stopNode  = stopsNode[i];
        const std::string stopWhere
            = gradientWhere + ".stops[" + std::to_string(i) + "]";
        if (!stopNode.is_object())
        {
            error = stopWhere + " 要写成对象 { at, color }";
            return false;
        }

        GradientStop stop;
        if (stopNode.contains("at")
            && !resolveNumber(stopNode["at"], theme, stop.at, error, stopWhere + ".at"))
            return false;
        if (stop.at < 0.0f || stop.at > 1.0f)
        {
            error = stopWhere + ".at 要在 0..1 之内（当前：" + std::to_string(stop.at) + "）";
            return false;
        }
        if (stop.at <= previousAt)
        {
            error = stopWhere + ".at 必须比上一个停靠点大（停在 "
                + std::to_string(previousAt) + "，这个是 " + std::to_string(stop.at) + "）";
            return false;
        }
        previousAt = stop.at;

        if (!stopNode.contains("color"))
        {
            error = stopWhere + " 缺少 color";
            return false;
        }
        if (!resolveColor(stopNode["color"], theme, stop.color, error, stopWhere + ".color"))
            return false;

        parsed.stops[i] = stop;
    }
    parsed.stopCount = (int)stopsNode.size();
    layer.gradient   = parsed;
}

    return true;
}

} // namespace compile
} // namespace bklv

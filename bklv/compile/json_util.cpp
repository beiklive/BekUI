#include "bklv/compile/json_util.hpp"

#include "bklv/core/logger.hpp"

#include <cmath>
#include <cstdlib>

namespace bklv
{
namespace compile
{

float scalePixels(float value)
{
    return value * getUiScale();
}

std::string normalizeThemeKey(const std::string& key, const std::string& where)
{
    const size_t slash = key.find('/');
    if (slash == std::string::npos)
        return key;

    std::string normalized = key;
    normalized[slash]      = '.';

    BKLV_LOG_WARNING("主题引用写法应带点：@theme/%s（%s）；已按 @theme/%s 解析", key.c_str(),
        where.c_str(), normalized.c_str());
    return normalized;
}

bool parseHex(const std::string& text, ImU32& out)
{
    if (text.size() != 7 && text.size() != 9)
        return false;
    if (text[0] != '#')
        return false;
    for (size_t i = 1; i < text.size(); i++)
        if (!std::isxdigit((unsigned char)text[i]))
            return false;

    const unsigned long value = std::strtoul(text.c_str() + 1, nullptr, 16);
    if (text.size() == 7)
        out = IM_COL32((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 255);
    else
        out = IM_COL32((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    return true;
}

void splitColor(ImU32 color, float out[4])
{
    out[0] = (float)(color & 0xFF);
    out[1] = (float)((color >> 8) & 0xFF);
    out[2] = (float)((color >> 16) & 0xFF);
    out[3] = (float)((color >> 24) & 0xFF);
}

ImU32 joinColor(const float in[4])
{
    auto clamp255 = [](float v) {
        return (unsigned int)std::max(0.0f, std::min(255.0f, std::round(v)));
    };
    return IM_COL32(clamp255(in[0]), clamp255(in[1]), clamp255(in[2]), clamp255(in[3]));
}

bool resolveColor(const Json& node, const ThemeData& theme, ImU32& out, std::string& error,
    const std::string& where)
{
    if (!node.is_string())
    {
        error = where + " 期望颜色字符串（\"#RRGGBB\" 或 \"@theme/color.x.y\"）";
        return false;
    }

    const std::string text = node.get<std::string>();
    if (text.rfind("@theme/", 0) == 0)
    {
        const std::string key = normalizeThemeKey(text.substr(7), where);
        if (!theme.hasColor(key))
        {
            // 不静默回退：写错 key 会变成一块洋红，比报错难查得多
            error = where + " 引用了主题里不存在的颜色：@theme/" + key
                + "（当前主题 " + theme.getName() + "；写法是 @theme/color.a.b）";
            return false;
        }
        out = theme.color(key);
        return true;
    }
    if (parseHex(text, out))
        return true;

    error = where + " 不是合法颜色：" + text;
    return false;
}

bool resolveNumber(const Json& node, const ThemeData& theme, float& out, std::string& error,
    const std::string& where)
{
    if (node.is_number())
    {
        out = node.get<float>();
        return true;
    }
    if (node.is_string())
    {
        const std::string text = node.get<std::string>();
        if (text.rfind("@theme/", 0) == 0)
        {
            const std::string key = normalizeThemeKey(text.substr(7), where);
            if (!theme.hasNumber(key))
            {
                error = where + " 引用了主题里不存在的数值：@theme/" + key
                    + "（当前主题 " + theme.getName() + "；写法是 @theme/metric.a.b）";
                return false;
            }
            out = theme.number(key);
            return true;
        }
        char* end = nullptr;
        const double value = std::strtod(text.c_str(), &end);
        if (end != text.c_str() && *end == '\0')
        {
            out = (float)value;
            return true;
        }
    }

    error = where + " 期望数值（数字或 \"@theme/metric.x.y\"）";
    return false;
}

} // namespace compile
} // namespace bklv

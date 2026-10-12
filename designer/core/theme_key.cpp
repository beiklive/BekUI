#include "designer/core/theme_key.hpp"

namespace designer
{

std::string themeKeyFromReference(const std::string& value)
{
    static const std::string prefix = "@theme/";
    if (value.rfind(prefix, 0) != 0)
        return std::string();

    std::string key = value.substr(prefix.size());
    if (key.empty())
        return std::string();

    // 容错：写成 @theme/color/a.b（斜杠）也认
    for (char& c : key)
        if (c == '/')
            c = '.';

    if (key.rfind("color.", 0) != 0 && key.rfind("metric.", 0) != 0)
        key = "color." + key; // 省前缀的写法默认是颜色

    return key;
}

} // namespace designer

#include "bklv/core/theme.hpp"

#include "bklv/core/logger.hpp"

namespace bklv
{

Theme::Theme()
{
    // 深色主题，取自 borealis 的默认配色并做了删减。
    this->colors["bklv/background"]      = rgb(26, 28, 33);
    this->colors["bklv/text"]            = rgb(235, 238, 242);
    this->colors["bklv/text_disabled"]   = rgb(130, 137, 148);
    this->colors["bklv/text_hint"]       = rgb(150, 160, 175);
    this->colors["bklv/highlight"]       = rgb(74, 144, 226);
    this->colors["bklv/clear"]           = rgb(26, 28, 33);
    this->colors["bklv/list/listItem"]   = rgb(45, 49, 56);
    this->colors["bklv/list/separator"]  = rgb(70, 76, 86);
    this->colors["bklv/rectangle"]       = rgb(57, 96, 145);
    this->colors["bklv/border"]          = rgb(90, 98, 110);
    this->colors["bklv/accent"]          = rgb(90, 170, 245);
    this->colors["bklv/card/blue"]       = rgb(57, 96, 145);
    this->colors["bklv/card/green"]      = rgb(70, 125, 100);
    this->colors["bklv/card/orange"]     = rgb(145, 94, 67);
    this->colors["bklv/card/purple"]     = rgb(105, 84, 150);
}

ImU32 Theme::getColor(const std::string& name) const
{
    auto it = this->colors.find(name);
    if (it == this->colors.end())
    {
        Logger::warning("Unknown theme color: " + name);
        return IM_COL32(255, 0, 255, 255); // 洋红代表缺色
    }
    return it->second;
}

void Theme::setColor(const std::string& name, ImU32 color)
{
    this->colors[name] = color;
}

Theme& getDefaultTheme()
{
    static Theme theme;
    return theme;
}

Style::Style()
{
    // 字号与行高，对应 borealis 的 bklv/label/*。
    this->numbers["bklv/label/default_font_size"]   = 20.0f;
    this->numbers["bklv/label/default_line_height"] = 1.35f;
    this->numbers["bklv/label/single_line_height"]  = 22.0f;

    // 内边距默认值，对应 borealis 的 bklv/box/*。
    this->numbers["bklv/box/default_padding"] = 0.0f;

    // 动画时长（秒），对应 borealis 的 bklv/animations/*。
    this->numbers["bklv/animations/highlight"] = 0.15f;
    this->numbers["bklv/animations/click"]     = 0.10f;

    // 矩形默认圆角。
    this->numbers["bklv/rectangle/corner_radius"] = 6.0f;
}

float Style::getNumber(const std::string& name) const
{
    auto it = this->numbers.find(name);
    if (it == this->numbers.end())
    {
        Logger::warning("Unknown style key: " + name);
        return 0.0f;
    }
    return it->second;
}

void Style::setNumber(const std::string& name, float value)
{
    this->numbers[name] = value;
}

Style& getDefaultStyle()
{
    static Style style;
    return style;
}

} // namespace bklv

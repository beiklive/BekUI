#pragma once

// 简化的主题：只保留颜色表，对应 borealis 的 core/theme.hpp。
// borealis 支持主题文件叠加覆盖，学习项目里改成一张固定表 + 运行时改色。

#include <string>
#include <unordered_map>

#include "bklv/core/geometry.hpp"

namespace bklv
{

class Theme
{
  public:
    Theme();

    // 取颜色；键不存在时返回洋红色以便立刻发现拼写错误。
    ImU32 getColor(const std::string& name) const;

    void setColor(const std::string& name, ImU32 color);

    // 简化版索引访问：theme["bklv/text"]。
    ImU32 operator[](const std::string& name) const { return this->getColor(name); }

  private:
    std::unordered_map<std::string, ImU32> colors;
};

// 全局默认主题，等价于 borealis 的 Application::getTheme()。
Theme& getDefaultTheme();

// 样式：数值型配置（字号、行高、动画时长等），对应 borealis 的 core/style.hpp。
class Style
{
  public:
    Style();

    float getNumber(const std::string& name) const;
    void setNumber(const std::string& name, float value);

    float operator[](const std::string& name) const { return this->getNumber(name); }

  private:
    std::unordered_map<std::string, float> numbers;
};

Style& getDefaultStyle();

} // namespace bklv

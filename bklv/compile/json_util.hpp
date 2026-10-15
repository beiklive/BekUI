#pragma once

// JSON 解析的共享工具（主题 / 视图 / 外观 / i18n 四套解析都用它）。
//
// 为什么单独抽：这四套解析各自实现过「取数字、取颜色、取枚举、报错位置」，
// 宽容规则与错误格式要在多处保持一致 —— 项目里已经因此踩过坑
//（`@theme/metric` 与 `@style/metric` 混淆）。所以取值与报错只在这里写一遍，
// 错误串统一带 key 路径（`where` 参数）。
//
// 这里**只做「JSON 值 → C++ 值 + 错误串」**，不含运行时、不含绘制。

#include <string>

#include <nlohmann/json.hpp>
#include "bklv/core/theme_data.hpp"
#include "bklv/core/ui_scale.hpp"

namespace bklv
{

// 项目里各解析 TU 原本各自写一遍这个别名；从这份共享头开始收敛到这里
using Json = nlohmann::json;

namespace compile
{

// 按 720p 的 UI 缩放换算像素量
float scalePixels(float value);

// `"#RRGGBB"` / `"#RRGGBBAA"`
bool parseHex(const std::string& text, ImU32& out);

// 颜色按 0..255 的 4 通道拆合（AppearanceInstance 内部就是这套标度）
void splitColor(ImU32 color, float out[4]);
ImU32 joinColor(const float in[4]);

// 颜色项：`"#RRGGBB"` 或 `"@theme/color.x.y"`；失败写 error（含 where 路径）返回 false
bool resolveColor(const Json& node, const ThemeData& theme, ImU32& out, std::string& error,
    const std::string& where);

// 数字项：数字，或 `@theme/metric.a.b` 这类引用；颜色与 opacity 不缩放
bool resolveNumber(const Json& node, const ThemeData& theme, float& out, std::string& error,
    const std::string& where);

} // namespace compile
} // namespace bklv

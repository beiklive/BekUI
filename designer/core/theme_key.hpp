#pragma once

// 主题引用的归一：`@theme/color.a.b` / `@theme/a.b`（省 color. 的写法）/ `@theme/metric.x`
// 统一成**装载进主题表里的点分全名**（`color.a.b` / `metric.x`）。
//
// 为什么单独做：主题加载时会把 colors.json 展平成带 `color.` 前缀的键，
// 而外观里可以省掉这个前缀写 `@theme/accent.base`。编辑器里各处的解析如果各写一套，
// 就很容易漏前缀——查一个不存在的键，值悄悄退化成 fallback（踩过：色板与对比度检查

// 都因此失效，还被"主题颜色缺失"的警告照出来）。
//
// 不是 @theme 引用时返回空串。
#include <string>

namespace designer
{

std::string themeKeyFromReference(const std::string& value);

} // namespace designer

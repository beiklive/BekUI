#pragma once

// demo 展示页的共用骨架：
//
//   [section 名称]
//   ┌──────────────────────────────────────────────┐
//   │  左边：居中放演示控件     右边：滚动看 JSON 源码 │
//   └──────────────────────────────────────────────┘
//
// 演示控件不是基础控件，外观放在 `resources/theme/default/demo/` 下。

#include <string>

#include "bklv/bklv.hpp"

namespace demo
{

// 读主题里的 JSON 原文（相对 resources/theme/default/，如 "demo/pill.json"）
std::string readThemeJson(const std::string& relativePath);

// 一个展示区块：标题 + 白底盒（左演示 / 右 JSON 滚动区）
bklv::Box* makeShowcaseSection(const std::string& title, bklv::View* demo,
    const std::string& jsonText);

// 纯色 rect（用 Box 的 backgroundColor 表达，专门用来演示布局）
bklv::Box* makeColorRect(ImU32 color, float width, float height, float radius = 8.0f);

} // namespace demo

#pragma once

// JSON 差异：保存前给用户看"哪个键从什么变成什么"。
// 逐叶子比较（数组按下标），所以 layers[2].fill 这种路径能直接指到属性表的哪一行。

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace designer
{

struct JsonChange
{
    std::string path;   // box.height / layers[1].fill / states.focused.hlBg.fill.to
    std::string before; // 原值（新增时为空）
    std::string after;  // 新值（删除时为空）
};

// 只报告真正不同的叶子；两边都是对象/数组时继续下钻
std::vector<JsonChange> diffJson(const nlohmann::ordered_json& before,
    const nlohmann::ordered_json& after);

// "box.height: 48 → 56" 这种一行摘要，界面直接用
std::string changeSummary(const JsonChange& change);

// 值的紧凑写法（字符串不加引号，方便看），给摘要与属性表用
std::string compactValue(const nlohmann::ordered_json& value);

} // namespace designer

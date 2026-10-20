#pragma once

// 便捷工厂：段落标题
//
// 结构就是一份外观（默认 `base/section`）：
//   text 槽 = 标题（左侧）
//   info 槽 = 右侧附加信息（数量、状态等，可选）
//
// 之所以做成函数而不是控件：它没有行为、没有额外状态，只有"建外观 + 填两个槽"。
// 想换外观就传第三个参数；不想要右侧信息就只传标题。

#include <string>

namespace bklv
{

class AppearanceView;

// title 为空时也照建（调用方可能只想要一条分隔/空段头）
// info 为空则不设置 info 槽（外观里的 info 层显示空白）
AppearanceView* makeSectionHeader(const std::string& title,
    const std::string& info = std::string(),
    const std::string& appearance = "base/section");

} // namespace bklv

#pragma once

// 图标与文字的对齐。
//
// 用户反馈过一个具体问题：图标总是比文字高一点。原因是直接拿"文字顶"当图标顶，
// 而图标通常比文字行矮，于是看着偏上。正确做法是**按行高取中**：
//
//   iconTop = textY + (lineHeight - iconSize) / 2
//
// 单独抽出来是为了能测（这条规则每处图标都要用，靠肉眼盯不住）。

namespace designer
{

float iconTopForTextLine(float textY, float lineHeight, float iconSize);

} // namespace designer

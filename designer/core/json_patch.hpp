#pragma once

// 增量写盘：只替换**变化了的顶层段**，文件其余部分逐字节保留。
//
// 为什么需要：写盘一直是整文件重写 —— 键序和缩进能保住，但用户手写的排版、
// 段与段之间的空行、注释性的空键……一律被重排。这里改成：把改动落到原文本上。
//
// 做法：把新旧两份文本各自扫成"顶层成员 → 该成员的值在原文里的字节区间"，
// 逐段比较；值变了的段用新文本里的那一段替换，其余原样保留。
// 顶层键集合变了（增/删键）就放弃（返回 false），由调用方退回整文件重写。

#include <string>

namespace designer
{

// 成功时 out 是打补丁后的全文；返回 false 表示做不到（键集合变了 / 解析失败）
bool patchTopLevel(const std::string& originalText, const std::string& updatedText,
    std::string& out, std::string& error);

} // namespace designer

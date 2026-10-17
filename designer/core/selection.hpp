#pragma once

// 设计器里"当前选中的东西"。三处界面（文件树 / 画布 / 检视器）只读这一份，
// 各自不再维护自己的"当前对象"（DESIGN.md §9 第①条修正）。

#include <string>

namespace designer
{

enum class SelectionKind
{
    NONE,
    APPEARANCE, // 外观文件里的一个层 / 状态通道
    VIEW,       // 视图描述里的一个节点
    TOKEN,      // 令牌表里的一行（pointer = /colors/accent.base 这类）
};

struct Selection
{
    SelectionKind kind = SelectionKind::NONE;
    std::string   file;    // 相对主题根的路径，如 "widget/button.text"
    std::string   pointer; // JSON Pointer，如 "/layers/2"（层）/ "/children/0"（视图节点）
    std::string   layerId; // 外观层/状态通道的 id（pointer 指向数组时用它更稳）

    bool valid() const { return this->kind != SelectionKind::NONE && !this->file.empty(); }

    bool operator==(const Selection& other) const
    {
        return this->kind == other.kind && this->file == other.file
            && this->pointer == other.pointer && this->layerId == other.layerId;
    }
    bool operator!=(const Selection& other) const { return !(*this == other); }

    static Selection none() { return {}; }
};

} // namespace designer

#pragma once

// 撤销栈：整文件快照（DESIGN.md §6.1）。同一字段的连续修改按 key + 时间窗合并，
// 所以拖一次数值拖动条只留一步。

#include <cstdint>
#include <deque>
#include <string>

namespace designer
{

class UndoStack
{
  public:
    explicit UndoStack(size_t limit = 100) : limit(limit) { }

    // 记录一次修改前的状态。coalesceKey 非空且与上一条相同、且在上次记录后 500ms 内
    // 时替换上一条（不新增）。nowMs 由调用方给（测试可注入）。
    void record(const std::string& before, const std::string& label, const std::string& coalesceKey = {},
        int64_t nowMs = 0);

    // 撤销/重做：把当前文本换出来，返回 false 表示没得撤销
    bool undo(const std::string& current, std::string& restored);
    bool redo(const std::string& current, std::string& restored);

    bool canUndo() const { return !this->undoStack.empty(); }
    bool canRedo() const { return !this->redoStack.empty(); }
    size_t undoCount() const { return this->undoStack.size(); }
    size_t redoCount() const { return this->redoStack.size(); }

    // 保存后调用：清空（避免保存后再撤销到保存前的"已保存"错觉）
    void clear();

  private:
    struct Entry
    {
        std::string text;
        std::string label;
        std::string coalesceKey;
        int64_t     timeMs = 0;
    };

    size_t              limit = 100;
    std::deque<Entry>   undoStack;
    std::deque<Entry>   redoStack;
    std::string         lastCoalesceKey;
};

} // namespace designer

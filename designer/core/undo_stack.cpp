#include "designer/core/undo_stack.hpp"

namespace designer
{

namespace
{
constexpr int64_t kCoalesceWindowMs = 500;
}

void UndoStack::record(const std::string& before, const std::string& label,
    const std::string& coalesceKey, int64_t nowMs)
{
    // 同 key、时间窗内：不新增，只把上一条的时间推后（内容仍是最早那份 before）
    if (!coalesceKey.empty() && !this->undoStack.empty()
        && this->undoStack.back().coalesceKey == coalesceKey
        && nowMs - this->undoStack.back().timeMs <= kCoalesceWindowMs)
    {
        this->undoStack.back().timeMs = nowMs;
        this->redoStack.clear();
        return;
    }

    Entry entry;
    entry.text        = before;
    entry.label       = label;
    entry.coalesceKey = coalesceKey;
    entry.timeMs      = nowMs;
    this->undoStack.push_back(std::move(entry));
    if (this->undoStack.size() > this->limit)
        this->undoStack.pop_front();
    this->redoStack.clear();
}

bool UndoStack::undo(const std::string& current, std::string& restored)
{
    if (this->undoStack.empty())
        return false;

    Entry entry = std::move(this->undoStack.back());
    this->undoStack.pop_back();

    Entry redoEntry;
    redoEntry.text        = current;
    redoEntry.label       = entry.label;
    redoEntry.coalesceKey = entry.coalesceKey;
    redoEntry.timeMs      = entry.timeMs;
    this->redoStack.push_back(std::move(redoEntry));

    restored = std::move(entry.text);
    return true;
}

bool UndoStack::redo(const std::string& current, std::string& restored)
{
    if (this->redoStack.empty())
        return false;

    Entry entry = std::move(this->redoStack.back());
    this->redoStack.pop_back();

    Entry undoEntry;
    undoEntry.text        = current;
    undoEntry.label       = entry.label;
    undoEntry.coalesceKey = entry.coalesceKey;
    undoEntry.timeMs      = entry.timeMs;
    this->undoStack.push_back(std::move(undoEntry));

    restored = std::move(entry.text);
    return true;
}

void UndoStack::clear()
{
    this->undoStack.clear();
    this->redoStack.clear();
    this->lastCoalesceKey.clear();
}

} // namespace designer

#pragma once

// 撤销/重做栈（存整份 JSON 快照）。
//
// 以前这段逻辑直接写在 UI 文件里：压栈、限制 64 步、清空重做 —— 等于没有测试覆盖。
// 抽出来之后能测"连续相同不重复入栈""撤销后重做""超过上限丢最老的"。

#include <string>
#include <vector>

namespace designer
{

class UndoStack
{
  public:
    static constexpr size_t MAX_STEPS = 64;

    // 开一份新文档：清空历史，把当前内容作为第 0 步
    void reset(const std::string& text);

    // 记一步。与栈顶相同（连续没变）时不重复入栈；入栈会清空重做
    void push(const std::string& text);

    bool canUndo() const { return this->steps.size() >= 2; }
    bool canRedo() const { return !this->redoStack.empty(); }

    // 撤销/重做：成功时把目标内容写到 out
    bool undo(std::string& out);
    bool redo(std::string& out);

    size_t size() const { return this->steps.size(); }

  private:
    std::vector<std::string> steps;
    std::vector<std::string> redoStack;
};

} // namespace designer

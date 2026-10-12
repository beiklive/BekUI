#include "designer/core/undo_stack.hpp"

namespace designer
{

void UndoStack::reset(const std::string& text)
{
    this->steps.clear();
    this->redoStack.clear();
    this->steps.push_back(text);
}

void UndoStack::push(const std::string& text)
{
    if (!this->steps.empty() && this->steps.back() == text)
        return; // 连续没变，不占步数

    this->steps.push_back(text);
    this->redoStack.clear();

    if (this->steps.size() > MAX_STEPS)
        this->steps.erase(this->steps.begin()); // 丢最老的
}

bool UndoStack::undo(std::string& out)
{
    if (!this->canUndo())
        return false;

    this->redoStack.push_back(this->steps.back());
    this->steps.pop_back();
    out = this->steps.back();
    return true;
}

bool UndoStack::redo(std::string& out)
{
    if (this->redoStack.empty())
        return false;

    const std::string text = this->redoStack.back();
    this->redoStack.pop_back();
    this->steps.push_back(text);
    out = text;
    return true;
}

} // namespace designer

#pragma once

// 文本输入弹窗（对应 borealis 的 views/edit_text_dialog）。
//
// 结构：
//   EditTextDialog（全屏容器：遮罩 + 面板）
//     ├─ 遮罩（复用 dialog_backdrop 外观）
//     └─ 面板（`edit_text_dialog` 外观）：标题层 + 输入框 + 计数层 + 按钮行
//
// 输入走哪条路，看平台：
//   · **Switch**：平台 ImeManager 弹系统软键盘，`open()` 返回 true，键盘收完回调
//   · **桌面**：没人接手，控件自己读键盘（字符队列 + Backspace/Del/方向键/回车/Esc）
//
// 文本编辑逻辑放在 TextEditModel 里（纯数据、不碰 ImGui），所以能无窗口测：
// 插入 / 退格 / 删除 / 光标移动 / 长度上限 / 数字过滤。
// 光标闪烁是绘制期算的（按时间），不进文本模型。

#include <functional>
#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/ime.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "bklv/widgets/button.hpp"

namespace bklv
{

// 输入框：外观驱动，另外要画光标（位置取决于光标在第几个字符处，属于语义绘制）
class TextField;

// 单行文本模型：光标按**字符**（不是字节）走，中文/emoji 不会被切坏
class TextEditModel
{
  public:
    void setText(const std::string& value);
    const std::string& getText() const { return this->text; }

    void setMaxLength(size_t value) { this->maxLength = value; }
    size_t getMaxLength() const { return this->maxLength; }

    // 只保留数字（配合 numeric 键盘用）
    void setNumeric(bool value) { this->numeric = value; }
    bool isNumeric() const { return this->numeric; }

    // 光标位置是"第几个字符"（0 = 最前）
    size_t getCursor() const { return this->cursor; }
    void setCursor(size_t value);

    // 返回是否真的改了内容（超长/被过滤会返回 false）
    bool insert(const std::string& utf8);
    bool backspace();
    bool deleteForward();
    void moveCursor(int delta);

    size_t getCharCount() const;

  private:
    std::vector<std::string> splitChars() const;
    size_t cursorToByte(const std::vector<std::string>& chars, size_t index) const;

    std::string text;
    size_t cursor    = 0;
    size_t maxLength = 32;
    bool numeric     = false;
};

class EditTextDialog : public Box
{
  public:
    EditTextDialog();

    void setTitle(const std::string& value);
    void setHintText(const std::string& value); // 空内容时的占位提示
    void setText(const std::string& value);
    const std::string& getText() const { return this->model.getText(); }

    void setMaxLength(size_t value);
    void setNumeric(bool value) { this->model.setNumeric(value); }

    // 用户确认时回调（取消不调）
    void setOnSubmit(std::function<void(const std::string&)> handler)
    {
        this->onSubmit = std::move(handler);
    }
    void setOnCancel(std::function<void()> handler) { this->onCancel = std::move(handler); }

    void open();
    void close();
    bool isOpen() const { return this->opened; }

    // 面板上的"确定 / 取消"按钮（想改文案就自己设）
    Button* getConfirmButton() const { return this->confirmButton; }
    Button* getCancelButton() const { return this->cancelButton; }

    // ---- 输入路径 ----
    // 桌面：update() 里自己读键盘；这里也可以直接喂字符串（测试/粘贴用）
    bool insertText(const std::string& value) { return this->model.insert(value); }
    void handleBackspace() { this->model.backspace(); }
    void handleDelete() { this->model.deleteForward(); }
    void handleCursorMove(int delta) { this->model.moveCursor(delta); }
    void handleSubmit();
    void handleCancel();

    // Switch：让平台弹软键盘（平台不接手就返回 false，退回内联编辑）
    bool openPlatformIme();

    TextEditModel& getModel() { return this->model; }
    const TextEditModel& getModel() const { return this->model; }

    void update() override;
    bool isFocusable() const override { return true; }

    std::string getClassString() const override { return "bklv::EditTextDialog"; }

  private:
    void refreshFieldText();

    TextField* field         = nullptr; // 输入框本体（外观驱动 + 光标）
    Button* confirmButton  = nullptr;
    Button* cancelButton   = nullptr;
    AppearanceView* backdrop = nullptr;
    AppearanceView* panel    = nullptr;

    TextEditModel model;
    std::string title;
    std::string hint;
    bool opened          = false;
    bool closing         = false;
    bool platformImeOpen = false;

    std::function<void(const std::string&)> onSubmit;
    std::function<void()> onCancel;
};

} // namespace bklv

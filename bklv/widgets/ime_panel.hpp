#pragma once

// 内置输入法面板（弹层）：
//   上半屏：大输入框（实时文本 + 光标；点击/触摸定位光标，LB/RB 或 ←/→ 移动）
//   下半屏：左边九宫格数字键盘 + 右边全键 QWERTY + 底部（中/英、大写、空格、取消、确定）
//
// 桌面没有系统软键盘，EditTextDialog/InputCell 通过 ImeManager 拉起它。

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/core/ime.hpp"

namespace bklv
{

class Label;
class Button;

class ImeField;

class ImePanel : public Box
{
  public:
    ImePanel(std::string initialText, int maxLength, bool numeric,
        std::function<void(bool accepted, const std::string& text)> callback);

    std::string getClassString() const override { return "bklv::ImePanel"; }

    // 测试/外部驱动
    void insertAscii(const std::string& value);
    void toggleLanguage();
    void toggleShift();
    void moveCaret(int delta);
    void setCaret(size_t charIndex);
    const std::string& getText() const { return this->text; }
    const std::string& getComposing() const { return this->composing; }
    std::vector<std::string> getCandidates() const;
    void chooseCandidate(size_t index);
    size_t getCaret() const { return this->caret; }

    void update() override;

  private:
    void buildLayout();
    void buildNumberPad(Box* parent);
    void buildFullKeyboard(Box* parent);
    void refreshCandidates();
    void refreshLabels();
    void insertText(const std::string& value);
    void backspace();
    void commitComposing();
    void confirm();
    void cancel();
    void finish(bool accepted);
    void relabelKeys();

    std::string displayText() const;
    std::string textWithComposing() const;
    size_t caretChars() const;

    std::string text;
    std::string composing;
    size_t caret      = 0; // 按"字符"计数
    int maxLength     = 32;
    bool numeric      = false;
    bool chinese      = true;
    bool shift        = false;
    float blinkTimer  = 0.0f;

    ImeField* field = nullptr;
    Label* hintLabel = nullptr;
    Box* candidateRow = nullptr;
    std::vector<Button*> candidateButtons;
    std::vector<std::pair<Button*, std::string>> letterKeys;
    std::function<void(bool, const std::string&)> callback;
};

ImeManager& getPanelImeManager();

} // namespace bklv

#pragma once

// 内置输入法面板（弹层）：全键 QWERTY + 中英切换 + 大小写 + 拼音候选。
//
// 桌面没有系统软键盘，EditTextDialog 通过 ImeManager 拉起它：
//   setImeManager(&getPanelImeManager());
// 面板挂在 App 的弹层栈上，确认/取消后 popOverlay 并回调。

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

class ImePanel : public Box
{
  public:
    ImePanel(std::string initialText, int maxLength, bool numeric,
        std::function<void(bool accepted, const std::string& text)> callback);

    std::string getClassString() const override { return "bklv::ImePanel"; }

    // 测试用
    void insertAscii(const std::string& value);
    void toggleLanguage();
    void toggleShift();
    const std::string& getText() const { return this->text; }
    const std::string& getComposing() const { return this->composing; }
    std::vector<std::string> getCandidates() const;
    void chooseCandidate(size_t index);

  private:
    void buildKeyboard();
    void refreshLabels();
    void refreshCandidates();
    void appendChar(const std::string& value);
    void backspace();
    void commitPinyin();
    void confirm();
    void cancel();
    void finish(bool accepted);
    void relabelKeys();

    std::string text;
    std::string composing; // 拼音输入中的字母
    int maxLength = 32;
    bool numeric = false;
    bool chinese = true;
    bool shift = false;
    std::vector<std::pair<Button*, std::string>> letterKeys;

    Label* textLabel = nullptr;
    Label* hintLabel = nullptr;
    Box* candidateRow = nullptr;
    Box* letterRows = nullptr;
    std::vector<std::pair<std::string, std::string>> dictionary; // pinyin -> 汉字
    std::function<void(bool, const std::string&)> callback;
};

// 把面板挂到弹层栈上的 ImeManager（桌面用；Switch 仍用系统键盘）。
ImeManager& getPanelImeManager();

} // namespace bklv

#pragma once

// 弹窗（对应 borealis 的 views/dialog）。
//
// 结构：
//   Dialog（全屏容器，绝对定位盖在最上层）
//     ├─ DialogBackdrop  外观驱动的遮罩（open 时透明度补间 0 -> 1）
//     └─ DialogPanel     外观驱动的面板（标题/正文是它外观里的文字层；底部一行按钮）
//
// 开关是**外观状态**驱动的：open / closed 两个状态，颜色与位移由主题 JSON 描述，
// tweeny 插值。所以"弹窗弹出"这件事本身也是可换肤的（时长、缓动、位移都在 JSON 里）。
//
// 打开时会 pushOverlay：输入层切模态——指针只命中弹窗内部、方向导航不会跑出去，
// 关闭后焦点自动回到打开之前的视图。

#include <functional>
#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "bklv/widgets/button.hpp"

namespace bklv
{

// 遮罩：单层 rect，open/closed 两个状态调透明度
class DialogBackdrop : public AppearanceView
{
  public:
    DialogBackdrop();
    std::string getClassString() const override { return "bklv::DialogBackdrop"; }
};

// 面板：外观里的 rect 层是面板本体，text 层是标题与正文
class DialogPanel : public AppearanceView
{
  public:
    DialogPanel();
    void setTitle(const std::string& value);
    void setText(const std::string& value);
    const std::string& getTitle() const { return this->title; }

    // 底部按钮行（Dialog 用它排按钮）
    Box* getButtonRow() const { return this->buttonRow; }

    std::string getClassString() const override { return "bklv::DialogPanel"; }

  protected:
    std::string getLayerText(const std::string& source) override;

  private:
    std::string title;
    std::string text;
    Box* buttonRow = nullptr;
};

class Dialog : public Box
{
  public:
    Dialog();

    void setTitle(const std::string& value);
    void setText(const std::string& value);

    // 加一个按钮。回调里通常调 close()。
    Button* addButton(const std::string& label, std::function<void()> onPress = nullptr);

    void open();
    void close();
    bool isOpen() const { return this->opened; }
    bool isClosing() const { return this->closing; }

    // 关闭时额外回调（比如刷新调用方状态）
    void setOnClose(std::function<void()> handler) { this->onClose = std::move(handler); }

    DialogPanel* getPanel() const { return this->panel; }
    DialogBackdrop* getBackdrop() const { return this->backdrop; }

    void update() override;

    std::string getClassString() const override { return "bklv::Dialog"; }

  private:
    DialogBackdrop* backdrop = nullptr;
    DialogPanel* panel       = nullptr;
    bool opened              = false;
    bool closing             = false;
    std::function<void()> onClose;
};

} // namespace bklv

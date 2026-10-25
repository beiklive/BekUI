#pragma once

// 对话框（对应 borealis 的 views/dialog）。
//
// 结构（结构在代码里，样式在 JSON 里）：
//   Dialog（全屏容器，绝对定位盖满，自己不吃焦点但能点）
//     ├─ backdrop  AppearanceView("base/box/dialog_backdrop")  整屏玻璃遮罩（高斯模糊 + 暗色染色）
//     └─ panel     AppearanceView("base/box/dialog")            面板卡片（阴影 / 底色 / 描边）
//         └─ body  Box(Axis::COLUMN)
//             ├─ header     Box(Axis::ROW)：图标 + 标题（都没设就 GONE）
//             ├─ scroller   ScrollBox(Axis::COLUMN)：可滚动内容区，吃掉剩余高度
//             └─ buttonRow  Box(Axis::ROW)：按钮行，默认靠右
//
// 为什么遮罩是一份外观而不是代码画的：
//   「背景高斯模糊」在 bklv 里是玻璃层（op: glass）的能力，模糊半径 / 质量由 Shell 统一决定，
//   外观只能调染色（fill）与透明度。所以遮罩就是"铺满、带 glass 层的 AppearanceView"，
//   玻璃关闭或后端不支持时会自动退化成半透明底（见 layer_draw_glass.cpp），不会整个消失。
//
// 打开 / 关闭走 App::pushOverlay / popOverlay：
//   pushOverlay 会让输入层切模态（指针只命中弹窗内部、方向导航不跑出去），
//   关闭后焦点自动回到打开之前的视图。Dialog 自己不碰输入状态。

#include <functional>
#include <string>

#include "bklv/ui/view/box.hpp"
#include "bklv/ui/widgets/appearance/appearance_view.hpp"

namespace bklv
{

class Label;
class ScrollBox;
class TextButton;

class Dialog : public Box
{
  public:
    Dialog();
    ~Dialog() override;

    // ---- 内容 ----
    // 可滚动内容区：往里 addView 任意视图（Label / Image / ListBox / 自定义）。
    Box* getContentBox() const;
    // 按钮行：默认 justifyContent = FLEX_END、gap 取 metric.dialog.buttonGap。
    Box* getButtonRow() const;
    // 顶部图标 + 标题那一行（想完全自定义表头就往这里 addView）。
    Box* getHeaderRow() const;

    // 顶部图标（图标字体字形，见 icon::glyph(...)）与标题。
    void setIcon(const std::string& glyph);
    void setTitle(const std::string& text);
    const std::string& getTitle() const { return this->title; }
    const std::string& getIcon() const { return this->icon; }

    // ---- 按钮 ----
    // 加一个对话框按钮（外观 base/button/button.dialog）。
    // 点下去先执行回调（可为空），然后**自动关闭**本对话框 —— 「取消」这种不传回调的
    // 按钮因此天然就是关掉；要在关掉前做校验，就在回调里自己做。
    TextButton* addButton(const std::string& label, std::function<void()> onPress = nullptr);
    void clearButtons();

    // ---- 开关 ----
    bool open();
    // freeSelf 为 true 时先回调、再 delete（一次性对话框用）。
    void close(bool freeSelf = false);
    bool isOpen() const { return this->opened; }
    void setOnClose(std::function<void()> handler) { this->onClose = std::move(handler); }
    // 点遮罩是否关闭（默认 true）。
    void setCloseOnBackdropClick(bool value) { this->closeOnBackdropClick = value; }

    // ---- 样式入口 ----
    AppearanceView* getPanel() const { return this->panel; }
    AppearanceView* getBackdrop() const { return this->backdrop; }
    void setPanelAppearance(const std::string& appearanceName, const std::string& variant = "");
    void setBackdropAppearance(const std::string& appearanceName, const std::string& variant = "");

    void applyTheme() override;
    // 全屏容器要能接住"点在面板外"的点击，否则点遮罩没反应。
    // 注意：可点 ≠ 可获得焦点 —— 焦点仍然只在面板内部的按钮上。
    bool isPointerTarget() const override { return true; }

    std::string getClassString() const override { return "bklv::Dialog"; }

  private:
    void setupStructure();
    void refreshHeaderMetrics();
    void updateHeaderVisibility();

    AppearanceView* backdrop = nullptr;
    AppearanceView* panel    = nullptr;
    Box* body                = nullptr;
    Box* header              = nullptr;
    Box* content             = nullptr;
    ScrollBox* scroller      = nullptr;
    Box* buttonRow           = nullptr;
    Label* iconLabel         = nullptr;
    Label* titleLabel        = nullptr;

    std::string icon;
    std::string title;
    bool opened               = false;
    bool closeOnBackdropClick = true;
    std::function<void()> onClose;
};

} // namespace bklv

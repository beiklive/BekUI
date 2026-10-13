#pragma once

// 下拉选择（对应 borealis 的 views/dropdown）。
//
// 结构：
//   Dropdown（一行：外观驱动的触发器，显示当前值）
//     └─ 点开时：DropdownPopup 作为**弹层**挂到根视图上
//          ├─ 全屏透明点击层（点外面关掉）
//          └─ 选项列表（锚在触发器正下方，选项多了可滚动）
//
// 复用 Dialog 那套弹层/模态机制：打开时输入切模态，关闭后焦点回到触发器。

#include <functional>
#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/views/scrolling_frame.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

// 一个选项（外观驱动：底 + 文字，带 selected / focused 状态）
class DropdownOption : public AppearanceView
{
  public:
    DropdownOption();

    void setLabel(const std::string& value) { this->setText(value); }
    const std::string& getLabel() const { return this->getText(); }

    void setSelected(bool value);
    bool isSelected() const { return this->selected; }

    void setOnSelect(std::function<void()> handler) { this->onSelect = std::move(handler); }
    void trigger();

    std::string getClassString() const override { return "bklv::DropdownOption"; }

  private:
    bool selected = false;
    std::function<void()> onSelect;
};

// 弹出层：全屏（接点击）+ 锚定在触发器下方的列表
// 弹层里那块"板"：锚在触发器上/下方，外观（底/圆角/边框/阴影）来自
// widgets/dropdown_popup.json。单独一个类是因为**全屏点击层**不能带底色
// （否则整屏被面板色盖住，底下界面全看不见——踩过）。
class DropdownPanel : public AppearanceView
{
  public:
    DropdownPanel()
        : AppearanceView("dropdown_popup", false)
    {
    }

    std::string getClassString() const override { return "bklv::DropdownPanel"; }
};

// 弹层：**全屏、透明**的一层（接住"点外面"），里面放锚定好的那块板
class DropdownPopup : public Box
{
  public:
    DropdownPopup();

    // 关掉自己（选完 / 点外面 / 按 A）
    void dismiss();

    // 重建选项列表
    void setOptions(const std::vector<std::string>& options, size_t selected,
        std::function<void(size_t)> onSelect);

    void setAnchor(float x, float y, float width);
    // 按可用空间定列表高度：优先下方、放不下且上方更宽裕就翻到上方，两边都不够就压缩高度
    void fitToSpace(float triggerTop, float triggerBottom, float gap, float canvasHeight,
        size_t optionCount);

    // 自己关掉时通知 Dropdown（否则 isOpen() 要等下一帧 update 才同步）
    void setOnClosed(std::function<void()> handler) { this->onClosed = std::move(handler); }

    // 那块板（测试要断它的外观）
    AppearanceView* getPanel() const { return this->anchorBox; }
    ScrollingFrame* getList() const { return this->list; }
    Box* getListContent() const { return this->listContent; }
    std::vector<DropdownOption*>& getOptions() { return this->options; }

    std::string getClassString() const override { return "bklv::DropdownPopup"; }

  private:
    DropdownPanel* anchorBox  = nullptr;
    ScrollingFrame* list      = nullptr;
    Box* listContent          = nullptr;
    std::vector<DropdownOption*> options;
    std::function<void()> onClosed;
};

class Dropdown : public Box
{
  public:
    Dropdown();

    void setTitle(const std::string& value);
    void setOptions(const std::vector<std::string>& options, size_t selected = 0);

    void setSelectedIndex(size_t index);
    size_t getSelectedIndex() const { return this->selected; }
    const std::string& getSelectedText() const;

    void setOnChange(std::function<void(size_t)> handler) { this->onChange = std::move(handler); }

    void open();
    void close();
    bool isOpen() const { return this->opened; }

    void update() override;

    // 触发器（外观驱动的那一行）
    AppearanceView* getTrigger() const { return this->trigger; }

    // 弹出层（测试与诊断用；没打开过时为 nullptr）
    DropdownPopup* getPopupForTest() const { return this->popup; }

    std::string getClassString() const override { return "bklv::Dropdown"; }
    const AttributeTable& attributes() const override;

  private:
    void refreshTriggerText();

    AppearanceView* trigger = nullptr; // 基类：外观驱动 + 文字层
    std::vector<std::string> options;
    size_t selected = 0;
    bool opened     = false;
    DropdownPopup* popup = nullptr;
    std::function<void(size_t)> onChange;
};

} // namespace bklv

#pragma once

// 单元格：列表里的一行（左侧标题 + 右侧内容区），对应 borealis 的 views/cells/*。
//
// 与 borealis 的差别（有意为之）：
// - borealis 是 6 个各写一套 XML + 绘制的类；这里共用一个基类 + 各自的外观 JSON，
//   所以换皮肤不用改 C++（和本库其它控件一套机制）。
// - 数据源与回调放在控件上（setOnChange 等），不依赖全局 Application。
//
// 结构（一份外观 JSON 描述 box + layers + states）：
//
//   Cell（外观 widgets/cell*.json）
//     ├─ titleLabel   左：标题
//     └─ rightBox     右：内容区（detail 文字 / 开关 / 单选点 / 滑块 / 箭头）
//
// 六种变体（外观名即类名对应的 JSON）：
//   DetailCell   右侧说明文字（可选箭头）
//   BooleanCell  右侧开关（自绘轨道 + 圆点，颜色在 JSON 里）
//   RadioCell    右侧单选圆点（选中时补内点）
//   SelectorCell 右侧 "‹ 值 ›"，左右键/LB/RB 换值
//   SliderCell   右侧内嵌 Slider（单元格本身不可聚焦，焦点交给滑块）
//   InputCell    右侧说明文字，A 键调起 EditTextDialog（可数字模式）

#include <functional>
#include <string>
#include <vector>

#include "bklv/views/label.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

class Slider;
class EditTextDialog;

// ---------------------------------------------------------------- 基类

class Cell : public AppearanceView
{
  public:
    explicit Cell(const std::string& appearance, const std::string& title = "");

    void setTitle(const std::string& value);
    const std::string& getTitle() const { return this->title; }

    void setDetail(const std::string& value);
    const std::string& getDetail() const { return this->detail; }
    Label* getDetailLabel() const { return this->detailLabel; }

    // 右侧内容区：想塞自己的东西就 addView 到这个 Box
    Box* getRightBox() const { return this->rightBox; }

    void setOnClick(std::function<void()> handler) { this->onClick = std::move(handler); }
    void trigger();

    // 禁用的单元格不响应动作，外观走 disabled 状态
    void setEnabled(bool enabled);
    bool isEnabled() const { return this->enabled; }

    void applyTheme() override;

    std::string getClassString() const override { return "bklv::Cell"; }
    const AttributeTable& attributes() const override;

  protected:
    // 右侧区里 说明文字 的字体（派生类可改）
    void refreshDetailStyle();

    Label* titleLabel  = nullptr;
    Box* rightBox      = nullptr;
    Label* detailLabel = nullptr;

    std::string title;
    std::string detail;
    std::function<void()> onClick;
    bool enabled = true;
};

// ---------------------------------------------------------------- 变体

// 右侧只有说明文字（可带箭头，箭头由外观 JSON 的层画）
class DetailCell : public Cell
{
  public:
    explicit DetailCell(const std::string& title = "", const std::string& detail = "");

    std::string getClassString() const override { return "bklv::DetailCell"; }
};

// 开关：A 键/点击切换，只支持开/关（三态开关不在 borealis 里，也就不做）
class BooleanCell : public Cell
{
  public:
    explicit BooleanCell(const std::string& title = "", bool value = false);

    void setValue(bool value);
    bool getValue() const { return this->value; }
    void setOnChange(std::function<void(bool)> handler) { this->onChange = std::move(handler); }

    void applyTheme() override;

    // 供测试与自定义绘制读（构造完就应该是有效颜色）
    ImU32 getTrackOnColor() const { return this->trackOn; }
    ImU32 getTrackOffColor() const { return this->trackOff; }
    ImU32 getKnobColor() const { return this->knobColor; }

    std::string getClassString() const override { return "bklv::BooleanCell"; }

  protected:
    void drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x, float y,
        float width, float height, const AppearanceInstance& appearance) override;

  private:
    bool value = false;
    // 开关的几何与配色（换肤时重新取；draw 里只读这些值）
    float switchWidth  = 52.0f;
    float switchHeight = 26.0f;
    float knobSize     = 20.0f;
    ImU32 trackOn      = 0;
    ImU32 trackOff     = 0;
    ImU32 knobColor    = 0;
    std::function<void(bool)> onChange;
};

// 单选圆点（成组的互斥由调用方管，和 borealis 一样）
class RadioCell : public Cell
{
  public:
    explicit RadioCell(const std::string& title = "", bool selected = false);

    void setSelected(bool selected);
    bool isSelected() const { return this->selected; }
    void setOnChange(std::function<void(bool)> handler) { this->onChange = std::move(handler); }

    void applyTheme() override;

    ImU32 getRingColor() const { return this->ringColor; }
    ImU32 getDotColor() const { return this->dotColor; }

    std::string getClassString() const override { return "bklv::RadioCell"; }

  protected:
    void drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x, float y,
        float width, float height, const AppearanceInstance& appearance) override;

  private:
    bool selected = false;
    float radioSize   = 22.0f;
    float ringStroke  = 2.0f;
    ImU32 ringColor   = 0;
    ImU32 dotColor    = 0;
    std::function<void(bool)> onChange;
};

// 右侧 "‹ 值 ›"：左右方向键 / LB / RB 换值，循环
class SelectorCell : public Cell
{
  public:
    explicit SelectorCell(const std::string& title = "");

    void setOptions(const std::vector<std::string>& options);
    void setSelectedIndex(int index);
    int getSelectedIndex() const { return this->selectedIndex; }
    const std::string& getSelectedText() const;

    void setOnChange(std::function<void(int, const std::string&)> handler)
    {
        this->onChange = std::move(handler);
    }

    // 下一项 / 上一项（越界循环）
    void selectNext();
    void selectPrevious();

    std::string getClassString() const override { return "bklv::SelectorCell"; }

  private:
    void refreshDetail();

    std::vector<std::string> options;
    int selectedIndex = 0;
    std::function<void(int, const std::string&)> onChange;
};

// 内嵌滑块：单元格不可聚焦，焦点给滑块（borealis 也是这么处理的）
class SliderCell : public Cell
{
  public:
    explicit SliderCell(const std::string& title = "");

    Slider* getSlider() const { return this->slider; }

    // 值域（默认 0~1）；进度 = (值 - min) / (max - min)
    void setRange(float minValue, float maxValue);
    void setValue(float value);
    float getValue() const;
    void setOnChange(std::function<void(float)> handler) { this->onChange = std::move(handler); }

    View* getDefaultFocus() override;
    // 单元格本身不聚焦：焦点给滑块（和 borealis 一样）
    bool isFocusable() const override { return false; }

    std::string getClassString() const override { return "bklv::SliderCell"; }

  private:
    Slider* slider    = nullptr;
    float minValue    = 0.0f;
    float maxValue    = 1.0f;
    std::function<void(float)> onChange;
};

// 右侧说明文字 + A 键调起输入弹窗；数字模式只收数字
class InputCell : public Cell
{
  public:
    explicit InputCell(const std::string& title = "");

    void setNumeric(bool numeric) { this->numeric = numeric; }
    bool isNumeric() const { return this->numeric; }

    // 打开输入弹窗（测试里也可以直接调 submitText 走同一条逻辑）
    void openInput();
    void submitText(const std::string& value);

    void setOnSubmit(std::function<void(const std::string&)> handler)
    {
        this->onSubmit = std::move(handler);
    }

    // 复用的弹窗（懒创建）
    EditTextDialog* getDialog();

    std::string getClassString() const override { return "bklv::InputCell"; }

  private:
    bool numeric = false;
    EditTextDialog* dialog = nullptr;
    std::function<void(const std::string&)> onSubmit;
};

} // namespace bklv

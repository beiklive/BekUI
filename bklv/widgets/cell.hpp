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
//   DetailCell   右侧说明文字（borealis 就两个 Label，没有箭头）
//   BooleanCell  **文字 开/关**（borealis 的做法）+ 切换时字号弹一下；
//                想要图形开关就 setStyle(SWITCH)（我们的额外样式）
//   RadioCell    **圆 + 对勾，只选中时显示**（borealis 的 CheckBox 画法）
//   SelectorCell 右侧当前值的**纯文本**；点击走 onOpen（demo 里弹 Dropdown），
//                另外保留了左右键/LB/RB 直接换值（我们的额外便利）
//   SliderCell   右侧内嵌 Slider（单元格本身不可聚焦，焦点交给滑块）
//   InputCell    右侧说明文字，A 键调起 EditTextDialog（可数字模式）

#include <functional>
#include <string>
#include <vector>

#include "bklv/core/time.hpp"
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
    Label* getTitleLabel() const { return this->titleLabel; }

    // 右侧内容区：想塞自己的东西就 addView 到这个 Box
    Box* getRightBox() const { return this->rightBox; }

    void setOnClick(std::function<void()> handler) { this->onClick = std::move(handler); }
    void trigger();

    // 禁用的单元格不响应动作，外观走 disabled 状态
    void setEnabled(bool enabled);
    bool isEnabled() const { return this->enabled; }

    // 禁用的单元格也不参与焦点（InputManager::canFocus 会查这条）
    bool isDisabled() const override { return !this->enabled; }

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

// 开/关的表现形式
enum class BooleanCellStyle
{
    TEXT,   // borealis 的做法：右侧文字"开/关"，切换时字号弹一下
    SWITCH, // 我们的额外样式：自绘轨道 + 圆点（更像手机开关）
};

// 开关：A 键/点击切换，只支持开/关（三态开关不在 borealis 里，也就不做）
class BooleanCell : public Cell
{
  public:
    explicit BooleanCell(const std::string& title = "", bool value = false);

    void setValue(bool value);
    bool getValue() const { return this->value; }
    void setOnChange(std::function<void(bool)> handler) { this->onChange = std::move(handler); }

    void setStyle(BooleanCellStyle value) { this->style = value; }
    BooleanCellStyle getStyle() const { return this->style; }

    // 文字模式的字号弹跳（borealis：1 -> 0.8 -> 1，各 100ms）；
    // update() 内部按帧间隔推进，测试可以喂固定 dt。
    float getTextScale() const { return this->textScale; }
    void stepTextBounce(float deltaSeconds);

    void applyTheme() override;
    void update() override;

    // 供测试与自定义绘制读（构造完就应该是有效颜色）
    ImU32 getTrackOnColor() const { return this->trackOn; }
    ImU32 getTrackOffColor() const { return this->trackOff; }
    ImU32 getKnobColor() const { return this->knobColor; }
    ImU32 getValueColor() const { return this->valueColor; }
    ImU32 getDisabledColor() const { return this->disabledColor; }

    std::string getClassString() const override { return "bklv::BooleanCell"; }

  protected:
    void drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x, float y,
        float width, float height, const AppearanceInstance& appearance) override;

  private:
    void refreshTextMode();

    bool value = false;
    BooleanCellStyle style = BooleanCellStyle::TEXT;

    // 图形开关（SWITCH 样式）的几何与配色
    float switchWidth  = 52.0f;
    float switchHeight = 26.0f;
    float knobSize     = 20.0f;
    ImU32 trackOn      = 0;
    ImU32 trackOff     = 0;
    ImU32 knobColor    = 0;

    // 文字模式（borealis）的配色与弹跳
    float baseDetailFontSize = 16.0f;
    float textScale          = 1.0f;
    ImU32 valueColor         = 0;
    ImU32 disabledColor      = 0;
    Animatable bounce{ 1.0f };
    bool bouncing = false;

    std::function<void(bool)> onChange;
};

// 单选：borealis 的 RadioCell 右侧是个 CheckBox —— **填充圆 + 对勾**，
// 而且**只有选中时才显示**（未选中那块区域是空的），选中时标题用 value 色。
class RadioCell : public Cell
{
  public:
    explicit RadioCell(const std::string& title = "", bool selected = false);

    void setSelected(bool selected);
    bool isSelected() const { return this->selected; }
    void setOnChange(std::function<void(bool)> handler) { this->onChange = std::move(handler); }

    void applyTheme() override;

    // 对勾圆是否显示（= 是否选中；borealis 未选中时整块 GONE）
    bool isIndicatorVisible() const { return this->selected; }

    ImU32 getCircleColor() const { return this->circleColor; }
    ImU32 getMarkColor() const { return this->markColor; }
    float getCircleRadius() const { return this->circleRadius; }

    std::string getClassString() const override { return "bklv::RadioCell"; }

  protected:
    void drawCustomLayer(ImDrawList* drawList, const AppearanceLayer& layer, float x, float y,
        float width, float height, const AppearanceInstance& appearance) override;

  private:
    void refreshTitleColor();

    bool selected = false;
    float circleRadius = 15.0f;
    ImU32 circleColor  = 0;
    ImU32 markColor    = 0;
    std::function<void(bool)> onChange;
};

// 右侧显示当前值的**纯文本**（borealis 就是 setText(data[selection])，没有 ‹ ›）；
// 点击/确认走 onOpen（demo 里弹 Dropdown）；左右键/LB/RB 直接换值是我们的额外便利。
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

    // 点击/确认时的回调。默认行为 = 弹 Dropdown（borealis 的 SelectorCell 就是这样），
    // 设了这个回调就由调用方接管。
    void setOnOpen(std::function<void()> handler) { this->onOpen = std::move(handler); }

    // 默认的"弹列表"实现（无 App 时退化成切到下一项，方便无窗口测试）
    void openDropdown();

    // 下一项 / 上一项（越界循环）
    void selectNext();
    void selectPrevious();

    std::string getClassString() const override { return "bklv::SelectorCell"; }

  private:
    void refreshDetail();

    std::vector<std::string> options;
    int selectedIndex = 0;
    std::function<void(int, const std::string&)> onChange;
    std::function<void()> onOpen;
    class Dropdown* dropdown = nullptr; // 懒创建，用来弹选项列表
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

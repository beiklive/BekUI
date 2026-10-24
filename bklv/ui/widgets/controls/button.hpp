#pragma once

// 按钮家族：抽象基类只管"按钮能做什么"，具体形态只管"往里塞什么内容"，一行绘制代码都没有。
//
// ---- 分工 ----
// Button（抽象）：行为 + 基础外观。基础外观固定是 `base/button/button`，只画
//   背景、边框、阴影与高亮底，外加按下位移与禁用态，**不含任何内容层**。
//   所以子类不用操心外观，只要声明自己的内容位、往里写内容。
// 具体形态：各自声明"我有哪些内容位"，外观是 `<基名>.<variant>`，默认 variant 就是形态名：
//     TextButton      -> base/button/button.text
//     IconButton      -> base/button/button.icon
//     IconTextButton  -> base/button/button.icon_text
//   换一套皮肤给 variant 换个后缀就行（如 `TextButton("确定", "bordered")`）。
//
// ---- 内容位 id ----
// 四个约定名字（`button_slot` 里），外观 JSON 的 `"data_id"` 与这里必须逐字一致；
// 新形态按需从这四个里挑，不要另造名字 —— JSON 视图里的数据也用同一套 key。
//   m_icon     字体图标位（`"font_kind": "icon"` 的文本层）
//   m_text     主文本位
//   m_subtext  次文本位（副标题 / 说明）
//   m_infotext 信息文本位（右侧小字 / 附加说明）
//
// ---- 两个 id 维度，别混 ----
// 内容位 id：见上，外观 JSON 用 `"data_id"` 索引、C++ 用 `setText(id, …)` 写。
// 视图 id：视图 JSON 里写 `"id": "ok"`，之后 `findViewById("ok")` 取到控件本身，
//   走 `View::setId()` / `View::getId()`，与内容位无关。

#include <functional>
#include <string>
#include <utility>

#include "bklv/ui/widgets/appearance/appearance_view.hpp"

namespace bklv
{

// 内容位 id：所有按钮共用这一套名字。
namespace button_slot
{
inline constexpr char ICON[]     = "m_icon";
inline constexpr char TEXT[]     = "m_text";
inline constexpr char SUBTEXT[]  = "m_subtext";
inline constexpr char INFOTEXT[] = "m_infotext";
} // namespace button_slot

// 抽象按钮：不能直接实例化，用下面的具体形态。
class Button : public AppearanceView
{
  public:
    ~Button() override = default;

    void setOnClick(std::function<void()> callback) { this->onClick = std::move(callback); }

    // 开关类形态（文字开关 / 图案开关）：切换外观状态 on/off，动效由主题描述。
    void setSwitched(bool on);
    bool isSwitched() const { return this->switched; }

    // 禁用的按钮不响应动作，外观走 disabled 状态（主题里可定义变灰 / 降透明度）。
    void setEnabled(bool enabled);
    bool isEnabled() const { return this->enabled; }

    // 禁用的按钮也不参与焦点（InputManager::canFocus 会查这条）。
    bool isDisabled() const override { return !this->enabled; }

    // 程序化触发（等同于用户按下）。
    virtual void trigger();

    void update() override;

    // 具体形态必须自报类型名（调试层按它显示）与属性表（认哪几个 JSON 属性）。
    std::string getClassString() const override = 0;
    const AttributeTable& attributes() const override = 0;

  protected:
    // 基础外观固定 `base/button/button`；variant 非空时最终外观名是
    // `<基名>.<variant>`，形态自己的内容层就写在那份外观里。
    explicit Button(std::string variant);

  private:
    std::function<void()> onClick;
    bool switched       = false;
    bool enabled        = true;
    double pressedUntil = 0.0; // 按下反馈的结束时刻（秒；时间基是 getCPUTimeUsec()，不依赖 ImGui 上下文）
};

// 纯文本按钮：内容位 `m_text`，默认外观 `base/button/button.text`。
class TextButton : public Button
{
  public:
    static constexpr const char* SLOT_TEXT = button_slot::TEXT;

    using AppearanceView::setText; // 保留 setText(id, value) 那个重载

    explicit TextButton(const std::string& text = "", const std::string& variant = "text");

    void setText(const std::string& text) { this->setText(SLOT_TEXT, text); }
    const std::string& getText() const { return this->getSlotText(SLOT_TEXT); }

    std::string getClassString() const override { return "bklv::TextButton"; }
    const AttributeTable& attributes() const override;
};

// 只有字体图标的按钮：内容位 `m_icon`，默认外观 `base/button/button.icon`。
class IconButton : public Button
{
  public:
    static constexpr const char* SLOT_ICON = button_slot::ICON;

    using AppearanceView::setIcon;

    explicit IconButton(const std::string& glyph = "", const std::string& variant = "icon");

    void setIcon(const std::string& glyph) { this->setIcon(SLOT_ICON, glyph); }
    const std::string& getIcon() const { return this->getSlotText(SLOT_ICON); }

    std::string getClassString() const override { return "bklv::IconButton"; }
    const AttributeTable& attributes() const override;
};

// 图标 + 文字：内容位 `m_icon` / `m_text`，默认外观 `base/button/button.icon_text`。
class IconTextButton : public Button
{
  public:
    static constexpr const char* SLOT_ICON = button_slot::ICON;
    static constexpr const char* SLOT_TEXT = button_slot::TEXT;

    using AppearanceView::setIcon;
    using AppearanceView::setText;

    IconTextButton(const std::string& text = "", const std::string& glyph = "", const std::string& variant = "icon_text");

    void setText(const std::string& text) { this->setText(SLOT_TEXT, text); }
    void setIcon(const std::string& glyph) { this->setIcon(SLOT_ICON, glyph); }
    const std::string& getText() const { return this->getSlotText(SLOT_TEXT); }
    const std::string& getIcon() const { return this->getSlotText(SLOT_ICON); }

    std::string getClassString() const override { return "bklv::IconTextButton"; }
    const AttributeTable& attributes() const override;
};

} // namespace bklv

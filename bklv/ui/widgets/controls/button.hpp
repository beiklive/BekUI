#pragma once

// 抽象按钮：只声明"按钮语义"与两个预留内容位，一行绘制代码都没有。
//
// 为什么拆成基类 + 三个具体类：外观 JSON 本来就是按形态分开的
// （`base/button/button`、`button.icon`、`button.icon_text`），每种形态认哪些内容位也不同。
// 基类收共同语义（能不能点、按下反馈、禁用、开关、属性表套路），
// 具体类只声明"用哪份外观、填哪几个内容位"。
//
// ---- 两个 id 维度，别混 ----
// 内容位 id（外观 JSON 里用 `"data_id"` 索引）：
//   Button::TEXT_SLOT = "text"   文本位
//   Button::ICON_SLOT = "icon"   字体图标位（`"font_kind": "icon"` 的层）
// 视图 id（视图 JSON 里写 `"id": "ok"`，之后 `findViewById("ok")` 取到控件本身）：
//   走 View::setId() / View::getId()，与内容位无关。
//
// 想加新的内容位（badge / trailing）不用改这里：`AppearanceView::setText("badge", …)`
// 配合外观里 `"data_id": "badge"` 就能用，见 `content_ids.hpp`。

#include <functional>
#include <string>
#include <utility>

#include "bklv/ui/widgets/appearance/appearance_view.hpp"
#include "bklv/ui/widgets/appearance/content_ids.hpp"

namespace bklv
{

// 抽象按钮：不能直接实例化，用下面的三个具体形态。
class Button : public AppearanceView
{
  public:
    // 两个预留内容位的 id（外观 JSON 的 `"data_id"` 就是这两个值）
    static constexpr const char* TEXT_SLOT = content::TEXT;
    static constexpr const char* ICON_SLOT = content::ICON;

    ~Button() override = default;

    // 内容位读写：两个 overload 来自基类（按 id 写任意槽），单参数版本写约定的槽
    using AppearanceView::setIcon;
    using AppearanceView::setText;
    void setText(const std::string& text);
    void setIcon(const std::string& glyph);
    const std::string& getText() const { return this->getSlotText(content::TEXT); }
    const std::string& getIcon() const { return this->getSlotText(content::ICON); }

    void setOnClick(std::function<void()> callback) { this->onClick = std::move(callback); }

    // 开关类变体（文字开关 / 图案开关）：切换外观状态 on/off，动效由主题描述
    void setSwitched(bool on);
    bool isSwitched() const { return this->switched; }

    // 禁用的按钮不响应动作，外观走 disabled 状态（主题里可定义变灰/降透明度）
    void setEnabled(bool enabled);
    bool isEnabled() const { return this->enabled; }

    // 禁用的按钮也不参与焦点（InputManager::canFocus 会查这条）
    bool isDisabled() const override { return !this->enabled; }

    // 程序化触发（等同于用户按下）
    virtual void trigger();

    void update() override;

    // 具体按钮必须自报类型名（调试层按它显示）与属性表（认哪几个 JSON 属性）
    std::string getClassString() const override = 0;
    const AttributeTable& attributes() const override = 0;

  protected:
    // appearanceName 是外观基名（如 "base/button/button"）；variant 非空时
    // 最终外观名是 "<基名>.<variant>"（见 AppearanceView::applyTheme）。
    Button(std::string appearanceName, std::string variant);

  private:
    std::function<void()> onClick;
    bool switched        = false;
    bool enabled         = true;
    double pressedUntil  = 0.0; // 按下反馈的结束时刻（秒；时间基是 getCPUTimeUsec()，不依赖 ImGui 上下文）
};

// 纯文本按钮：外观 `base/button/button`（variant 给定时是 `button.<variant>`）
class TextButton : public Button
{
  public:
    explicit TextButton(const std::string& text = "", const std::string& variant = "");

    std::string getClassString() const override { return "bklv::TextButton"; }
    const AttributeTable& attributes() const override;
};

// 只有字体图标的按钮：默认外观 `base/button/button.icon`
class IconButton : public Button
{
  public:
    explicit IconButton(const std::string& glyph = "", const std::string& variant = "icon");

    std::string getClassString() const override { return "bklv::IconButton"; }
    const AttributeTable& attributes() const override;
};

// 图标 + 文字：默认外观 `base/button/button.icon_text`
class IconTextButton : public Button
{
  public:
    IconTextButton(
        const std::string& text = "", const std::string& glyph = "", const std::string& variant = "icon_text");

    std::string getClassString() const override { return "bklv::IconTextButton"; }
    const AttributeTable& attributes() const override;
};

} // namespace bklv

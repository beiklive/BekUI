#pragma once

// 按钮：外观完全来自主题 JSON（`widgets/button.json`），
// 状态动效（聚焦变色、按下反馈、禁用变灰）也由 JSON 描述、tweeny 插值。
//
// 本类只负责语义：文字内容、能不能点、点了干什么。
// 绘制（底/边框/文字/圆角）一行代码都没有——那是外观 JSON 的事。

#include <functional>
#include <string>

#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

class Button : public AppearanceView
{
  public:
    explicit Button(const std::string& text = "", const std::string& variant = "");

    void setOnClick(std::function<void()> callback) { this->onClick = std::move(callback); }

    // 开关类变体（文字开关 / 图案开关）：切换外观状态 on/off，动效由主题描述
    void setSwitched(bool on);
    bool isSwitched() const { return this->switched; }

    // 禁用的按钮不响应动作，外观走 disabled 状态（主题里可定义变灰/降透明度）
    void setEnabled(bool enabled);
    bool isEnabled() const { return this->enabled; }

    // 禁用的按钮也不参与焦点（InputManager::canFocus 会查这条）
    bool isDisabled() const override { return !this->enabled; }

    // 程序化触发（等同于用户按下）。虚函数：选择器按钮要把它改成"进/出设置态"
    virtual void trigger();

    void update() override;

    std::string getClassString() const override { return "bklv::Button"; }
    const AttributeTable& attributes() const override;



  private:
    std::function<void()> onClick;
    bool switched = false;
    bool enabled          = true;
    float pressedUntil    = 0.0f; // 按下反馈的结束时刻（ImGui 时间）
};

} // namespace bklv

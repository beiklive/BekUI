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

    // 禁用的按钮不响应动作，外观走 disabled 状态（主题里可定义变灰/降透明度）
    void setEnabled(bool enabled);
    bool isEnabled() const { return this->enabled; }

    // 程序化触发（等同于用户按下）
    void trigger();

    void update() override;

    std::string getClassString() const override { return "bklv::Button"; }
    const AttributeTable& attributes() const override;

  private:
    std::function<void()> onClick;
    bool enabled          = true;
    float pressedUntil    = 0.0f; // 按下反馈的结束时刻（ImGui 时间）
};

} // namespace bklv

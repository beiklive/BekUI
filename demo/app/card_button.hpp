#pragma once

// 主页卡片按钮：Balatro 那种"轻微浮动 + 聚焦抬起/倾斜"的动效。
//
// 全部是**绘制期变换**（translateY / rotate / scale），不参与布局 —— 卡片浮起来不会把
// 邻居顶下去，也不会触发重排。样式在 `base/button/card.json`。

#include <functional>
#include <string>

#include "bklv/widgets/appearance_view.hpp"

namespace demo
{

class CardButton : public bklv::AppearanceView
{
  public:
    CardButton(const std::string& title, const std::string& subtitle, const std::string& icon);

    void setOnClick(std::function<void()> callback) { this->onClick = std::move(callback); }

    void update() override;
    std::string getClassString() const override { return "demo::CardButton"; }

  protected:
    void onFocusGained() override;
    void onFocusLost() override;

  private:
    std::function<void()> onClick;
    float phase   = 0.0f; // 每张卡错开相位，避免整齐划一地上下
    float time    = 0.0f;
    float lift    = 0.0f; // 当前抬起高度（平滑逼近目标）
    float tilt    = 0.0f;
    bool  focused = false;
};

} // namespace demo

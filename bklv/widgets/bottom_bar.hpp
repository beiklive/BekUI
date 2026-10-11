#pragma once

// 底栏：上边线 + 一行文字（对应 borealis 的 views/bottom_bar）。
// 结构在 `widgets/bottom_bar.json` 里；C++ 只负责文字内容。
//
// 和 bklv 已有的 Hints（画在画面最上层、由 App::drawHints 下发）不同：
// 这是一个普通视图，参与布局，可以放在页面里的任意位置。

#include <string>

#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

class BottomBar : public AppearanceView
{
  public:
    explicit BottomBar(const std::string& text = "", const std::string& variant = "");

    // 从当前焦点注册的动作生成提示文案（"A 确定   B 返回"）
    void showHintsFromFocus();

    std::string getClassString() const override { return "bklv::BottomBar"; }
    const AttributeTable& attributes() const override;
};

} // namespace bklv

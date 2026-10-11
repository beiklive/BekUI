#pragma once

// 底栏：上边线 + 一行文字（对应 borealis 的 views/bottom_bar）。
// 结构在 `widgets/bottom_bar.json` 里；C++ 只负责文字内容。
//
// 和 `widgets/hints_bar`（提示栏，显示 [按键图标] 说明）的区别：
// 这个只显示一行纯文字，想改文案就 setText()。AppletFrame 默认用提示栏做底栏，
// 需要文字底栏时用 `AppletFrame::setUseTextFooter(true)`。

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

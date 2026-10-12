#pragma once

// 底部栏（对应 borealis 的 views/bottom_bar）：**一个控件同时装提示与状态**。
//
//   BottomBar（装饰在 widgets/bottom_bar.json：上边线 1px）
//     ├─ HintsBar   左侧：当前焦点的动作提示（[按键图标] 说明 …）
//     └─ StatusArea 右侧：时钟（Switch 上还有电量）
//
// 高度 / 左右外边距 / 内边距都按 borealis 的 hints/* 与 applet_frame/footer_height 来，
// 写在 bottom_bar.json 的 box 段里。

#include <string>

#include "bklv/core/box.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "bklv/widgets/hints_bar.hpp"
#include "bklv/widgets/status_area.hpp"

namespace bklv
{

class BottomBar : public AppearanceView
{
  public:
    explicit BottomBar(const std::string& variant = "");

    HintsBar* getHints() const { return this->hints; }
    StatusArea* getStatus() const { return this->status; }

    // 刷新左侧提示（切页 / 焦点变化后由外壳调；HintsBar 自己也会在 update 里刷）
    void refreshHints() { this->hints->refresh(); }

    // 换肤/改缩放时重新套用（尺寸与上边线都在 widgets/bottom_bar.json）
    void applyTheme() override;

    std::string getClassString() const override { return "bklv::BottomBar"; }

  private:
    HintsBar* hints   = nullptr;
    StatusArea* status = nullptr;
};

} // namespace bklv

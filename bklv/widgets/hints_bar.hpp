#pragma once

// 提示栏（对应 borealis 的 views/hints）：把当前焦点注册的动作显示成
//   [按键图标] 说明    [按键图标] 说明   …
//
// 图标来自图标字体（Switch 系统字体的 NintendoExt 自带，桌面随包带
// resources/font/switch_icons.ttf），字形码位和 borealis 一致。
//
// 焦点一变就重建条目（输入层的提示列表变了才重建，不是每帧）。

#include <string>
#include <vector>

#include "bklv/core/box.hpp"
#include "bklv/widgets/appearance_view.hpp"

namespace bklv
{

// 单条提示：图标 + 文字
class HintItemView : public AppearanceView
{
  public:
    HintItemView();

    void setButton(int button);
    void setLabel(const std::string& text);

    std::string getClassString() const override { return "bklv::HintItemView"; }

  protected:
    std::string getLayerText(const std::string& source) override;
    ImFont* getLayerFont(const AppearanceLayer& layer) override;

  private:
    std::string icon;
    std::string label;
};

class HintsBar : public Box
{
  public:
    explicit HintsBar(const std::string& variant = "");

    // 立刻按当前焦点刷新（一般不用手动调，update() 会做）
    void refresh();


    // 没有提示时是否隐藏（默认隐藏，免得留一条空栏）
    void setHideWhenEmpty(bool value) { this->hideWhenEmpty = value; }

    void update() override;

    std::string getClassString() const override { return "bklv::HintsBar"; }

    // 测试用：当前条目数
    size_t getItemCount() const { return this->items.size(); }

  private:
    std::vector<HintItemView*> items;
    std::string lastSignature;
    bool hideWhenEmpty = true;
};

} // namespace bklv

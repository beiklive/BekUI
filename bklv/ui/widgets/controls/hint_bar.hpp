#pragma once

// 提示栏三件套：BottomBar（底栏）→ HintBar（提示条）→ Hint（一条提示）。
//
//   ┌──────────────────────────────────────────────┐  ← bottombar.json：100px 高、满宽
//   │                                              │     顶部 1px 分隔线（separator 层）
//   │                        Ⓐ 确认   Ⓑ 返回       │  ← hintbar.json：靠右居中
//   └──────────────────────────────────────────────┘
//
// 分工（和 Button 家族一样"代码管结构与行为、JSON 只管样式"）：
//   * 三个类都**不可聚焦**：提示栏不参与方向键导航，也不会被焦点框框到；
//   * `Hint` 覆写 isPointerTarget() → 能点，但点了**不抢焦点**（焦点留在原控件上，
//     所以提示栏不会因为点了一下就变样）。点击走 TapGestureRecognizer，和 borealis
//     的 Hint 一个路子；动作本身调的是注册那个键时给的 listener；
//   * 内容全部来自 InputManager::getActiveHints()（沿焦点父链收集 + 去重 + 排序），
//     HintBar 用签名比对只在内容变化时重建条目，不每帧重建。
//
// 命名注意：数据层的 `bklv::HintItem`（input/input.hpp，无 UI 的 {按键, 文字}）是
// 另一回事；这里的视图叫 `Hint`，对应 borealis 的 views/hint。

#include <cstddef>
#include <string>
#include <vector>

#include "bklv/ui/input/input.hpp"
#include "bklv/ui/widgets/appearance/appearance_view.hpp"

namespace bklv
{

// 一条提示：按键图标 + 说明文字。
//
// 两个内容位的 id 与 `resources/theme/default/base/hintitem.json` 里的 `data_id` 对齐：
// `icon`（图标字体字形）、`label`（说明文字）。
class Hint : public AppearanceView
{
  public:
    Hint();

    // 设置这一条提示：哪个键、显示什么、点了（或按了）执行什么。
    // owner 是注册这个动作的视图，回调时优先传它（listener 的签名是 void(View*)）。
    void setHint(ControllerButton button, const std::string& text, const ActionListener& listener,
        View* owner);

    ControllerButton getButton() const { return this->button; }
    const std::string& getLabel() const { return this->label; }

    // 能点，但不当焦点（见头注释）。
    bool isPointerTarget() const override { return true; }

    std::string getClassString() const override { return "bklv::Hint"; }

  private:
    void refreshContent();
    void invoke();

    ControllerButton button = ControllerButton::NONE;
    std::string label;
    ActionListener listener;
    View* owner = nullptr;
};

// 一排提示。宽度自适应内容，位置由外层（BottomBar）的 box 段决定。
class HintBar : public AppearanceView
{
  public:
    HintBar();

    // 立刻按当前焦点刷新（update() 每帧会调，一般不用手动调）
    void refresh();

    // 没有提示时是否隐藏自己（默认隐藏，免得留一条空栏）
    void setHideWhenEmpty(bool value) { this->hideWhenEmpty = value; }
    bool isHideWhenEmpty() const { return this->hideWhenEmpty; }

    // 当前条目数（调试 / 验收用）
    size_t getItemCount() const { return this->items.size(); }

    // 取第 index 条（调试 / 验收用；点击行为可以靠它手动派发手势来验）
    Hint* getItem(size_t index) const
    {
        return index < this->items.size() ? this->items[index] : nullptr;
    }

    void update() override;

    std::string getClassString() const override { return "bklv::HintBar"; }

  private:
    std::vector<Hint*> items;
    std::string lastSignature;
    bool hideWhenEmpty = true;
};

// 底栏：一条 100px 高的横条，顶部一条 1px 分隔线，右侧居中放提示条。
// 挂在外壳的覆盖层（overlay）上；内容层要自己让出 100px（见 demo）。
class BottomBar : public AppearanceView
{
  public:
    BottomBar();

    HintBar* getHintBar() const { return this->hintBar; }

    std::string getClassString() const override { return "bklv::BottomBar"; }

  private:
    HintBar* hintBar = nullptr;
};

} // namespace bklv

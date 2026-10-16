#pragma once

// 焦点框的绘制者：挂在外壳的浮层（HUD）上，每帧**拉取**当前焦点的形状画出来。
//
// 依赖方向是单向的：
//   FocusRingView → 问输入管理器"当前焦点是谁" → 问那个视图"你是什么形状"
// 控件完全不知道焦点框的存在（只实现一个 View::getFocusOutline()），
// 所以换画法、换类型、开关，都不用动任何控件。
//
// 颜色来自 LUT 贴图（core/focus_ring.hpp 的说明），样式不跟主题 JSON 走 —— 由代码写死，
// 想改走顶层外壳（Shell）的接口。

#include "bklv/core/focus_ring.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/core/view.hpp"

namespace bklv
{

class FocusRingView : public View
{
  public:
    FocusRingView();
    ~FocusRingView() override;

    std::string getClassString() const override { return "bklv::FocusRingView"; }

    // 全局开关（由外壳转发）
    void setRingEnabled(bool enabled) { this->ringEnabled = enabled; }
    bool isRingEnabled() const { return this->ringEnabled; }

    // 换类型：NONE / FLOW / BREATHING / SOLID / CUSTOM
    void setRingType(focus_ring::Type type) { this->style.type = type; }
    focus_ring::Type getRingType() const { return this->style.type; }

    // 细调（线宽/缝/速度/透明度/分段/自定义渲染器）
    focus_ring::Style& getRingStyle() { return this->style; }
    const focus_ring::Style& getRingStyle() const { return this->style; }

    // 当前这一刻会不会真的画出来（有焦点 + 开关打开 + 贴图就绪）。测试与调试用。
    bool isRingVisible() const;

    // 显式推进流动时钟：测试用确定性步进；平时由 update() 按帧时钟推进。
    void stepPhase(float deltaSeconds);

    void update() override;
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        FrameContext* ctx) override;

  private:
    void ensureTexture();

    focus_ring::Style style;
    TextureRef lut;
    bool textureLoadAttempted = false;
    bool ringEnabled          = true;
    float elapsed             = 0.0f; // 流动时钟（秒）；相位由它算出来，便于确定性测试
};

} // namespace bklv

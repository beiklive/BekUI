#pragma once

// 焦点框的绘制者：挂在宿主根视图（或浮层）上，每帧**拉取**当前焦点的形状画出来。
//
// 依赖方向是单向的：
//   FocusRingView → 问输入管理器"当前焦点是谁" → 问那个视图"你是什么形状"
// 控件完全不知道焦点框的存在（只实现一个 `View::getFocusOutline()`），
// 所以换画法、换类型、开关，都不用动任何控件。
//
// 颜色来自 LUT 贴图（`graphics/paint/focus_ring.hpp` 的说明），样式不跟主题 JSON 走 ——
// 由代码写死；宿主想调就通过 `getRingStyle()` 改。
//
// 用法：
//   shell->getFocusRingView()   // Shell 已经建好并挂在浮层上，宿主不用自己加
// 它自己是绝对定位铺满父容器、不参与布局、也不吃输入（可聚焦性默认 false）。

#include "bklv/graphics/paint/focus_ring.hpp"
#include "bklv/graphics/render/graphics.hpp"
#include "bklv/ui/view/view.hpp"

namespace bklv
{

class FocusRingView : public View
{
  public:
    FocusRingView();
    ~FocusRingView() override;

    std::string getClassString() const override { return "bklv::FocusRingView"; }

    // 全局开关
    void setRingEnabled(bool enabled) { this->ringEnabled = enabled; }
    bool isRingEnabled() const { return this->ringEnabled; }

    // 换类型：NONE / FLOW / BREATHING / SOLID / CUSTOM
    void setRingType(focus_ring::Type type) { this->style.type = type; }
    focus_ring::Type getRingType() const { return this->style.type; }

    // 细调（线宽 / 缝 / 速度 / 透明度 / 分段 / 自定义渲染器）
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

#pragma once

// 调试层：把所有视图的边界框出来，并在够大的框里写上"类名 + 尺寸"。
// 对应 borealis 的 views/debug_layer（以及它 demo 里 XML 属性 wireframe="true"）。
//
// 与 borealis 的差别：
// - 收集边界是**纯函数**（`collectDebugBounds`），不依赖绘制，所以能无窗口测试。
// - 挂在 `App::setDebugLayerEnabled(true)` 上：作为根视图最后一个子节点、绝对定位铺满，
//   因此画在所有内容之上，且不吃焦点/不拦输入（isFocusable=false）。
// - 颜色在主题里（`color.debug.*`），不写死。

#include <string>
#include <vector>

#include "bklv/foundation/geom/geometry.hpp"
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/view/view.hpp"

namespace bklv
{

// 一个被框出来的视图
struct DebugBounds
{
    Rect rect;
    std::string className;
    int depth = 0;
};

// 遍历视图树收集（父在前、子在后；GONE 的跳过）。maxDepth 防环/防爆栈。
void collectDebugBounds(
    View* root, std::vector<DebugBounds>& out, int maxDepth = 32, int depth = 0);

class DebugLayer : public View
{
  public:
    DebugLayer();

    // 从哪个视图开始收集（通常是 App 的根视图）
    void setRoot(View* value) { this->root = value; }

    // 是否画文字（框太小就不画，免得糊成一团）
    void setShowText(bool value) { this->showText = value; }
    bool isShowText() const { return this->showText; }

    void applyTheme() override;
    void draw(ImDrawList* drawList, float x, float y, float width, float height,
        FrameContext* ctx) override;

    // 纯浮层：不吃焦点、不参与命中（点哪都穿过去）
    bool isFocusable() const override { return false; }

    std::string getClassString() const override { return "bklv::DebugLayer"; }

  private:
    View* root      = nullptr;
    bool showText   = true;
    std::vector<DebugBounds> bounds;
    ImU32 boundsColor = 0;
    ImU32 textColor   = 0;
};

} // namespace bklv

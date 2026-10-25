#pragma once

// 「调试」标签页：把库里的调试视图与平台能力摆出来。
//
//   - 应用内日志（DebugLogView，挂在外壳的调试层）
//   - FPS / 帧时间浮层（FpsOverlay，挂在 HUD）
//   - 调试边框（DebugLayer，把所有视图的边界框出来）
//   - 按页背景（PresetBackdrop：网格 / 漂浮 / 渐变）
//   - 安全区内缩（Shell::setSafeAreaInsets，模拟刘海 / 电视 overscan）
//   - 玻璃开关与质量档（Shell::setGlassEnabled / setGlassQuality）
//
// 所有开关都是"再点一次切回去"，页面上有一个状态行显示当前值。

namespace bklv
{
class View;
}

namespace demo
{

class DemoApp;

bklv::View* buildDebugTab(DemoApp& app);

} // namespace demo

#pragma once

// UI 缩放基准：**所有主题里的尺寸都按 1280x720 写**，运行时按窗口宽度等比放大。
//
// 这是 borealis 的做法（`ORIGINAL_WINDOW_WIDTH/HEIGHT = 1280x720`，
// `windowScale = windowWidth / 1280`，它是在绘制时缩放整块画布）。
// 本库在"取值"这一层缩放，效果一样：布局坐标还是真实像素，
// 但主题里的尺寸、字号、圆角、内边距都会跟着窗口等比变化，
// 所以 720p 上调好的比例在 1080p / 4K 上看起来是一致的。
//
// 缩放作用在**所有"按 720p 量的像素量"**上，三处用同一个基准（否则换分辨率就错位）：
//   · ThemeData::number() 里 `metric.*` 会乘缩放（`metric.anim.*` 是毫秒、不缩放）
//   · 外观 JSON 里的 radius/strokeWidth/width/height/fontSize/inset/offset 会乘缩放
//     （opacity 是系数、duration 是毫秒，都不缩放）
//   · **视图描述 JSON** 里的像素量（width/height/min*/margin*/top/left/right/bottom/
//     borderThickness/cornerRadius/fontSize/图片的 offsetX/offsetY 等）也会乘 —— 靠
//     `AttributeDef::pixels` 这一位标记，在 `applyFromTable` 里统一乘。百分比（"50%"）、
//     auto、系数（grow/shrink/alpha/显示比例）不乘；`@style/` 引用本来就乘过，不重复乘。
// 代码里直接写死的像素值（setCornerRadius(8) 之类）**不缩放**：
// 想跟随 720p 比例就用主题 metric，或者把数字写进外观 JSON。

namespace bklv
{

constexpr float UI_BASE_WIDTH  = 1280.0f;
constexpr float UI_BASE_HEIGHT = 720.0f;

// 当前缩放系数（默认 1.0）
float getUiScale();
void setUiScale(float scale);

// 按画布尺寸算出缩放系数；变了就返回 true（调用方负责重新套用外观 + 重排）
bool updateUiScaleFromCanvas(float canvasWidth, float canvasHeight);

} // namespace bklv

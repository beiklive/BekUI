#pragma once

// bklv 的伞头文件：常用类型一次引入，省掉一长串 include。
//
//     #include "bklv/bklv.hpp"
//     auto app = bklv::App();
//     bklv::Box root(bklv::Axis::COLUMN);
//     app.setRootView(&root);
//
// 只收"写界面会用到"的公开头，按模块分组（与应用侧的目录一一对应，方便顺着目录找）。
// 具体后端（`render/gl|vk|gles3|libnx`）与平台适配（`platform/`）是按平台和图形 API
// 各编一份的，不属于使用者的接口，所以不在这里；需要平台能力时走 `core/platform.hpp`
// 的抽象。想更精确地控制依赖，也可以只包含具体某一个头（头文件彼此独立，都自带 include）。
//
// 依赖方向（违反会被 scripts/check_layers.py 拦下）：
//
//     widgets → view → theme → core
//     app 是组合根，可以用上面所有；platform 只在平台适配里出现。
//
// 新增公开头时请放进对应的小节，别让这个文件退化成"随便堆一列"。

// ---- app：组合根与上下文 ----
#include "bklv/ui/app/app.hpp"
#include "bklv/ui/app/context.hpp"

// ---- core：基础机制（生命周期、事件、日志、资源、平台抽象、宿主服务）----
#include "bklv/foundation/core/alive.hpp"
#include "bklv/foundation/core/cache.hpp"
#include "bklv/foundation/core/event.hpp"
#include "bklv/foundation/core/frame_budget.hpp"
#include "bklv/foundation/core/frame_stats.hpp"
#include "bklv/foundation/core/globals.hpp"
#include "bklv/foundation/core/i18n.hpp"
#include "bklv/foundation/core/logger.hpp"
#include "bklv/foundation/core/platform.hpp"
#include "bklv/foundation/core/resource.hpp"
#include "bklv/foundation/core/services.hpp"
#include "bklv/foundation/core/task.hpp"
#include "bklv/foundation/core/ui_scale.hpp"

// ---- geom：几何与颜色换算 ----
#include "bklv/foundation/geom/geometry.hpp"

// ---- anim：时间源、缓动、过渡与补间 ----
#include "bklv/foundation/anim/time.hpp"

// ---- media：字体、图标、图片解码、音频 ----
#include "bklv/system/media/audio.hpp"
#include "bklv/system/media/font.hpp"
#include "bklv/system/media/image_decoder.hpp"

// ---- render：图形接口（具体后端见 render/gl|vk|gles3|libnx）----
#include "bklv/graphics/render/graphics.hpp"

// ---- paint：绘制效果（投影等）----
#include "bklv/graphics/paint/shadow.hpp"

// ---- theme：外观数据模型与解析 ----
#include "bklv/ui/theme/appearance.hpp"
#include "bklv/ui/theme/attributes.hpp"
#include "bklv/ui/theme/theme_data.hpp"

// ---- view：视图树与 JSON 视图 ----
#include "bklv/ui/view/box.hpp"
#include "bklv/ui/view/json_actions.hpp"
#include "bklv/ui/view/json_data.hpp"
#include "bklv/ui/view/json_view.hpp"
#include "bklv/ui/view/view.hpp"

// ---- input：键盘 / 手柄 / 手势 / 输入法 ----
#include "bklv/ui/input/gamepad.hpp"
#include "bklv/ui/input/gesture.hpp"
#include "bklv/ui/input/ime.hpp"
#include "bklv/ui/input/input.hpp"

// ---- widgets：外观驱动的控件 ----
// 目前只留基础框架（外观解析 / 层绘制 / 通知）与 Button；其余控件已移除、等 demo 重做时按需加回。
// Box 在 `view/box.hpp`，Label 在 `widgets/basic/label.hpp`。
#include "bklv/ui/widgets/appearance/appearance_view.hpp"
#include "bklv/ui/widgets/controls/button.hpp"
#include "bklv/ui/widgets/controls/notification.hpp"

// ---- widgets（语义件）：文本 / 图片 / 容器 / 列表 / 调试覆盖层 ----
#include "bklv/ui/widgets/basic/image.hpp"
#include "bklv/ui/widgets/basic/label.hpp"
#include "bklv/ui/widgets/basic/padding.hpp"
#include "bklv/ui/widgets/basic/progress_spinner.hpp"
#include "bklv/ui/widgets/container/h_scrolling_frame.hpp"
#include "bklv/ui/widgets/container/scroll_box.hpp"
#include "bklv/ui/widgets/container/scrolling_frame.hpp"
#include "bklv/ui/widgets/debug/debug_layer.hpp"
#include "bklv/ui/widgets/debug/debug_log_view.hpp"
#include "bklv/ui/widgets/list/json_cell.hpp"
#include "bklv/ui/widgets/list/recycler.hpp"

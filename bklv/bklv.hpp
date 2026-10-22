#pragma once

// bklv 的伞头文件：常用类型一次引入，省掉一长串 include。
//
//     #include "bklv/bklv.hpp"
//     auto* app = new bklv::App();
//     app->setRootView(new bklv::Shell());
//
// 只收"写界面会用到"的公开头：core 的接口层、views 的语义控件、widgets 的外观控件。
// backends / platforms 是按平台与图形 API 选一份编的，不属于使用者的接口，
// 所以不在这里（需要平台能力时走 core/platform.hpp 的抽象）。
// 想更精确地控制依赖，也可以只包含具体某一个头（头文件彼此独立，都自带 include）。

// ---- core：接口层 ----
#include "bklv/core/app.hpp"
#include "bklv/core/appearance.hpp"
#include "bklv/core/alive.hpp"
#include "bklv/core/audio.hpp"
#include "bklv/core/cache.hpp"
#include "bklv/core/attributes.hpp"
#include "bklv/core/box.hpp"
#include "bklv/core/font.hpp"
#include "bklv/core/frame_budget.hpp"
#include "bklv/core/frame_stats.hpp"
#include "bklv/core/gamepad.hpp"
#include "bklv/core/event.hpp"
#include "bklv/core/geometry.hpp"
#include "bklv/core/gesture.hpp"
#include "bklv/core/globals.hpp"
#include "bklv/core/graphics.hpp"
#include "bklv/core/i18n.hpp"
#include "bklv/core/image_decoder.hpp"
#include "bklv/core/ime.hpp"
#include "bklv/core/input.hpp"
#include "bklv/core/json_view.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/platform.hpp"
#include "bklv/core/resource.hpp"
#include "bklv/core/shadow.hpp"
#include "bklv/core/task.hpp"
#include "bklv/core/theme_data.hpp"
#include "bklv/core/time.hpp"
#include "bklv/core/ui_scale.hpp"
#include "bklv/core/view.hpp"

// ---- views：语义控件 ----
#include "bklv/views/debug_layer.hpp"
#include "bklv/views/debug_log_view.hpp"
#include "bklv/views/h_scrolling_frame.hpp"
#include "bklv/views/image.hpp"
#include "bklv/views/json_cell.hpp"
#include "bklv/views/label.hpp"
#include "bklv/views/padding.hpp"
#include "bklv/views/progress_spinner.hpp"
#include "bklv/views/recycler.hpp"
#include "bklv/views/scrolling_frame.hpp"
#include "bklv/views/scroll_box.hpp"

// ---- widgets：外观驱动的控件 ----
#include "bklv/widgets/account.hpp"
#include "bklv/widgets/appearance_view.hpp"
#include "bklv/widgets/bottom_bar.hpp"
#include "bklv/widgets/button.hpp"
#include "bklv/widgets/section.hpp"
#include "bklv/widgets/selector_button.hpp"
#include "bklv/widgets/cell.hpp"
#include "bklv/widgets/dialog.hpp"
#include "bklv/widgets/dropdown.hpp"
#include "bklv/widgets/edit_text_dialog.hpp"
#include "bklv/widgets/form_buttons.hpp"
#include "bklv/widgets/header.hpp"
#include "bklv/widgets/ime_panel.hpp"
#include "bklv/widgets/hints_bar.hpp"
#include "bklv/widgets/notification.hpp"
#include "bklv/widgets/pager.hpp"
#include "bklv/widgets/rectangle.hpp"
#include "bklv/widgets/shell.hpp"
#include "bklv/widgets/slider.hpp"
#include "bklv/widgets/status_area.hpp"
#include "bklv/widgets/tab_frame.hpp"
#include "bklv/widgets/toast.hpp"

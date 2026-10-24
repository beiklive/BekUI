# 目录架构重构（2026-10）

只动**目录与 include 路径**：公共 API、类名、命名空间、控件行为、布局结果、渲染顺序、资源加载路径全部未改。`bklv::compile` / `bklv::layers` 这两个命名空间名字保留（改名会破坏公开 API），目录按职责拆开后在头文件注释里说明了差异。

## 1 最终目录树与模块职责

```
bklv/
  bklv.hpp                 伞头（按模块分节，只暴露稳定的公开头）
  core/
  geom/
  paint/
  anim/
  json/
  theme/
  layer/
  view/
  app/
  media/
  input/
  render/
  platform/
  widgets/
      appearance/  controls/  basic/  container/  list/  debug/
      view_types.cpp       内置视图类型注册入口
```

| 模块 | 文件 | 职责 |
|---|---|---|
| `core/` | 28 | 基础机制：全局状态登记、资源路径、日志、任务与事件、服务、UI 缩放、帧预算与帧统计、解码队列、i18n、平台接口 |
| `geom/` | 2 | 几何：Point / Size / Rect、绘制期变换 |
| `paint/` | 6 | 绘制效果：渐变顶点网格、柔化阴影（外投影 / 内阴影 / cutout）、焦点框形状与 LUT 采样 |
| `anim/` | 5 | 动画：缓动与补间、视图转场、外观 transition 通道播放器 |
| `json/` | 6 | JSON 工具：取值与诊断、变量块展开（`&` / `@`）、错误策略 |
| `theme/` | 20 | 外观与主题（领域）：ThemeData 与主题解析、Appearance 编译（box / layers / states / import）、图层类型表与参数表 |
| `layer/` | 8 | 图层绘制（领域）：六个内置层的绘制实现、第三方层注册模板 |
| `view/` | 10 | 视图：View / Box（Yoga 布局）/ ViewTemplate 两段式、JSON 数据与动作绑定 |
| `app/` | 4 | 应用：App 帧循环、Context、通知层 |
| `media/` | 15 | 媒体：字体与图标字体、图片解码与写出（PNG/JPEG/GIF/WebP/SVG）、音频与音效表 |
| `input/` | 8 | 输入：键盘 / 手柄 / 手势、焦点与模态栈、IME 模型 |
| `render/` | 36 | 渲染：RenderBackend 接口 + gl / vk / gles3 / libnx 四个后端 + glass/ 玻璃子系统 |
| `platform/` | 9 | 平台实现：SDL3（macOS / Linux / Windows）/ libnx（Switch） |
| `widgets/` | 42 | 控件：appearance（外观驱动基类）/ controls / basic / container / list / debug |
| `bklv.hpp` | 1 | 伞头 |

## 2 分类统计

| 项 | 数量 |
|---|---|
| 重构前 `bklv/` 源文件（.hpp + .cpp，不含 `3rd/`） | 199 |
| 重构后 | 200 |
| 移动（含"内容改动大、git 拆成 D + A"的移动） | 170 |
| 原地修改 | 8 |
| 新增 | 1 |
| 删除 | 0 |
| 无法分类 | 0 |

拆分/合并说明（其余文件都是整体移动，没有拆）：

- `compile/` 拆成三层职责：`json/`（通用 JSON 工具）、`theme/`（外观与主题解析）、`view/`（视图描述解析）。原因：`theme_parse` 要写 `ThemeData`、`view_parse` 要注册视图类型，两者与 JSON 工具混在一起时 `theme → layer` 与 `layer → theme` 形成模块级环。
- `core/glass/` 整组移到 `render/glass/`（跨后端部分留在顶层，`gl/` 与 `vk/` 是后端实现）：玻璃是渲染能力，不属于核心机制。
- `backends/` 按 API 拆成 `render/gl/`、`render/vk/`、`render/gles3/`、`render/libnx/`，接口 `render/graphics.*` 留在 `render/` 顶层。
- `views/` 与 `widgets/` 合并成 `widgets/` 并按用途分子目录：两者都是"控件"，只靠"是否外观驱动"分目录会让同类控件被拆到两个顶层目录里。
- `bklv/widgets/view_types.cpp` 是唯一新增文件：把"注册内置视图类型"从 `view_parse.cpp` 搬到控件层，消除 `view → widgets` 反向依赖（注册表 API 仍在 `view/json_view.hpp`）。
- `layer_draw_custom.cpp` 改动较大（去掉对 `AppearanceView` 的依赖），git 判定为 D + A，实际是 `widgets/layer_draw_custom.cpp` → `layer/layer_draw_custom.cpp`。

## 3 文件迁移清单

共 170 条移动（按新路径排序）。

| 原路径 | 新路径 | 模块 |
|---|---|---|
| `bklv/core/time.cpp` | `bklv/anim/time.cpp` | anim |
| `bklv/core/time.hpp` | `bklv/anim/time.hpp` | anim |
| `bklv/core/transition.hpp` | `bklv/anim/transition.hpp` | anim |
| `bklv/core/transition_player.cpp` | `bklv/anim/transition_player.cpp` | anim |
| `bklv/core/transition_player.hpp` | `bklv/anim/transition_player.hpp` | anim |
| `bklv/core/app.cpp` | `bklv/app/app.cpp` | app |
| `bklv/core/app.hpp` | `bklv/app/app.hpp` | app |
| `bklv/core/context.cpp` | `bklv/app/context.cpp` | app |
| `bklv/core/context.hpp` | `bklv/app/context.hpp` | app |
| `bklv/core/geometry.hpp` | `bklv/geom/geometry.hpp` | geom |
| `bklv/core/transform.hpp` | `bklv/geom/transform.hpp` | geom |
| `bklv/core/gamepad.cpp` | `bklv/input/gamepad.cpp` | input |
| `bklv/core/gamepad.hpp` | `bklv/input/gamepad.hpp` | input |
| `bklv/core/gesture.cpp` | `bklv/input/gesture.cpp` | input |
| `bklv/core/gesture.hpp` | `bklv/input/gesture.hpp` | input |
| `bklv/core/ime.cpp` | `bklv/input/ime.cpp` | input |
| `bklv/core/ime.hpp` | `bklv/input/ime.hpp` | input |
| `bklv/core/input.cpp` | `bklv/input/input.cpp` | input |
| `bklv/core/input.hpp` | `bklv/input/input.hpp` | input |
| `bklv/compile/json_policy.cpp` | `bklv/json/json_policy.cpp` | json |
| `bklv/compile/json_policy.hpp` | `bklv/json/json_policy.hpp` | json |
| `bklv/compile/json_util.cpp` | `bklv/json/json_util.cpp` | json |
| `bklv/compile/json_util.hpp` | `bklv/json/json_util.hpp` | json |
| `bklv/compile/json_vars.cpp` | `bklv/json/json_vars.cpp` | json |
| `bklv/compile/json_vars.hpp` | `bklv/json/json_vars.hpp` | json |
| `bklv/widgets/layer_draw.hpp` | `bklv/layer/layer_draw.hpp` | layer |
| `bklv/widgets/layer_draw_custom.cpp` | `bklv/layer/layer_draw_custom.cpp` | layer |
| `bklv/widgets/layer_draw_glass.cpp` | `bklv/layer/layer_draw_glass.cpp` | layer |
| `bklv/widgets/layer_draw_image.cpp` | `bklv/layer/layer_draw_image.cpp` | layer |
| `bklv/widgets/layer_draw_rect.cpp` | `bklv/layer/layer_draw_rect.cpp` | layer |
| `bklv/widgets/layer_draw_shadow.cpp` | `bklv/layer/layer_draw_shadow.cpp` | layer |
| `bklv/widgets/layer_draw_text.cpp` | `bklv/layer/layer_draw_text.cpp` | layer |
| `bklv/compile/layer_register.hpp` | `bklv/layer/layer_register.hpp` | layer |
| `bklv/core/audio.cpp` | `bklv/media/audio.cpp` | media |
| `bklv/core/audio.hpp` | `bklv/media/audio.hpp` | media |
| `bklv/core/font.cpp` | `bklv/media/font.cpp` | media |
| `bklv/core/font.hpp` | `bklv/media/font.hpp` | media |
| `bklv/core/icon_font.cpp` | `bklv/media/icon_font.cpp` | media |
| `bklv/core/icon_font.hpp` | `bklv/media/icon_font.hpp` | media |
| `bklv/core/image_decoder.cpp` | `bklv/media/image_decoder.cpp` | media |
| `bklv/core/image_decoder.hpp` | `bklv/media/image_decoder.hpp` | media |
| `bklv/core/image_decoder_gif.cpp` | `bklv/media/image_decoder_gif.cpp` | media |
| `bklv/core/image_decoder_svg.cpp` | `bklv/media/image_decoder_svg.cpp` | media |
| `bklv/core/image_decoder_webp.cpp` | `bklv/media/image_decoder_webp.cpp` | media |
| `bklv/core/image_writer.cpp` | `bklv/media/image_writer.cpp` | media |
| `bklv/core/image_writer.hpp` | `bklv/media/image_writer.hpp` | media |
| `bklv/core/sound_bank.cpp` | `bklv/media/sound_bank.cpp` | media |
| `bklv/core/sound_bank.hpp` | `bklv/media/sound_bank.hpp` | media |
| `bklv/core/focus_ring.cpp` | `bklv/paint/focus_ring.cpp` | paint |
| `bklv/core/focus_ring.hpp` | `bklv/paint/focus_ring.hpp` | paint |
| `bklv/core/gradient.cpp` | `bklv/paint/gradient.cpp` | paint |
| `bklv/core/gradient.hpp` | `bklv/paint/gradient.hpp` | paint |
| `bklv/core/shadow.cpp` | `bklv/paint/shadow.cpp` | paint |
| `bklv/core/shadow.hpp` | `bklv/paint/shadow.hpp` | paint |
| `bklv/platforms/sdl3_gamepad.cpp` | `bklv/platform/sdl3_gamepad.cpp` | platform |
| `bklv/platforms/sdl3_gamepad.hpp` | `bklv/platform/sdl3_gamepad.hpp` | platform |
| `bklv/platforms/sdl3_platform.cpp` | `bklv/platform/sdl3_platform.cpp` | platform |
| `bklv/platforms/sdl3_platform.hpp` | `bklv/platform/sdl3_platform.hpp` | platform |
| `bklv/platforms/switch_gamepad.cpp` | `bklv/platform/switch_gamepad.cpp` | platform |
| `bklv/platforms/switch_gamepad.hpp` | `bklv/platform/switch_gamepad.hpp` | platform |
| `bklv/platforms/switch_ime.cpp` | `bklv/platform/switch_ime.cpp` | platform |
| `bklv/platforms/switch_platform.cpp` | `bklv/platform/switch_platform.cpp` | platform |
| `bklv/platforms/switch_platform.hpp` | `bklv/platform/switch_platform.hpp` | platform |
| `bklv/backends/gl_loader.cpp` | `bklv/render/gl/gl_loader.cpp` | render |
| `bklv/backends/gl_loader.hpp` | `bklv/render/gl/gl_loader.hpp` | render |
| `bklv/backends/imgui_impl_opengl3_wrapper.cpp` | `bklv/render/gl/imgui_impl_opengl3_wrapper.cpp` | render |
| `bklv/backends/opengl_backend.cpp` | `bklv/render/gl/opengl_backend.cpp` | render |
| `bklv/backends/opengl_backend.hpp` | `bklv/render/gl/opengl_backend.hpp` | render |
| `bklv/backends/gl_glass.cpp` | `bklv/render/glass/gl/gl_glass.cpp` | render |
| `bklv/backends/gl_glass.hpp` | `bklv/render/glass/gl/gl_glass.hpp` | render |
| `bklv/backends/gl_glass_procs.cpp` | `bklv/render/glass/gl/gl_glass_procs.cpp` | render |
| `bklv/backends/gl_glass_procs.hpp` | `bklv/render/glass/gl/gl_glass_procs.hpp` | render |
| `bklv/backends/gl_glass_shaders.cpp` | `bklv/render/glass/gl/gl_glass_shaders.cpp` | render |
| `bklv/backends/gl_glass_shaders.hpp` | `bklv/render/glass/gl/gl_glass_shaders.hpp` | render |
| `bklv/core/glass/glass_backend.hpp` | `bklv/render/glass/glass_backend.hpp` | render |
| `bklv/core/glass/glass_blur_plan.cpp` | `bklv/render/glass/glass_blur_plan.cpp` | render |
| `bklv/core/glass/glass_blur_plan.hpp` | `bklv/render/glass/glass_blur_plan.hpp` | render |
| `bklv/core/glass/glass_geometry.cpp` | `bklv/render/glass/glass_geometry.cpp` | render |
| `bklv/core/glass/glass_geometry.hpp` | `bklv/render/glass/glass_geometry.hpp` | render |
| `bklv/core/glass/glass_queue.cpp` | `bklv/render/glass/glass_queue.cpp` | render |
| `bklv/core/glass/glass_queue.hpp` | `bklv/render/glass/glass_queue.hpp` | render |
| `bklv/core/glass/glass_request.hpp` | `bklv/render/glass/glass_request.hpp` | render |
| `bklv/core/glass/glass_service.cpp` | `bklv/render/glass/glass_service.cpp` | render |
| `bklv/core/glass/glass_service.hpp` | `bklv/render/glass/glass_service.hpp` | render |
| `bklv/core/glass/glass_style.hpp` | `bklv/render/glass/glass_style.hpp` | render |
| `bklv/core/glass/glass_tuning.cpp` | `bklv/render/glass/glass_tuning.cpp` | render |
| `bklv/core/glass/glass_tuning.hpp` | `bklv/render/glass/glass_tuning.hpp` | render |
| `bklv/backends/vk_glass.cpp` | `bklv/render/glass/vk/vk_glass.cpp` | render |
| `bklv/backends/vk_glass.hpp` | `bklv/render/glass/vk/vk_glass.hpp` | render |
| `bklv/backends/gles3_backend.cpp` | `bklv/render/gles3/gles3_backend.cpp` | render |
| `bklv/backends/gles3_backend.hpp` | `bklv/render/gles3/gles3_backend.hpp` | render |
| `bklv/backends/gles3_loader.hpp` | `bklv/render/gles3/gles3_loader.hpp` | render |
| `bklv/core/graphics.cpp` | `bklv/render/graphics.cpp` | render |
| `bklv/core/graphics.hpp` | `bklv/render/graphics.hpp` | render |
| `bklv/backends/imgui_impl_libnx.cpp` | `bklv/render/libnx/imgui_impl_libnx.cpp` | render |
| `bklv/backends/imgui_impl_libnx.hpp` | `bklv/render/libnx/imgui_impl_libnx.hpp` | render |
| `bklv/backends/vulkan_backend.cpp` | `bklv/render/vk/vulkan_backend.cpp` | render |
| `bklv/backends/vulkan_backend.hpp` | `bklv/render/vk/vulkan_backend.hpp` | render |
| `bklv/backends/vulkan_ui_pass.hpp` | `bklv/render/vk/vulkan_ui_pass.hpp` | render |
| `bklv/core/appearance.cpp` | `bklv/theme/appearance.cpp` | theme |
| `bklv/core/appearance.hpp` | `bklv/theme/appearance.hpp` | theme |
| `bklv/compile/appearance_box.cpp` | `bklv/theme/appearance_box.cpp` | theme |
| `bklv/compile/appearance_box.hpp` | `bklv/theme/appearance_box.hpp` | theme |
| `bklv/compile/appearance_children.cpp` | `bklv/theme/appearance_children.cpp` | theme |
| `bklv/compile/appearance_children.hpp` | `bklv/theme/appearance_children.hpp` | theme |
| `bklv/compile/appearance_gradient.cpp` | `bklv/theme/appearance_gradient.cpp` | theme |
| `bklv/compile/appearance_gradient.hpp` | `bklv/theme/appearance_gradient.hpp` | theme |
| `bklv/compile/appearance_imports.cpp` | `bklv/theme/appearance_imports.cpp` | theme |
| `bklv/compile/appearance_imports.hpp` | `bklv/theme/appearance_imports.hpp` | theme |
| `bklv/compile/appearance_layer_fields.cpp` | `bklv/theme/appearance_layer_fields.cpp` | theme |
| `bklv/compile/appearance_layer_fields.hpp` | `bklv/theme/appearance_layer_fields.hpp` | theme |
| `bklv/compile/appearance_layer_meta.cpp` | `bklv/theme/appearance_layer_meta.cpp` | theme |
| `bklv/compile/appearance_layer_meta.hpp` | `bklv/theme/appearance_layer_meta.hpp` | theme |
| `bklv/core/attributes.cpp` | `bklv/theme/attributes.cpp` | theme |
| `bklv/core/attributes.hpp` | `bklv/theme/attributes.hpp` | theme |
| `bklv/compile/layer_registry.cpp` | `bklv/theme/layer_registry.cpp` | theme |
| `bklv/compile/layer_registry.hpp` | `bklv/theme/layer_registry.hpp` | theme |
| `bklv/core/theme_data.hpp` | `bklv/theme/theme_data.hpp` | theme |
| `bklv/compile/theme_parse.cpp` | `bklv/theme/theme_parse.cpp` | theme |
| `bklv/core/box.cpp` | `bklv/view/box.cpp` | view |
| `bklv/core/box.hpp` | `bklv/view/box.hpp` | view |
| `bklv/core/json_actions.cpp` | `bklv/view/json_actions.cpp` | view |
| `bklv/core/json_actions.hpp` | `bklv/view/json_actions.hpp` | view |
| `bklv/core/json_data.cpp` | `bklv/view/json_data.cpp` | view |
| `bklv/core/json_data.hpp` | `bklv/view/json_data.hpp` | view |
| `bklv/core/json_view.hpp` | `bklv/view/json_view.hpp` | view |
| `bklv/core/view.cpp` | `bklv/view/view.cpp` | view |
| `bklv/core/view.hpp` | `bklv/view/view.hpp` | view |
| `bklv/compile/view_parse.cpp` | `bklv/view/view_parse.cpp` | view |
| `bklv/widgets/appearance_children.cpp` | `bklv/widgets/appearance/appearance_children.cpp` | widgets |
| `bklv/widgets/appearance_children.hpp` | `bklv/widgets/appearance/appearance_children.hpp` | widgets |
| `bklv/widgets/appearance_view.cpp` | `bklv/widgets/appearance/appearance_view.cpp` | widgets |
| `bklv/widgets/appearance_view.hpp` | `bklv/widgets/appearance/appearance_view.hpp` | widgets |
| `bklv/widgets/content_ids.hpp` | `bklv/widgets/appearance/content_ids.hpp` | widgets |
| `bklv/widgets/widget_types.cpp` | `bklv/widgets/appearance/widget_types.cpp` | widgets |
| `bklv/widgets/widget_types.hpp` | `bklv/widgets/appearance/widget_types.hpp` | widgets |
| `bklv/views/image.cpp` | `bklv/widgets/basic/image.cpp` | widgets |
| `bklv/views/image.hpp` | `bklv/widgets/basic/image.hpp` | widgets |
| `bklv/views/label.cpp` | `bklv/widgets/basic/label.cpp` | widgets |
| `bklv/views/label.hpp` | `bklv/widgets/basic/label.hpp` | widgets |
| `bklv/views/padding.cpp` | `bklv/widgets/basic/padding.cpp` | widgets |
| `bklv/views/padding.hpp` | `bklv/widgets/basic/padding.hpp` | widgets |
| `bklv/views/progress_spinner.cpp` | `bklv/widgets/basic/progress_spinner.cpp` | widgets |
| `bklv/views/progress_spinner.hpp` | `bklv/widgets/basic/progress_spinner.hpp` | widgets |
| `bklv/views/h_scrolling_frame.cpp` | `bklv/widgets/container/h_scrolling_frame.cpp` | widgets |
| `bklv/views/h_scrolling_frame.hpp` | `bklv/widgets/container/h_scrolling_frame.hpp` | widgets |
| `bklv/views/scroll_box.cpp` | `bklv/widgets/container/scroll_box.cpp` | widgets |
| `bklv/views/scroll_box.hpp` | `bklv/widgets/container/scroll_box.hpp` | widgets |
| `bklv/views/scrolling_frame.cpp` | `bklv/widgets/container/scrolling_frame.cpp` | widgets |
| `bklv/views/scrolling_frame.hpp` | `bklv/widgets/container/scrolling_frame.hpp` | widgets |
| `bklv/widgets/button.cpp` | `bklv/widgets/controls/button.cpp` | widgets |
| `bklv/widgets/button.hpp` | `bklv/widgets/controls/button.hpp` | widgets |
| `bklv/widgets/notification.cpp` | `bklv/widgets/controls/notification.cpp` | widgets |
| `bklv/widgets/notification.hpp` | `bklv/widgets/controls/notification.hpp` | widgets |
| `bklv/views/debug_layer.cpp` | `bklv/widgets/debug/debug_layer.cpp` | widgets |
| `bklv/views/debug_layer.hpp` | `bklv/widgets/debug/debug_layer.hpp` | widgets |
| `bklv/views/debug_log_view.cpp` | `bklv/widgets/debug/debug_log_view.cpp` | widgets |
| `bklv/views/debug_log_view.hpp` | `bklv/widgets/debug/debug_log_view.hpp` | widgets |
| `bklv/views/fps_overlay.cpp` | `bklv/widgets/debug/fps_overlay.cpp` | widgets |
| `bklv/views/fps_overlay.hpp` | `bklv/widgets/debug/fps_overlay.hpp` | widgets |
| `bklv/views/preset_backdrop.cpp` | `bklv/widgets/debug/preset_backdrop.cpp` | widgets |
| `bklv/views/preset_backdrop.hpp` | `bklv/widgets/debug/preset_backdrop.hpp` | widgets |
| `bklv/views/data_source.cpp` | `bklv/widgets/list/data_source.cpp` | widgets |
| `bklv/views/data_source.hpp` | `bklv/widgets/list/data_source.hpp` | widgets |
| `bklv/views/item_binder.cpp` | `bklv/widgets/list/item_binder.cpp` | widgets |
| `bklv/views/item_binder.hpp` | `bklv/widgets/list/item_binder.hpp` | widgets |
| `bklv/views/json_cell.cpp` | `bklv/widgets/list/json_cell.cpp` | widgets |
| `bklv/views/json_cell.hpp` | `bklv/widgets/list/json_cell.hpp` | widgets |
| `bklv/views/recycler.cpp` | `bklv/widgets/list/recycler.cpp` | widgets |
| `bklv/views/recycler.hpp` | `bklv/widgets/list/recycler.hpp` | widgets |

新增文件：

- `bklv/widgets/view_types.cpp`（内置视图类型注册入口）

原地修改（路径没变，含依赖改写或依赖消解）：

- `bklv/3rd/tweeny`
- `bklv/CMakeLists.txt`
- `bklv/bklv.hpp`
- `bklv/core/decode_queue.cpp`
- `bklv/core/frame_budget.cpp`
- `bklv/core/platform.cpp`
- `bklv/core/platform.hpp`
- `bklv/core/services.hpp`

## 4 架构与依赖说明

```
core / geom / json                       最底层（不认识任何上层）
anim / media / paint / render / input    → core, geom, json
theme                                    → core, geom, json, anim, media
layer                                    → core, theme（层绘制只认图层表与绘制上下文）
view                                     → core, geom, anim, theme, media, input
app                                      → core, view, theme, media, input, render, widgets（组合根）
platform                                 → core, input, render
widgets                                  → 除 platform / render 外的所有层
任何一层                                 → 3rd（第三方是叶子）
```

`scripts/check_layers.py` 逐对强制上表。刻意放行的三条：

1. `app → widgets`：组合根要创建通知层与调试层，属正常下游使用。
2. `core → platform` / `core → render`：工厂反转（`core/platform.cpp` 按编译期注册表创建实现），平台/后端实现 `core/platform.hpp` 声明的接口。
3. 存量 4 条反向依赖（BASELINE 白名单，消解一条要删一行）：

| 方向 | 文件 | 原因 |
|---|---|---|
| theme → view | `bklv/theme/appearance.cpp` | 外观要把 box 段套到视图节点上 |
| theme → view | `bklv/theme/attributes.cpp` | 属性表 handler 直接写 `View` 的 setter |
| media → theme | `bklv/media/sound_bank.cpp` | 音效表读取主题里的 `sound.json` 与 `@theme/` 路径 |
| json → theme | `bklv/json/json_util.hpp` | 变量展开要解析 `@theme/` 引用 |

公共接口与内部实现的边界：`bklv/bklv.hpp` 只 include 各模块的公开头，且不许顺着 include 碰具体后端（`render/vk/`、`render/gl/`、`render/gles3/`、`render/libnx/`、`render/glass/{gl,vk}/`）、`platform/` 或 `<vulkan/vulkan.h>`；这条由 `check_layers.py` 的伞头可达性检查强制。

平台实现 vs 兼容层：`platform/` 只有真正存在的两个平台（SDL3 与 libnx）；后端差异（GL 原型加载、GLES3、Vulkan、libnx 的 ImGui 后端）在 `render/*/` 里，两者都不进 `core/`。新代码归档规则：先判断"属于哪个职责"，再落到对应模块；跨层的能力（如玻璃）放使用方所在的层（`render/glass/`），不留在 `core/`。

## 5 构建与测试结果

| 配置 | 命令 | 结果 |
|---|---|---|
| MSVC 14.44 · GL | `cmake -S . -B build/gl -G "Visual Studio 17 2022"` + `cmake --build build/gl --config Release` | 0 error / 0 warning；`dist/embed_imgui_gl.exe` 4,645,888 B、`dist/multi_context.exe` 4,595,952 B |
| MSVC 14.44 · Vulkan | `cmake -S . -B build/vk -G "Visual Studio 17 2022" -DBKLV_GRAPHICS=vk` + build | 0 error / 0 warning |
| MinGW GCC 16.2 · GL | `BKLV_CMAKE_GENERATOR=Ninja BKLV_BUILD_DIR=build/mingw-gl bash scripts/ci_build.sh gl` | 0 error / 0 warning |
| Switch · devkitA64 | `./scripts/build_switch.sh` | 编译 + 链接通过，`dist/switch/bklv_switch.nro` 9.1 MB（romfs 63 个资源文件 / 862,904 B） |
| 本地门禁 | `bash scripts/check.sh` | 命名 200 文件通过 / 分层 4 条白名单无新增 / 主题 2 套 41 份外观一致 / GL 与 VK 零警告 / 全部检查通过 |
| 运行时 | `dist/embed_imgui_gl.exe`、`dist/multi_context.exe` 各跑 8 秒 | 主题 113 色 / 166 数值 / 41 控件外观装载成功、文案 334 条、字体与图标字体加载成功、stderr 0 行 |

未执行项：真机 Switch 运行（本机无设备，只验证编译与打包）。

## 6 质量自检

- [x] 每个文件都有明确模块归属（199 个文件全部归类，0 个 misc/common）
- [x] 目录树反映真实边界（`widgets/` 子目录按用途而非父类拆分层级）
- [x] 依赖方向单向（`check_layers.py` 逐对强制，仅保留 4 条有据白名单）
- [x] 没有为分层造空接口 / 转发类（`layer/` 与 `theme/` 的拆分按真实数据模型切）
- [x] C++ 公共 API 未改（类名、命名空间、函数签名、头文件名不变）
- [x] 伞头 `bklv/bklv.hpp` 只 include 新路径，未引入后端/平台头
- [x] CMake 源清单、漏项检查目录、`scripts/*.sh`、CI workflow 全部跟上新路径
- [x] `scripts/check_naming.py` / `check_layers.py` / `check_theme.py` 通过
- [x] 示例程序编译并运行（GL 桌面、两个宿主）
- [x] Vulkan 后端编译通过（MSVC）
- [x] Switch 编译 + 打包通过
- [x] 资源加载路径未变（`resources/` 相对可执行文件目录，romfs 保留 `resources/` 一层）
- [x] 没有遗留旧路径引用（`grep` 旧目录名 = 0）
- [x] 没有重复源文件（`imgui_impl_opengl3_wrapper.cpp` 只在一个清单里）
- [x] 文档同步（README / AGENTS.md / docs/*.md / 生成脚本 / 本报告）

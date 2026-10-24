# 目录架构重构（2026-10）

只动**目录与 include 路径**：公共 API、类名、命名空间、控件行为、布局结果、渲染顺序、资源加载路径全部未改。`bklv::compile` / `bklv::layers` 这两个命名空间名字保留（改名会破坏公开 API），目录按职责拆开后在头文件注释里说明了差异。

## 1 最终目录树与模块职责

```
bklv/
  bklv.hpp                 伞头（按大类分节，只暴露稳定的公开头）
  foundation/              基础：不认识任何界面概念
    core/  geom/  json/  anim/
  graphics/                绘制：只认 ImDrawList 与渲染接口
    paint/  render/
        gl/  vk/  gles3/  libnx/        四个后端实现
        glass/  glass/gl/  glass/vk/    玻璃子系统（跨后端 + 后端实现）
  ui/                      界面：视图树、外观、控件、输入
    view/  theme/  layer/  input/  app/
    widgets/
        appearance/  controls/  basic/  container/  list/  debug/
        view_types.cpp     内置视图类型注册入口
  system/                  运行时支撑
    media/  platform/
```

四个大类分别对应"不认识界面 / 只会画 / 界面 / 平台与素材"四种职责；大类的选择标准是**依赖方向**而不是数量（`foundation` 与 `graphics` 谁都不认识上层，`ui` 是唯一会反向依赖它们的层，`system` 是平台与素材的落地处）。模块名在全库唯一，所以分层门禁按**模块名**判定，`layer_of()` 取目录的第二级。

| 模块 | 文件 | 职责 |
|---|---|---|
| `foundation/core/` | 28 | 基础机制：全局状态登记、资源路径、日志、任务与事件、服务、UI 缩放、帧预算与帧统计、解码队列、i18n、平台接口 |
| `foundation/geom/` | 2 | 几何：Point / Size / Rect、绘制期变换 |
| `graphics/paint/` | 6 | 绘制效果：渐变顶点网格、柔化阴影（外投影 / 内阴影 / cutout）、焦点框形状与 LUT 采样 |
| `foundation/anim/` | 5 | 动画：缓动与补间、视图转场、外观 transition 通道播放器 |
| `foundation/json/` | 6 | JSON 工具：取值与诊断、变量块展开（`&` / `@`）、错误策略 |
| `ui/theme/` | 20 | 外观与主题（领域）：ThemeData 与主题解析、Appearance 编译（box / layers / states / import）、图层类型表与参数表 |
| `ui/layer/` | 8 | 图层绘制（领域）：六个内置层的绘制实现、第三方层注册模板 |
| `ui/view/` | 10 | 视图：View / Box（Yoga 布局）/ ViewTemplate 两段式、JSON 数据与动作绑定 |
| `ui/app/` | 4 | 应用：App 帧循环、Context、通知层 |
| `system/media/` | 15 | 媒体：字体与图标字体、图片解码与写出（PNG/JPEG/GIF/WebP/SVG）、音频与音效表 |
| `ui/input/` | 8 | 输入：键盘 / 手柄 / 手势、焦点与模态栈、IME 模型 |
| `graphics/render/` | 36 | 渲染：RenderBackend 接口 + gl / vk / gles3 / libnx 四个后端 + glass/ 玻璃子系统 |
| `system/platform/` | 9 | 平台实现：SDL3（macOS / Linux / Windows）/ libnx（Switch） |
| `ui/widgets/` | 42 | 控件：appearance（外观驱动基类）/ controls / basic / container / list / debug |
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

- `compile/` 拆成三层职责：`foundation/json/`（通用 JSON 工具）、`ui/theme/`（外观与主题解析）、`ui/view/`（视图描述解析）。原因：`theme_parse` 要写 `ThemeData`、`view_parse` 要注册视图类型，两者与 JSON 工具混在一起时 `theme → layer` 与 `layer → theme` 形成模块级环。
- `foundation/core/glass/` 整组移到 `graphics/render/glass/`（跨后端部分留在顶层，`gl/` 与 `vk/` 是后端实现）：玻璃是渲染能力，不属于核心机制。
- `backends/` 按 API 拆成 `graphics/render/gl/`、`graphics/render/vk/`、`graphics/render/gles3/`、`graphics/render/libnx/`，接口 `graphics/render/graphics.*` 留在 `graphics/render/` 顶层。
- `views/` 与 `ui/widgets/` 合并成 `ui/widgets/` 并按用途分子目录：两者都是"控件"，只靠"是否外观驱动"分目录会让同类控件被拆到两个顶层目录里。
- `bklv/ui/widgets/view_types.cpp` 是唯一新增文件：把"注册内置视图类型"从 `view_parse.cpp` 搬到控件层，消除 `view → widgets` 反向依赖（注册表 API 仍在 `ui/view/json_view.hpp`）。
- `layer_draw_custom.cpp` 改动较大（去掉对 `AppearanceView` 的依赖），git 判定为 D + A，实际是 `ui/widgets/layer_draw_custom.cpp` → `ui/layer/layer_draw_custom.cpp`。

## 3 文件迁移清单

> 二级收拢（用户要求"分的目录太多了可以分大类然后分小类"）：上面这 14 个模块在定稿后又整体收进 4 个大类
> （`foundation/` `graphics/` `ui/` `system/`）。这一步只有 14 次目录 `git mv`、**没有文件跨模块移动**，
> 187 个文件的 include 路径加了 `大类/` 前缀（1078 处），公共 API 与行为不变。
> 门禁、CMake 清单、`scripts/build_switch.sh`、伞头与文档同步改过；`scripts/check_layers.py` 的规则表
> 仍按模块名书写，只是 `layer_of()` 从"第一级目录"改成"第二级目录"。

共 170 条移动（按新路径排序）。

| 原路径 | 新路径 | 模块 |
|---|---|---|
| `bklv/foundation/core/time.cpp` | `bklv/foundation/anim/time.cpp` | anim |
| `bklv/foundation/core/time.hpp` | `bklv/foundation/anim/time.hpp` | anim |
| `bklv/foundation/core/transition.hpp` | `bklv/foundation/anim/transition.hpp` | anim |
| `bklv/foundation/core/transition_player.cpp` | `bklv/foundation/anim/transition_player.cpp` | anim |
| `bklv/foundation/core/transition_player.hpp` | `bklv/foundation/anim/transition_player.hpp` | anim |
| `bklv/foundation/core/app.cpp` | `bklv/ui/app/app.cpp` | app |
| `bklv/foundation/core/app.hpp` | `bklv/ui/app/app.hpp` | app |
| `bklv/foundation/core/context.cpp` | `bklv/ui/app/context.cpp` | app |
| `bklv/foundation/core/context.hpp` | `bklv/ui/app/context.hpp` | app |
| `bklv/foundation/core/geometry.hpp` | `bklv/foundation/geom/geometry.hpp` | geom |
| `bklv/foundation/core/transform.hpp` | `bklv/foundation/geom/transform.hpp` | geom |
| `bklv/foundation/core/gamepad.cpp` | `bklv/ui/input/gamepad.cpp` | input |
| `bklv/foundation/core/gamepad.hpp` | `bklv/ui/input/gamepad.hpp` | input |
| `bklv/foundation/core/gesture.cpp` | `bklv/ui/input/gesture.cpp` | input |
| `bklv/foundation/core/gesture.hpp` | `bklv/ui/input/gesture.hpp` | input |
| `bklv/foundation/core/ime.cpp` | `bklv/ui/input/ime.cpp` | input |
| `bklv/foundation/core/ime.hpp` | `bklv/ui/input/ime.hpp` | input |
| `bklv/foundation/core/input.cpp` | `bklv/ui/input/input.cpp` | input |
| `bklv/foundation/core/input.hpp` | `bklv/ui/input/input.hpp` | input |
| `bklv/compile/json_policy.cpp` | `bklv/foundation/json/json_policy.cpp` | json |
| `bklv/compile/json_policy.hpp` | `bklv/foundation/json/json_policy.hpp` | json |
| `bklv/compile/json_util.cpp` | `bklv/foundation/json/json_util.cpp` | json |
| `bklv/compile/json_util.hpp` | `bklv/foundation/json/json_util.hpp` | json |
| `bklv/compile/json_vars.cpp` | `bklv/foundation/json/json_vars.cpp` | json |
| `bklv/compile/json_vars.hpp` | `bklv/foundation/json/json_vars.hpp` | json |
| `bklv/ui/widgets/layer_draw.hpp` | `bklv/ui/layer/layer_draw.hpp` | layer |
| `bklv/ui/widgets/layer_draw_custom.cpp` | `bklv/ui/layer/layer_draw_custom.cpp` | layer |
| `bklv/ui/widgets/layer_draw_glass.cpp` | `bklv/ui/layer/layer_draw_glass.cpp` | layer |
| `bklv/ui/widgets/layer_draw_image.cpp` | `bklv/ui/layer/layer_draw_image.cpp` | layer |
| `bklv/ui/widgets/layer_draw_rect.cpp` | `bklv/ui/layer/layer_draw_rect.cpp` | layer |
| `bklv/ui/widgets/layer_draw_shadow.cpp` | `bklv/ui/layer/layer_draw_shadow.cpp` | layer |
| `bklv/ui/widgets/layer_draw_text.cpp` | `bklv/ui/layer/layer_draw_text.cpp` | layer |
| `bklv/compile/layer_register.hpp` | `bklv/ui/layer/layer_register.hpp` | layer |
| `bklv/foundation/core/audio.cpp` | `bklv/system/media/audio.cpp` | media |
| `bklv/foundation/core/audio.hpp` | `bklv/system/media/audio.hpp` | media |
| `bklv/foundation/core/font.cpp` | `bklv/system/media/font.cpp` | media |
| `bklv/foundation/core/font.hpp` | `bklv/system/media/font.hpp` | media |
| `bklv/foundation/core/icon_font.cpp` | `bklv/system/media/icon_font.cpp` | media |
| `bklv/foundation/core/icon_font.hpp` | `bklv/system/media/icon_font.hpp` | media |
| `bklv/foundation/core/image_decoder.cpp` | `bklv/system/media/image_decoder.cpp` | media |
| `bklv/foundation/core/image_decoder.hpp` | `bklv/system/media/image_decoder.hpp` | media |
| `bklv/foundation/core/image_decoder_gif.cpp` | `bklv/system/media/image_decoder_gif.cpp` | media |
| `bklv/foundation/core/image_decoder_svg.cpp` | `bklv/system/media/image_decoder_svg.cpp` | media |
| `bklv/foundation/core/image_decoder_webp.cpp` | `bklv/system/media/image_decoder_webp.cpp` | media |
| `bklv/foundation/core/image_writer.cpp` | `bklv/system/media/image_writer.cpp` | media |
| `bklv/foundation/core/image_writer.hpp` | `bklv/system/media/image_writer.hpp` | media |
| `bklv/foundation/core/sound_bank.cpp` | `bklv/system/media/sound_bank.cpp` | media |
| `bklv/foundation/core/sound_bank.hpp` | `bklv/system/media/sound_bank.hpp` | media |
| `bklv/foundation/core/focus_ring.cpp` | `bklv/graphics/paint/focus_ring.cpp` | paint |
| `bklv/foundation/core/focus_ring.hpp` | `bklv/graphics/paint/focus_ring.hpp` | paint |
| `bklv/foundation/core/gradient.cpp` | `bklv/graphics/paint/gradient.cpp` | paint |
| `bklv/foundation/core/gradient.hpp` | `bklv/graphics/paint/gradient.hpp` | paint |
| `bklv/foundation/core/shadow.cpp` | `bklv/graphics/paint/shadow.cpp` | paint |
| `bklv/foundation/core/shadow.hpp` | `bklv/graphics/paint/shadow.hpp` | paint |
| `bklv/platforms/sdl3_gamepad.cpp` | `bklv/system/platform/sdl3_gamepad.cpp` | platform |
| `bklv/platforms/sdl3_gamepad.hpp` | `bklv/system/platform/sdl3_gamepad.hpp` | platform |
| `bklv/platforms/sdl3_platform.cpp` | `bklv/system/platform/sdl3_platform.cpp` | platform |
| `bklv/platforms/sdl3_platform.hpp` | `bklv/system/platform/sdl3_platform.hpp` | platform |
| `bklv/platforms/switch_gamepad.cpp` | `bklv/system/platform/switch_gamepad.cpp` | platform |
| `bklv/platforms/switch_gamepad.hpp` | `bklv/system/platform/switch_gamepad.hpp` | platform |
| `bklv/platforms/switch_ime.cpp` | `bklv/system/platform/switch_ime.cpp` | platform |
| `bklv/platforms/switch_platform.cpp` | `bklv/system/platform/switch_platform.cpp` | platform |
| `bklv/platforms/switch_platform.hpp` | `bklv/system/platform/switch_platform.hpp` | platform |
| `bklv/backends/gl_loader.cpp` | `bklv/graphics/render/gl/gl_loader.cpp` | render |
| `bklv/backends/gl_loader.hpp` | `bklv/graphics/render/gl/gl_loader.hpp` | render |
| `bklv/backends/imgui_impl_opengl3_wrapper.cpp` | `bklv/graphics/render/gl/imgui_impl_opengl3_wrapper.cpp` | render |
| `bklv/backends/opengl_backend.cpp` | `bklv/graphics/render/gl/opengl_backend.cpp` | render |
| `bklv/backends/opengl_backend.hpp` | `bklv/graphics/render/gl/opengl_backend.hpp` | render |
| `bklv/backends/gl_glass.cpp` | `bklv/graphics/render/glass/gl/gl_glass.cpp` | render |
| `bklv/backends/gl_glass.hpp` | `bklv/graphics/render/glass/gl/gl_glass.hpp` | render |
| `bklv/backends/gl_glass_procs.cpp` | `bklv/graphics/render/glass/gl/gl_glass_procs.cpp` | render |
| `bklv/backends/gl_glass_procs.hpp` | `bklv/graphics/render/glass/gl/gl_glass_procs.hpp` | render |
| `bklv/backends/gl_glass_shaders.cpp` | `bklv/graphics/render/glass/gl/gl_glass_shaders.cpp` | render |
| `bklv/backends/gl_glass_shaders.hpp` | `bklv/graphics/render/glass/gl/gl_glass_shaders.hpp` | render |
| `bklv/foundation/core/glass/glass_backend.hpp` | `bklv/graphics/render/glass/glass_backend.hpp` | render |
| `bklv/foundation/core/glass/glass_blur_plan.cpp` | `bklv/graphics/render/glass/glass_blur_plan.cpp` | render |
| `bklv/foundation/core/glass/glass_blur_plan.hpp` | `bklv/graphics/render/glass/glass_blur_plan.hpp` | render |
| `bklv/foundation/core/glass/glass_geometry.cpp` | `bklv/graphics/render/glass/glass_geometry.cpp` | render |
| `bklv/foundation/core/glass/glass_geometry.hpp` | `bklv/graphics/render/glass/glass_geometry.hpp` | render |
| `bklv/foundation/core/glass/glass_queue.cpp` | `bklv/graphics/render/glass/glass_queue.cpp` | render |
| `bklv/foundation/core/glass/glass_queue.hpp` | `bklv/graphics/render/glass/glass_queue.hpp` | render |
| `bklv/foundation/core/glass/glass_request.hpp` | `bklv/graphics/render/glass/glass_request.hpp` | render |
| `bklv/foundation/core/glass/glass_service.cpp` | `bklv/graphics/render/glass/glass_service.cpp` | render |
| `bklv/foundation/core/glass/glass_service.hpp` | `bklv/graphics/render/glass/glass_service.hpp` | render |
| `bklv/foundation/core/glass/glass_style.hpp` | `bklv/graphics/render/glass/glass_style.hpp` | render |
| `bklv/foundation/core/glass/glass_tuning.cpp` | `bklv/graphics/render/glass/glass_tuning.cpp` | render |
| `bklv/foundation/core/glass/glass_tuning.hpp` | `bklv/graphics/render/glass/glass_tuning.hpp` | render |
| `bklv/backends/vk_glass.cpp` | `bklv/graphics/render/glass/vk/vk_glass.cpp` | render |
| `bklv/backends/vk_glass.hpp` | `bklv/graphics/render/glass/vk/vk_glass.hpp` | render |
| `bklv/backends/gles3_backend.cpp` | `bklv/graphics/render/gles3/gles3_backend.cpp` | render |
| `bklv/backends/gles3_backend.hpp` | `bklv/graphics/render/gles3/gles3_backend.hpp` | render |
| `bklv/backends/gles3_loader.hpp` | `bklv/graphics/render/gles3/gles3_loader.hpp` | render |
| `bklv/foundation/core/graphics.cpp` | `bklv/graphics/render/graphics.cpp` | render |
| `bklv/foundation/core/graphics.hpp` | `bklv/graphics/render/graphics.hpp` | render |
| `bklv/backends/imgui_impl_libnx.cpp` | `bklv/graphics/render/libnx/imgui_impl_libnx.cpp` | render |
| `bklv/backends/imgui_impl_libnx.hpp` | `bklv/graphics/render/libnx/imgui_impl_libnx.hpp` | render |
| `bklv/backends/vulkan_backend.cpp` | `bklv/graphics/render/vk/vulkan_backend.cpp` | render |
| `bklv/backends/vulkan_backend.hpp` | `bklv/graphics/render/vk/vulkan_backend.hpp` | render |
| `bklv/backends/vulkan_ui_pass.hpp` | `bklv/graphics/render/vk/vulkan_ui_pass.hpp` | render |
| `bklv/foundation/core/appearance.cpp` | `bklv/ui/theme/appearance.cpp` | theme |
| `bklv/foundation/core/appearance.hpp` | `bklv/ui/theme/appearance.hpp` | theme |
| `bklv/compile/appearance_box.cpp` | `bklv/ui/theme/appearance_box.cpp` | theme |
| `bklv/compile/appearance_box.hpp` | `bklv/ui/theme/appearance_box.hpp` | theme |
| `bklv/compile/appearance_children.cpp` | `bklv/ui/theme/appearance_children.cpp` | theme |
| `bklv/compile/appearance_children.hpp` | `bklv/ui/theme/appearance_children.hpp` | theme |
| `bklv/compile/appearance_gradient.cpp` | `bklv/ui/theme/appearance_gradient.cpp` | theme |
| `bklv/compile/appearance_gradient.hpp` | `bklv/ui/theme/appearance_gradient.hpp` | theme |
| `bklv/compile/appearance_imports.cpp` | `bklv/ui/theme/appearance_imports.cpp` | theme |
| `bklv/compile/appearance_imports.hpp` | `bklv/ui/theme/appearance_imports.hpp` | theme |
| `bklv/compile/appearance_layer_fields.cpp` | `bklv/ui/theme/appearance_layer_fields.cpp` | theme |
| `bklv/compile/appearance_layer_fields.hpp` | `bklv/ui/theme/appearance_layer_fields.hpp` | theme |
| `bklv/compile/appearance_layer_meta.cpp` | `bklv/ui/theme/appearance_layer_meta.cpp` | theme |
| `bklv/compile/appearance_layer_meta.hpp` | `bklv/ui/theme/appearance_layer_meta.hpp` | theme |
| `bklv/foundation/core/attributes.cpp` | `bklv/ui/theme/attributes.cpp` | theme |
| `bklv/foundation/core/attributes.hpp` | `bklv/ui/theme/attributes.hpp` | theme |
| `bklv/compile/layer_registry.cpp` | `bklv/ui/theme/layer_registry.cpp` | theme |
| `bklv/compile/layer_registry.hpp` | `bklv/ui/theme/layer_registry.hpp` | theme |
| `bklv/foundation/core/theme_data.hpp` | `bklv/ui/theme/theme_data.hpp` | theme |
| `bklv/compile/theme_parse.cpp` | `bklv/ui/theme/theme_parse.cpp` | theme |
| `bklv/foundation/core/box.cpp` | `bklv/ui/view/box.cpp` | view |
| `bklv/foundation/core/box.hpp` | `bklv/ui/view/box.hpp` | view |
| `bklv/foundation/core/json_actions.cpp` | `bklv/ui/view/json_actions.cpp` | view |
| `bklv/foundation/core/json_actions.hpp` | `bklv/ui/view/json_actions.hpp` | view |
| `bklv/foundation/core/json_data.cpp` | `bklv/ui/view/json_data.cpp` | view |
| `bklv/foundation/core/json_data.hpp` | `bklv/ui/view/json_data.hpp` | view |
| `bklv/foundation/core/json_view.hpp` | `bklv/ui/view/json_view.hpp` | view |
| `bklv/foundation/core/view.cpp` | `bklv/ui/view/view.cpp` | view |
| `bklv/foundation/core/view.hpp` | `bklv/ui/view/view.hpp` | view |
| `bklv/compile/view_parse.cpp` | `bklv/ui/view/view_parse.cpp` | view |
| `bklv/ui/widgets/appearance_children.cpp` | `bklv/ui/widgets/appearance/appearance_children.cpp` | widgets |
| `bklv/ui/widgets/appearance_children.hpp` | `bklv/ui/widgets/appearance/appearance_children.hpp` | widgets |
| `bklv/ui/widgets/appearance_view.cpp` | `bklv/ui/widgets/appearance/appearance_view.cpp` | widgets |
| `bklv/ui/widgets/appearance_view.hpp` | `bklv/ui/widgets/appearance/appearance_view.hpp` | widgets |
| `bklv/ui/widgets/content_ids.hpp` | `bklv/ui/widgets/appearance/content_ids.hpp` | widgets |
| `bklv/ui/widgets/widget_types.cpp` | `bklv/ui/widgets/appearance/widget_types.cpp` | widgets |
| `bklv/ui/widgets/widget_types.hpp` | `bklv/ui/widgets/appearance/widget_types.hpp` | widgets |
| `bklv/views/image.cpp` | `bklv/ui/widgets/basic/image.cpp` | widgets |
| `bklv/views/image.hpp` | `bklv/ui/widgets/basic/image.hpp` | widgets |
| `bklv/views/label.cpp` | `bklv/ui/widgets/basic/label.cpp` | widgets |
| `bklv/views/label.hpp` | `bklv/ui/widgets/basic/label.hpp` | widgets |
| `bklv/views/padding.cpp` | `bklv/ui/widgets/basic/padding.cpp` | widgets |
| `bklv/views/padding.hpp` | `bklv/ui/widgets/basic/padding.hpp` | widgets |
| `bklv/views/progress_spinner.cpp` | `bklv/ui/widgets/basic/progress_spinner.cpp` | widgets |
| `bklv/views/progress_spinner.hpp` | `bklv/ui/widgets/basic/progress_spinner.hpp` | widgets |
| `bklv/views/h_scrolling_frame.cpp` | `bklv/ui/widgets/container/h_scrolling_frame.cpp` | widgets |
| `bklv/views/h_scrolling_frame.hpp` | `bklv/ui/widgets/container/h_scrolling_frame.hpp` | widgets |
| `bklv/views/scroll_box.cpp` | `bklv/ui/widgets/container/scroll_box.cpp` | widgets |
| `bklv/views/scroll_box.hpp` | `bklv/ui/widgets/container/scroll_box.hpp` | widgets |
| `bklv/views/scrolling_frame.cpp` | `bklv/ui/widgets/container/scrolling_frame.cpp` | widgets |
| `bklv/views/scrolling_frame.hpp` | `bklv/ui/widgets/container/scrolling_frame.hpp` | widgets |
| `bklv/ui/widgets/button.cpp` | `bklv/ui/widgets/controls/button.cpp` | widgets |
| `bklv/ui/widgets/button.hpp` | `bklv/ui/widgets/controls/button.hpp` | widgets |
| `bklv/ui/widgets/notification.cpp` | `bklv/ui/widgets/controls/notification.cpp` | widgets |
| `bklv/ui/widgets/notification.hpp` | `bklv/ui/widgets/controls/notification.hpp` | widgets |
| `bklv/views/debug_layer.cpp` | `bklv/ui/widgets/debug/debug_layer.cpp` | widgets |
| `bklv/views/debug_layer.hpp` | `bklv/ui/widgets/debug/debug_layer.hpp` | widgets |
| `bklv/views/debug_log_view.cpp` | `bklv/ui/widgets/debug/debug_log_view.cpp` | widgets |
| `bklv/views/debug_log_view.hpp` | `bklv/ui/widgets/debug/debug_log_view.hpp` | widgets |
| `bklv/views/fps_overlay.cpp` | `bklv/ui/widgets/debug/fps_overlay.cpp` | widgets |
| `bklv/views/fps_overlay.hpp` | `bklv/ui/widgets/debug/fps_overlay.hpp` | widgets |
| `bklv/views/preset_backdrop.cpp` | `bklv/ui/widgets/debug/preset_backdrop.cpp` | widgets |
| `bklv/views/preset_backdrop.hpp` | `bklv/ui/widgets/debug/preset_backdrop.hpp` | widgets |
| `bklv/views/data_source.cpp` | `bklv/ui/widgets/list/data_source.cpp` | widgets |
| `bklv/views/data_source.hpp` | `bklv/ui/widgets/list/data_source.hpp` | widgets |
| `bklv/views/item_binder.cpp` | `bklv/ui/widgets/list/item_binder.cpp` | widgets |
| `bklv/views/item_binder.hpp` | `bklv/ui/widgets/list/item_binder.hpp` | widgets |
| `bklv/views/json_cell.cpp` | `bklv/ui/widgets/list/json_cell.cpp` | widgets |
| `bklv/views/json_cell.hpp` | `bklv/ui/widgets/list/json_cell.hpp` | widgets |
| `bklv/views/recycler.cpp` | `bklv/ui/widgets/list/recycler.cpp` | widgets |
| `bklv/views/recycler.hpp` | `bklv/ui/widgets/list/recycler.hpp` | widgets |

新增文件：

- `bklv/ui/widgets/view_types.cpp`（内置视图类型注册入口）

原地修改（路径没变，含依赖改写或依赖消解）：

- `bklv/3rd/tweeny`
- `bklv/CMakeLists.txt`
- `bklv/bklv.hpp`
- `bklv/foundation/core/decode_queue.cpp`
- `bklv/foundation/core/frame_budget.cpp`
- `bklv/foundation/core/platform.cpp`
- `bklv/foundation/core/platform.hpp`
- `bklv/foundation/core/services.hpp`

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
2. `core → platform` / `core → render`：工厂反转（`foundation/core/platform.cpp` 按编译期注册表创建实现），平台/后端实现 `foundation/core/platform.hpp` 声明的接口。
3. 存量 4 条反向依赖（BASELINE 白名单，消解一条要删一行）：

| 方向 | 文件 | 原因 |
|---|---|---|
| theme → view | `bklv/ui/theme/appearance.cpp` | 外观要把 box 段套到视图节点上 |
| theme → view | `bklv/ui/theme/attributes.cpp` | 属性表 handler 直接写 `View` 的 setter |
| media → theme | `bklv/system/media/sound_bank.cpp` | 音效表读取主题里的 `sound.json` 与 `@theme/` 路径 |
| json → theme | `bklv/foundation/json/json_util.hpp` | 变量展开要解析 `@theme/` 引用 |

公共接口与内部实现的边界：`bklv/bklv.hpp` 只 include 各模块的公开头，且不许顺着 include 碰具体后端（`graphics/render/vk/`、`graphics/render/gl/`、`graphics/render/gles3/`、`graphics/render/libnx/`、`graphics/render/glass/{gl,vk}/`）、`system/platform/` 或 `<vulkan/vulkan.h>`；这条由 `check_layers.py` 的伞头可达性检查强制。

平台实现 vs 兼容层：`system/platform/` 只有真正存在的两个平台（SDL3 与 libnx）；后端差异（GL 原型加载、GLES3、Vulkan、libnx 的 ImGui 后端）在 `graphics/render/*/` 里，两者都不进 `foundation/core/`。新代码归档规则：先判断"属于哪个职责"，再落到对应模块；跨层的能力（如玻璃）放使用方所在的层（`graphics/render/glass/`），不留在 `foundation/core/`。

## 5 构建与测试结果

| 配置 | 命令 | 结果 |
|---|---|---|
| MSVC 14.44 · GL | `cmake -S . -B build/gl -G "Visual Studio 17 2022"` + `cmake --build build/gl --config Release` | 0 error / 0 warning；`dist/embed_imgui_gl.exe` 4,645,376 B、`dist/multi_context.exe` 4,605,440 B |
| MSVC 14.44 · Vulkan | `cmake -S . -B build/vk -G "Visual Studio 17 2022" -DBKLV_GRAPHICS=vk` + build | 0 error / 0 warning（96 个编译单元全量重编） |
| MinGW GCC 16.2 · GL | `BKLV_CMAKE_GENERATOR=Ninja BKLV_BUILD_DIR=build/mingw-gl bash scripts/ci_build.sh gl` | 0 error / 0 warning |
| Switch · devkitA64 | `./scripts/build_switch.sh` | 编译 + 链接通过，`dist/switch/bklv_switch.nro` 9.1 MB（romfs 63 个资源文件 / 862,904 B） |
| 本地门禁 | `bash scripts/check.sh` | 命名 200 文件通过 / 分层 4 条白名单无新增 / 主题 2 套 41 份外观一致 / GL 与 VK 零警告 / 全部检查通过 |
| 运行时 | `dist/embed_imgui_gl.exe`、`dist/multi_context.exe` 各跑 8 秒 | 主题 113 色 / 166 数值 / 41 控件外观装载成功、文案 334 条、字体与图标字体加载成功、stderr 0 行 |

未执行项：真机 Switch 运行（本机无设备，只验证编译与打包）。

## 6 质量自检

- [x] 每个文件都有明确模块归属（199 个文件全部归类，0 个 misc/common）
- [x] 目录树反映真实边界（`ui/widgets/` 子目录按用途而非父类拆分层级）
- [x] 依赖方向单向（`check_layers.py` 逐对强制，仅保留 4 条有据白名单）
- [x] 没有为分层造空接口 / 转发类（`ui/layer/` 与 `ui/theme/` 的拆分按真实数据模型切）
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

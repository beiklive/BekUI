# bklv 速查

`bklv/` 是从 [borealis](https://github.com/xfangfang/borealis)（Apache-2.0，wiliwili 使用的框架）
移植过来的视图层。这份文档是写代码时的速查表：文件职责、常用调用、以及踩过的坑。

设计上跟 borealis 保持一致：**没有独立的渲染树，一个 View 持有一个 `YGNode`**，
布局由 Yoga 递归计算，绘制由 `View::draw` 递归完成。

绘制后端从 NanoVG 换成了 ImGui 的 `ImDrawList`，并按需砍掉了这次用不到的
阴影、渐变、焦点高亮、点击动画、XML 布局和平台抽象层。

## 文件职责

下表路径都相对 `bklv/`。

| 文件 | 职责 |
|---|---|
| `core/geometry.hpp` | Point / Size / Rect 与颜色辅助 |
| `core/theme.hpp/.cpp` | 样式数值表与颜色主题 |
| `core/time.hpp/.cpp` | `Animatable` 补间（自带实现，不依赖 tweeny） |
| `core/font.hpp/.cpp` | 字体加载：桌面读系统字体文件；Switch 用 `pl` 服务取内置共享字体并合并 fallback 链；都没有则退回内置字体 |
| `core/platform.hpp/.cpp` | **平台接口**：窗口、事件循环、子系统工厂、系统能力 |
| `platforms/sdl3_platform.*` | macOS/Linux/Windows 的 SDL3 平台实现 |
| `platforms/switch_platform.*` | Switch 的 libnx 平台实现（EGL/GLES3 + pad + 电源/网络/语言） |
| `core/gamepad.hpp/.cpp` | 手柄输入抽象（桌面 SDL3 / Switch libnx） |
| `core/graphics.hpp/.cpp` | **渲染后端接口** + 纹理管理，上层与图形 API 的唯一分界 |
| `backends/opengl_backend.*` | 桌面 OpenGL 后端 |
| `backends/vulkan_backend.*` | Vulkan 后端（动态渲染优先，退到 render pass；交换链重建、纹理上传） |
| `backends/gles3_backend.*` | Switch 的 EGL + OpenGL ES 3 后端 |
| `backends/imgui_impl_libnx.*` | ImGui 的 libnx 平台后端（把按键/触摸喂进 ImGuiIO） |
| `backends/gl_loader.hpp/.cpp` | 跨平台 GL 入口：macOS framework / Linux libGL 直接用原型，Windows 用运行时加载（复用 ImGui 自带的 imgl3w，不引入 glad/gl3w） |
| `backends/gles3_loader.hpp` | Switch 的 GLES3 头 |
| `core/image_decoder.hpp/.cpp` | 图片解码抽象：macOS 走 ImageIO，其余平台走 stb_image |
| `core/resource.hpp/.cpp` | 资源路径解析：桌面用可执行文件目录，Switch 用 `romfs:/` |
| `core/view.hpp/.cpp` | 视图基类：Yoga 样式透传、布局失效、绘制模板、测量回调 |
| `core/box.hpp/.cpp` | 容器：增删子节点时同步 Yoga 树、Flexbox 属性、递归绘制 |
| `core/app.hpp/.cpp` | 帧循环：ImGui 上下文、布局、绘制调度。窗口由 `Platform` 负责 |
| `core/logger.hpp/.cpp` | 日志：级别过滤、时间戳、文件行号、环形缓冲（日志窗口读它） |
| `core/input.hpp/.cpp` | 输入与焦点：hitTest、方向导航、动作注册、手柄 |
| `views/label.hpp/.cpp` | 文本：自动换行、单行省略号、对齐、排版缓存 |
| `views/scrolling_frame.hpp/.cpp` | 纵向滚动：惯性、拖拽、滚动条、焦点自动滚动 |
| `views/h_scrolling_frame.hpp/.cpp` | 横向滚动 |
| `views/recycler.hpp/.cpp` | 虚拟列表：对象池、复用、视口预取与回收 |
| `views/image.hpp/.cpp` | 图片：四种缩放模式、圆角 |
| `views/progress_spinner.hpp/.cpp` | 加载转圈 |
| `views/hints.hpp/.cpp` | 底部提示栏 |
| `views/debug_window.hpp/.cpp` | 调试窗口：ImGui 窗口显示日志流与运行状态 |

## 布局（Box）

以下属性全部直接透传给 Yoga：

```cpp
Box* box = new Box(Axis::ROW);          // ROW 横向 / COLUMN 纵向
box->setPadding(16.0f);                  // 内边距，也支持 4 边分别设置
box->setGap(14.0f);                      // 相邻子节点间距
box->setJustifyContent(JustifyContent::SPACE_BETWEEN);  // 主轴分布
box->setAlignItems(AlignItems::CENTER);  // 交叉轴对齐
Box* child = new Box(Axis::COLUMN);
child->setDimensions(90.0f, 90.0f);      // 固定尺寸
child->setGrow(1.0f);                    // 或按比例分配剩余空间
child->setShrink(0.0f);                  // 固定宽度不被压缩
child->setMargin(8.0f);                  // 外边距
box->addView(child);
```

视图树管理：`addView(view[, position])`、`removeView(view, free)`、`clearViews()`、
`getView(id)`。**增删子节点时 `children` 与 Yoga 节点树必须同步更新**，
否则布局结果会和 C++ 对象树不一致；子节点析构时会自动从父节点摘除。

## 交互与内容控件

```cpp
// 输入与焦点：View 自己注册动作，提示栏会自动显示
cell->registerAction(ControllerButton::A, "播放", [](View* v) { /* ... */ });
bool focusable = cell->isFocusable();          // 想被方向键选中就重写它

// 纵向滚动
ScrollingFrame* frame = new ScrollingFrame();
frame->setHeight(300.0f);
Box* content = new Box(Axis::COLUMN);
content->setHeight(40.0f * 100);               // 内容必须显式给高度
frame->setContentView(content);

// 虚拟列表：1000 条数据只创建约 8 个 cell
RecyclerFrame* list = new RecyclerFrame();
list->setItemHeight(80.0f);
list->setPreFetchLine(1);
list->registerCell("default", [] { return new MyCell(); });
list->setDataSource(&mySource);                // 实现 getItemCount / getItemIdentifier

// 图片
Image* image = new Image();
image->setDimensions(96.0f, 60.0f);
image->setCornerRadius(6.0f);
image->setScalingType(ImageScalingType::FILL); // FIT / FILL / STRETCH / CENTER
// 路径相对"资源目录"：桌面 = 可执行文件目录（构建会复制 resources/ 过去），
// Switch = romfs:/（资源打进 nro）。setImageFromFile 内部会解析，不用自己拼。
image->setImageFromFile("resources/testcard.png");
```

键盘：方向键或 WASD 导航，回车/空格确认，Esc 返回。
鼠标点击默认**只聚焦不执行**动作；想点一下就执行，在控件构造函数里设
`onPointerAction = true`（按钮、tab 都这么做）。列表项保持默认，避免误触发。

## 不生成 imgui.ini

bklv 的绘制全部落在 viewport 的绘制列表上，不依赖 ImGui 窗口的位置和尺寸，
所以默认把 `io.IniFilename` 置空，ImGui 既不会读取也不会写出 `imgui.ini`。

```cpp
// 默认就是关闭的，无需任何调用
bklv::App app;

// 想恢复 ImGui 默认行为（读写 imgui.ini）时：
app.setIniPersistenceEnabled(true);
// 或者用环境变量：BKLV_USE_INI=1 ./dist/demo
```

实现位置：`bklv::App::run()` 里 `ImGui::CreateContext()` 之后、
第一次 `NewFrame()` 之前设置 `ImGui::GetIO().IniFilename = nullptr`。
这个赋值必须在那之前完成，之后改无效。

## 日志与调试窗口

日志用 `BKLV_LOG_INFO(...)` 等宏，输出带 `文件:行号`。日志同时进控制台与
`Logger` 的环形缓冲，界面上的调试窗口（`views/debug_window.hpp`）读缓冲渲染。

**不要为每条日志创建 View** —— borealis 那样做会不断触发 Yoga 重排。
`demo` 界面上右上角的「显示日志」按钮开关日志窗口。

## 已知的坑（都已处理）

1. **字体必须惰性解析。** 视图树是在 `RunDemo` 的参数里构建的，而字体在 `App::run()`
   里才加载；如果 `Label` 在构造时绑定字体，会绑到 `nullptr`，文本永远测不出宽度。
2. **字体加载后必须重排一次。** 树构建时字体还没就绪，文本会测出 0 宽并被 Yoga 缓存。
   `App::frame` 用 `getFontGeneration()` 检测字体变化并触发一次全树重算。
3. **`invalidate()` 不能用 `YGUndefined` 重算。** 在 `update()` 里新建视图（虚拟列表
   每帧都在建 cell）会触发 `invalidate`，如果传 `YGUndefined`，百分比宽高全部失去参照，
   整棵树会塌成内容尺寸。现在用 `App::getLayoutWidth/Height()` 记住的画布尺寸重算。
4. **滚动偏移不能重复叠加。** `getLocalX/Y()` 已含 `translation`，`getX/Y()` 只累加父节点，
   否则滚动距离会被算两次（之前 500 的偏移变成了 1000）。
5. **回收的 cell 可能正是当前焦点。** 被回收时必须清掉"已自动滚动"标记，否则焦点转回来时不会滚动。
6. **图片路径按进程工作目录解析**，且加载失败会被 `g_pathCache` 永久缓存
   （`TEXTURE_CACHE_FAILED`），只会打一条日志。所以要在项目根目录下运行；
   出现"图片位置空着"时先确认工作目录，别急着怀疑渲染后端。

## 分层约定

```
bklv/views/*               Image / Label / ScrollingFrame / RecyclerFrame
                            只认 ImDrawList 和 ImTextureID
        ↓
bklv/core/graphics.hpp     RenderBackend 接口 + 纹理管理
bklv/core/platform.hpp     Platform 接口（窗口 / 事件循环 / 系统能力）
        ↓
bklv/backends/*            OpenGLBackend / VulkanBackend（桌面）/ GLES3Backend（Switch）
bklv/platforms/*           PlatformSDL3（桌面）/ PlatformSwitch（libnx）
```

**后端选择是编译期的**，没有运行时开关——后端代码、链接库、shader 版本都不同。
选择点在 `bklv/platforms/*.cpp` 的 `createBackend()`，是几行 `#if`，
没有注册表、没有优先级列表。加后端的步骤见 [`notes/adding-render-backend.md`](../notes/adding-render-backend.md)。

## 学习提示

- **Yoga**：先配置父节点的 `FlexDirection`、`JustifyContent`、`AlignItems`、`Gap` 和
  `Padding`；再设置子节点尺寸约束或 `FlexGrow`；调用 `YGNodeCalculateLayout()` 后读取
  `YGNodeLayoutGetLeft/Top/Width/Height()`。Yoga 只算几何布局，不负责绘制。
- **Tweeny**：用 `tweeny::from(start).to(end).via(easing).during(frames).build()` 创建动画；
  每帧按经过时间调用 `step(frames)`，通过 `peek()` 读取插值。继续调用 `to()` 可添加关键帧，
  用 `on(tweeny::event::complete, callback)` 监听完成事件。
- **Dear ImGui**：负责接收输入、生成控件和绘制命令；窗口由平台层（SDL3 或 libnx）支持，
  绘制由图形后端（OpenGL / GLES3 / Vulkan）完成。
- **平台层**：上层只跟 `Platform` 与 `RenderBackend` 两个接口打交道。
  加平台写一个 `Platform` 子类，加图形 API 写一个 `RenderBackend` 子类，view 层不用动。
- **Switch 特有**：构建带 `-fno-rtti`，类型判断用 `View::asBox()` 虚函数而不是 `dynamic_cast`；
  Yoga 需要单独用 C++20 编译。

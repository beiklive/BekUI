# ImGui / Yoga / Tweeny 学习项目

面向 macOS 的 C++17 学习项目：用 SDL3 创建窗口，用 Dear ImGui 学习控件和布局，并通过 Yoga 与 Tweeny 示例练习自动布局和动画插值。

## 三方库

| 子模块 | 固定版本 | 用途 | 许可证 |
|---|---|---|---|
| `3rd/imgui` | 上游固定提交 | 即时模式 GUI、控件和绘制 | MIT |
| `3rd/yoga` | `v3.2.1` | Flexbox 自动布局、尺寸约束和间距 | MIT |
| `3rd/tweeny` | `v4.1.0`（上游实际标签） | C++17 头文件动画插值、缓动和关键帧 | MIT |
| SDL3（Homebrew） | 本机安装版本 | 窗口、输入事件和 OpenGL 上下文 | zlib |

Tweeny 仓库当前没有 `tweeny-v4.1.0` 这个标签，实际发布标签为 `v4.1.0`，本项目固定在该标签对应提交 `68a7f44`。Yoga 固定在 `v3.2.1` 对应提交 `042f501`。Git 子模块记录精确提交，因此克隆项目后可复现相同版本。

## 目录

- `3rd/`：第三方库 Git 子模块。
- `demo/demo_01_widgets.cpp`：基础控件。
- `demo/demo_02_layout.cpp`：ImGui 布局 API。
- `demo/demo_03_custom_widget.cpp`：自定义控件绘制。
- `demo/demo_04_yoga.cpp`：用 Yoga 计算 Flexbox 布局，再用 ImGui 绘制结果。
- `demo/demo_05_tweeny.cpp`：Tweeny 缓动、多段关键帧和循环动画。
- `demo/demo_06_bklv_layout.cpp`：用 `src/bklv` 移植层搭布局（Box 布局 + Label 文本）。
- `demo/demo_06_scene.h`：Demo 06 的场景构建，抽出来供无窗口验证复用。
- `demo/demo_07_bklv_showcase.cpp`：虚拟列表 + 滚动 + 图片 + 焦点导航 + 调试层的完整示例。
- `demo/demo_07_scene.h`：Demo 07 的场景构建。
- `resources/testcard.png`：Demo 07 用的测试图片。
- `src/bklv/`：从 borealis 移植的视图层，见下一节。
- `notes/`：架构与移植评估笔记。
- `src/main.cpp`：综合入口。
- `build.sh`：一次性编译所有示例。
- `dist/`：编译产物，不提交到 Git。

## bklv 移植层

`src/bklv` 是从 [borealis](https://github.com/xfangfang/borealis)（Apache-2.0，wiliwili 使用的框架）
移植过来的视图层，核心设计保持一致：**没有独立的渲染树，一个 View 持有一个 YGNode**，
布局由 Yoga 递归计算，绘制由 `View::draw` 递归完成。

绘制后端从 NanoVG 换成了 ImGui 的 `ImDrawList`，并按需砍掉了这次用不到的
阴影、渐变、焦点高亮、点击动画、XML 布局和平台抽象层。

| 文件 | 职责 |
|---|---|
| `core/geometry.hpp` | Point / Size / Rect 与颜色辅助 |
| `core/theme.hpp/.cpp` | 样式数值表与颜色主题 |
| `core/time.hpp/.cpp` | `Animatable` 补间（自带实现，不依赖 tweeny） |
| `core/font.hpp/.cpp` | 字体加载：优先系统中文字体，否则退回内置字体 |
| `core/graphics.hpp/.cpp` | **渲染后端接口** + 纹理管理，上层与图形 API 的唯一分界 |
| `backends/opengl_backend.*` | OpenGL 后端（当前唯一图形后端） |
| `backends/null_backend.*` | 空后端：开窗不渲染，用于无图形环境自检 |
| `core/view.hpp/.cpp` | 视图基类：Yoga 样式透传、布局失效、绘制模板、测量回调 |
| `core/box.hpp/.cpp` | 容器：增删子节点时同步 Yoga 树、Flexbox 属性、递归绘制 |
| `core/app.hpp/.cpp` | SDL3 窗口 + 帧循环，整棵树画在 `GetForegroundDrawList()` 上 |
| `core/input.hpp/.cpp` | 输入与焦点：hitTest、方向导航、动作注册、手柄 |
| `core/texture.hpp/.cpp` | 图片解码（macOS ImageIO）+ OpenGL 纹理上传 |
| `views/label.hpp/.cpp` | 文本：自动换行、单行省略号、对齐、排版缓存 |
| `views/scrolling_frame.hpp/.cpp` | 纵向滚动：惯性、拖拽、滚动条、焦点自动滚动 |
| `views/h_scrolling_frame.hpp/.cpp` | 横向滚动 |
| `views/recycler.hpp/.cpp` | 虚拟列表：对象池、复用、视口预取与回收 |
| `views/image.hpp/.cpp` | 图片：四种缩放模式、圆角 |
| `views/progress_spinner.hpp/.cpp` | 加载转圈 |
| `views/hints.hpp/.cpp` | 底部提示栏 |
| `views/debug_layer.hpp/.cpp` | 调试层：布局树 + 当前焦点（F1 开关） |

Box 提供的布局能力（全部直接透传给 Yoga）：

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

### 交互与内容控件

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
image->setImageFromFile("resources/testcard.png");
```

键盘：方向键或 WASD 导航，回车/空格确认，Esc 返回，F1 开关调试层。

### 已知的坑（都已处理）

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

## 初始化与编译

克隆已有项目时初始化全部子模块：

```bash
git submodule update --init --recursive
```

安装 SDL3（只需安装一次），然后编译：

```bash
brew install sdl3
./build.sh
```

脚本会把 Yoga 核心源码（按 Yoga 3.2.1 要求使用 C++20）构建成 `dist/libyogacore.a`，
再把 bklv 移植层编译成 `dist/bklv-objects/**.o`，最后链接出七个 demo 和 `src/main.cpp`。
Tweeny 是头文件库，示例按 C++17 编译，无需单独编译或链接。

## 运行

```bash
./dist/demo_04_yoga
./dist/demo_05_tweeny
./dist/demo_06_bklv_layout
```

每个程序会打开独立窗口，关闭窗口即可退出。

## 学习提示

- Yoga：先配置父节点的 `FlexDirection`、`JustifyContent`、`AlignItems`、`Gap` 和 `Padding`；再设置子节点尺寸约束或 `FlexGrow`；调用 `YGNodeCalculateLayout()` 后读取 `YGNodeLayoutGetLeft/Top/Width/Height()`。Yoga 只算几何布局，不负责绘制。
- Tweeny：用 `tweeny::from(start).to(end).via(easing).during(frames).build()` 创建动画；每帧按经过时间调用 `step(frames)`，通过 `peek()` 读取插值，并把数值用于 ImGui 绘制。继续调用 `to()` 可添加关键帧，使用 `on(tweeny::event::complete, callback)` 监听完成事件。
- Dear ImGui：负责接收输入、生成控件和绘制命令；本项目窗口由 SDL3 后端支持，绘制由 OpenGL3 后端完成。

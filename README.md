# ImGui / Yoga / Tweeny 学习项目

面向 macOS 的 C++17 学习项目：用 SDL3 创建窗口，用 Dear ImGui 学习控件和布局，并通过 Yoga 与 Tweeny 示例练习自动布局和动画插值。

## 三方库

| 子模块 | 固定版本 | 用途 | 许可证 |
|---|---|---|---|
| `3rd/imgui` | 上游固定提交 | 即时模式 GUI、控件和绘制 | MIT |
| `3rd/yoga` | `v3.2.1` | Flexbox 自动布局、尺寸约束和间距 | MIT |
| `3rd/tweeny` | `v4.1.0`（上游实际标签） | C++17 头文件动画插值、缓动和关键帧 | MIT |
| `3rd/SDL` | `release-3.4.16` | 窗口、输入事件、手柄和 OpenGL 上下文（源码编译成静态库） | zlib |
| `3rd/stb` | 上游 master | `stb_image.h`：Linux / Switch 的图片解码 | public domain / MIT |

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
- `src/main.cpp`：**能力总览**。Tab 页面展示已实现的能力（控件 / 文本 / 布局 / 列表 / 调试）。
- `CMakeLists.txt`：构建入口，一次编出全部示例（两个后端各一个构建目录）。
- `build.bat`：Windows 上的便利入口，找工具链后驱动同一个 `CMakeLists.txt`。
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
| `core/platform.hpp/.cpp` | **平台接口**：窗口、事件循环、子系统工厂、系统能力 |
| `platforms/sdl3_platform.*` | macOS/Linux/Windows 的 SDL3 平台实现 |
| `platforms/switch_platform.*` | Switch 的 libnx 平台实现（EGL/GLES3 + pad + 电源/网络/语言） |
| `core/gamepad.hpp/.cpp` | 手柄输入抽象（桌面 SDL3 / Switch libnx） |
| `core/graphics.hpp/.cpp` | **渲染后端接口** + 纹理管理，上层与图形 API 的唯一分界 |
| `backends/opengl_backend.*` | 桌面 OpenGL 后端 |
| `backends/vulkan_backend.*` | Vulkan 后端（动态渲染优先，退到 render pass；交换链重建、纹理上传） |
| `backends/gles3_backend.*` | Switch 的 EGL + OpenGL ES 3 后端 |
| `backends/imgui_impl_libnx.*` | ImGui 的 libnx 平台后端（把按键/触摸喂进 ImGuiIO） |
| `backends/null_backend.*` | 空后端：开窗不渲染，用于无图形环境自检 |
| `backends/gl_loader.hpp/.cpp` | 跨平台 GL 入口：macOS framework / Linux libGL 直接用原型，Windows 用运行时加载（复用 ImGui 自带的 imgl3w，不引入 glad/gl3w） |
| `backends/gles3_loader.hpp` | Switch 的 GLES3 头 |
| `core/image_decoder.hpp/.cpp` | 图片解码抽象：macOS 走 ImageIO，其余平台走 stb_image |
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
| `views/debug_window.hpp/.cpp` | 调试窗口：ImGui 窗口显示日志流与运行状态 |

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

键盘：方向键或 WASD 导航，回车/空格确认，Esc 返回。

### 不生成 imgui.ini

bklv 的绘制全部落在 viewport 的绘制列表上，不依赖 ImGui 窗口的位置和尺寸，
所以默认把 `io.IniFilename` 置空，ImGui 既不会读取也不会写出 `imgui.ini`。

```cpp
// 默认就是关闭的，无需任何调用
bklv::App app;

// 想恢复 ImGui 默认行为（读写 imgui.ini）时：
app.setIniPersistenceEnabled(true);
// 或者用环境变量：BKLV_USE_INI=1 ./dist/demo_07_bklv_showcase
```

实现位置：`bklv::App::run()` 里 `ImGui::CreateContext()` 之后、
第一次 `NewFrame()` 之前设置 `ImGui::GetIO().IniFilename = nullptr`。
这个赋值必须在那之前完成，之后改无效。

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

## 平台支持

| 平台 | 实现方式 | 图片解码 | 状态 |
|---|---|---|---|
| macOS | SDL3（`platforms/sdl3_platform.*`） | ImageIO（系统） | 已跑通 |
| Linux | 同上 | stb_image | 已在 Ubuntu 24.04 容器验证构建 |
| Windows | 同上（MSYS2 / MinGW-w64） | stb_image | 已构建并跑通（GL / Vulkan / 空后端三种都验证过） |
| Nintendo Switch | libnx + EGL/GLES3（`platforms/switch_platform.*`） | stb_image | 编译产出 .nro 通过；画面待真机确认 |
| Android / iOS | 待定（SDL3 支持，需各自工程骨架） | stb_image | 未开始 |

Switch 上为什么不用 SDL3：SDL3 上游没有 Switch 后端（`src/video/` 下没有对应实现，
CMake 里也没有分支），devkitPro 的 portlibs 里只有 SDL2。所以 Switch 直接走原生：
窗口用 libnx 的 `NWindow`，图形用 EGL + OpenGL ES 3，输入用 `padUpdate` + 触摸屏；
ImGui 的平台后端也得自己写（`backends/imgui_impl_libnx.*`，ImGui 只有 SDL 的）。

### Switch 构建与运行

```bash
./scripts/build_switch.sh            # 默认 demo_07，产出 dist/switch/demo_07.nro
./scripts/build_switch.sh demo_06    # 指定 demo
./scripts/build_switch.sh demo_07 --clean
nxlink -s dist/switch/demo_07.nro    # 推到机器（需机器上开着 hbmenu 的网络加载）
```

## 渲染后端

绘制代码分三层，上层完全不接触图形 API：

```
src/bklv/views/*            Image / Label / ScrollingFrame / RecyclerFrame
                            只认 ImDrawList 和 ImTextureID
        ↓
src/bklv/core/graphics.hpp  RenderBackend 接口 + 纹理管理
src/bklv/core/platform.hpp  Platform 接口（窗口 / 事件循环 / 系统能力）
        ↓
src/bklv/backends/*         OpenGLBackend（桌面）/ GLES3Backend（Switch）/ NullBackend
src/bklv/platforms/*        PlatformSDL3（桌面）/ PlatformSwitch（libnx）
```

**每个 demo 都编 GL 和 Vulkan 两个版本**（约定见 `AGENTS.md`）。
后端是编译期确定的，没有运行时开关——后端代码、链接库、shader 版本都不同，
运行时切换在一个构建里没有意义。

构建入口只有一个：根目录的 `CMakeLists.txt`。后端是编译期决定的，所以
「每个 demo 都能编 GL 和 Vulkan 两版」靠**两个构建目录**实现：

```bash
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl   # GL，产物在 dist/
cmake -S . -B build/vk -DBKLV_GRAPHICS=vk && cmake --build build/vk   # Vulkan，产物在 dist/vk/
cmake -S . -B build/null -DBKLV_NULL_BACKEND=ON && cmake --build build/null  # 空后端：开窗但不绘制
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl -DBKLV_TARGETS=main && cmake --build build/gl  # 只编主程序
```

`-DBKLV_TARGETS=<子串>` 按目标名做子串匹配，只编匹配到的目标。改主程序时用它
可以省掉 7 个 demo 的编译与链接；没匹配到任何目标会在配置阶段报错并列出可用目标。
不过 CMake 有增量编译，改一个文件通常只重编那一两个目标，所以这个开关主要是
首次配置时省时间。

Windows 上等价的是 `build.bat`（见下文「Windows（MSYS2 / MinGW-w64）」），
它只负责找 MSYS2 与 MinGW 工具链、把工具链放进 `PATH`，构建逻辑仍然只有
`CMakeLists.txt` 一份：

```bat
build.bat                 :: GL
build.bat --vk            :: Vulkan
build.bat --null          :: 空后端
build.bat --only=main     :: 只编主程序
```

| 构建 | 图形后端 | 输出目录 | 状态 |
|---|---|---|---|
| 默认（`BKLV_GRAPHICS=gl`） | `backends/opengl_backend`（OpenGL 3.2 Core） | `dist/` | 可用 |
| `BKLV_GRAPHICS=vk` | `backends/vulkan_backend`（Vulkan） | `dist/vk/` | 可用（Windows 已跑通并过校验层） |
| Switch | `backends/gles3_backend`（EGL + OpenGL ES 3） | `dist/switch/` | 编译通过 |
| `BKLV_NULL_BACKEND=ON` | `backends/null_backend` | `dist/` | 可用 |

Vulkan 构建需要 SDK：macOS 用 `brew install vulkan-headers vulkan-loader molten-vk`，
Linux 用 `apt install libvulkan-dev`，Windows 用
`pacman -S mingw-w64-x86_64-vulkan-headers mingw-w64-x86_64-vulkan-loader`
（或装 LunarG SDK 并设 `VULKAN_SDK`），或用 `VULKAN_SDK` 指定。缺 SDK 时构建会明确报错。

选择逻辑就在各平台的 `createBackend()` 里（`platforms/sdl3_platform.cpp` /
`platforms/switch_platform.cpp`），是几行 `#if`，没有注册表、没有优先级列表。
加后端的步骤见 `notes/adding-render-backend.md`。

## 初始化与编译

克隆已有项目时初始化全部子模块：

```bash
git submodule update --init --recursive
```

只需要 `cmake`（SDL3 作为子目录一起编），**不需要 `brew install sdl3`**：

```bash
brew install cmake
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl
```

Switch 另需 devkitPro（devkitA64 + libnx），用 `scripts/build_switch.sh` 单独构建
（工具链和编译参数与桌面完全不同，且不能链接 SDL3，所以它走独立脚本、不并入 CMake）。

### Linux

`CMakeLists.txt` 会按平台分支：编译器用 `g++`，图片解码切到 stb_image，
链接参数由 SDL3 这个 CMake 子项目自己带出来。

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
    libgl1-mesa-dev libglx-dev libx11-dev libxext-dev \
    libwayland-dev libxkbcommon-dev
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl
```

SDL3 在 Linux 的可选依赖很多（X11 的 xcursor/xi/xfixes/xrandr、Wayland、各类音频后端），
`CMakeLists.txt` 用 `pkg_check_modules` 自动检测：有对应开发包就编，没有就关掉，
因此不需要把它们装全。

> **注意：`dist/` 是平台相关的，不要跨平台共用同一个工作区。**
> 在容器里验证 Linux 时请在容器内构建，别把容器的 `dist/` 留在宿主的工作区，
> 否则回到 macOS 会报 `archive member not a mach-o file`。
> 顺带一提，`build/` 也一样：GL 与 Vulkan 各用一个构建目录（`build/gl`、`build/vk`），
> 不要共用一个目录反复改 `BKLV_GRAPHICS`。

### Windows（MSYS2 / MinGW-w64）

Windows 上用根目录的 `build.bat`：它只负责找到 MSYS2 与 MinGW 工具链、把工具链
放进 `PATH`，然后把参数交给 CMake。构建逻辑仍然只有 `CMakeLists.txt` 一份。

**一次性准备**（装 MSYS2 后在其终端里执行）：

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
# 只有要编 Vulkan 时才需要这两个
pacman -S mingw-w64-x86_64-vulkan-headers mingw-w64-x86_64-vulkan-loader
```

**构建与运行**：

```bat
build.bat                 :: GL，产物 dist\*.exe
build.bat --vk            :: Vulkan，产物 dist\vk\*.exe
build.bat --null          :: 空后端，开窗但不绘制
build.bat --gl --gui      :: 不弹控制台窗口（默认保留控制台，方便看日志）
build.bat --sdl-force     :: 删掉构建目录重新配置
build.bat --only=main     :: 只编主程序，跳过 7 个 demo
```

```bat
dist\demo_07_bklv_showcase.exe
dist\imgui_main.exe
```

几个 Windows 专属的点：

- **在 MSYS2 里要用「MSYS2 MINGW64」终端。** 默认的 MSYS 终端 `MINGW_PREFIX`
  是空的，CMake 找不到编译器；`CMakeLists.txt` 只在 `MINGW_PREFIX` 存在时兜底，
  找不到就打印一句提示，不会去猜路径。用 `build.bat` 则不受影响，它会自己把
  工具链放进 `PATH`。
- **OpenGL 是运行时加载的。** `opengl32.dll` 只导出 GL 1.1，GL 3.2 Core 的函数
  （`glCreateShader`、`glGenVertexArrays`、`glBindSampler`…）必须取地址。
  `src/bklv/backends/gl_loader.cpp` 复用 ImGui 自带的 imgl3w 完成这件事，
  不引入 glad/gl3w 之类的额外依赖，也不加生成步骤。
- **中文字体走系统字体。** `core/font.cpp` 与 `demo/demo_common.h` 按平台挑字体
  （Windows 优先微软雅黑，其次黑体、宋体；macOS 优先冬青黑体、苹方）。
  一个都找不到才退回 ImGui 内置字体，那时中文会显示成方块。
- **Vulkan 已经能出画面。** Vulkan 后端走动态渲染（`VK_KHR_dynamic_rendering` /
  Vulkan 1.3 core），设备不支持时退到传统 render pass + framebuffer。
  Windows 上跑 `dist\vk\*.exe` 即可；想同时开校验层，设环境变量
  `BKLV_VULKAN_VALIDATION=1`，并把 `VK_LAYER_PATH` 指向校验层所在目录
  （MSYS2 装的包放在 `mingw64\bin`，不在 `share\vulkan\explicit_layer.d`）。
- **Vulkan 的颜色格式选 UNORM，不选 sRGB。** ImGui 给的像素分量就是要显示的
  8 位值，交换链若用 `_SRGB` 格式，硬件会再编码一次，整屏被提亮
  （64 → 137、128 → 188），看起来发灰。选 UNORM 后两个后端的颜色完全一致，
  可以拿纯色块实测比对：`dist/` 与 `dist/vk/` 同一像素点的 RGB 应完全相同。
- **MSVC 未验证。** 这套构建走的是 MinGW-w64。用 MSVC 需要另加一套配置，目前没做。

构建时 CMake 依次做这些事：

1. `add_subdirectory(3rd/SDL)` 把 SDL3 编成静态库（SDL3 自己的 CMake 处理平台后端探测）
2. Yoga 核心源码（按 Yoga 要求使用 C++20）编成 `yogacore` 静态库
3. bklv 移植层与 ImGui 编成 `bklv_core` / `bklv_app` / `imgui_impl` 目标库
4. 链接出七个 demo 和 `src/main.cpp`

Tweeny 是头文件库，示例按 C++17 编译，无需单独编译或链接。
SDL3 是静态链接的，Windows 上只依赖系统 DLL，不依赖 `SDL3.dll`。

## 运行

```bash
./dist/demo_04_yoga
./dist/demo_05_tweeny
./dist/demo_06_bklv_layout
./dist/demo_07_bklv_showcase          # 加 -v 启动即开调试层
```

Windows 上是同名文件加 `.exe`：

```bat
dist\demo_04_yoga.exe
dist\demo_07_bklv_showcase.exe -v
```

每个程序会打开独立窗口，关闭窗口即可退出。
`resources/testcard.png` 是相对工作目录读的，请在项目根目录下运行。

## 学习提示

- Yoga：先配置父节点的 `FlexDirection`、`JustifyContent`、`AlignItems`、`Gap` 和 `Padding`；再设置子节点尺寸约束或 `FlexGrow`；调用 `YGNodeCalculateLayout()` 后读取 `YGNodeLayoutGetLeft/Top/Width/Height()`。Yoga 只算几何布局，不负责绘制。
- Tweeny：用 `tweeny::from(start).to(end).via(easing).during(frames).build()` 创建动画；每帧按经过时间调用 `step(frames)`，通过 `peek()` 读取插值，并把数值用于 ImGui 绘制。继续调用 `to()` 可添加关键帧，使用 `on(tweeny::event::complete, callback)` 监听完成事件。
- Dear ImGui：负责接收输入、生成控件和绘制命令；窗口由平台层（SDL3 或 libnx）支持，绘制由图形后端（OpenGL / GLES3）完成。
- 平台层：上层只跟 `Platform` 与 `RenderBackend` 两个接口打交道。加平台写一个 `Platform` 子类，加图形 API 写一个 `RenderBackend` 子类，view 层不用动。
- Switch 特有：`-fno-rtti` 所以类型判断用 `View::asBox()` 虚函数而不是 `dynamic_cast`；Yoga 需要单独用 C++20 编译。

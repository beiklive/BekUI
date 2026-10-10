# AGENTS.md — 本项目的开发约定

## 图形后端：demo 一律双版本

**每个 demo 都要能编出 GL 和 Vulkan 两个版本**，不是二选一。

原因：GL 和 Vulkan 是两条完全不同的渲染路径（上下文创建、shader、资源生命周期都不同），
只在一个后端上验证过的界面代码不算验证过。双版本让「上层与图形 API 无关」这条设计
随时可被检验——如果某个 demo 换后端跑不起来，说明它漏了抽象。

### 构建

```bash
./build.sh                      # 默认 GL
./build.sh --vk                 # Vulkan
BKLV_GRAPHICS=vk ./build.sh     # 等价写法
```

产物分目录，互不覆盖：

| 构建 | 输出目录 |
|---|---|
| GL | `dist/` |
| Vulkan | `dist/vk/` |
| Switch | `dist/switch/` |

因为分目录，可执行文件名不带后缀。

### 加新 demo 时

`build.sh` 的 `TARGETS` 里加一行即可，两个后端会自动各编一份；
不要写任何依赖具体图形 API 的代码。需要图形 API 能力时，走 `RenderBackend` 接口。

```bash
"demo_08_xxx:${ROOT_DIR}/demo/demo_08_xxx.cpp:1"
#                                             ↑ 末位 1 表示需要链接 bklv
```

### 后端选择是编译期的事

见 `notes/adding-render-backend.md`。选择点在 `src/bklv/platforms/*.cpp` 的
`createBackend()`，不在 App 里做运行时切换。

---

## 平台

| 平台 | 构建 | 状态 |
|---|---|---|
| macOS | `./build.sh` | 已完成 |
| Linux | `./build.sh`（自动识别，用 g++ + pkg-config） | 已完成 |
| Switch | `./scripts/build_switch.sh` | 编译通过，画面待真机验证 |
| Windows | 未做 | 待办 |
| Android / iOS | 未做 | 待办 |

`Switch` 走独立脚本：工具链（devkitA64）与编译参数和桌面完全不同，且不能链接 SDL3。

## 依赖

第三方都走 git submodule，构建脚本自己编译，不依赖系统安装：

| 子模块 | 用途 |
|---|---|
| `3rd/imgui` | GUI 与绘制 |
| `3rd/yoga` | Flexbox 布局 |
| `3rd/tweeny` | 补间动画（仅老 demo 直接用） |
| `3rd/SDL` | 桌面的窗口/输入/GL 上下文（源码编译成静态库） |
| `3rd/stb` | Linux/Switch 的图片解码 |

系统只要求：`cmake`（编 SDL3）、以及平台本身的工具链。
**不要引入需要手动安装的开发库**，除非在 README 里写清楚安装命令。

## 代码

- C++17；`3rd/yoga` 单独用 C++20 编译
- `-Wall -Wextra -Wpedantic` 必须零警告
- Switch 构建带 `-fno-rtti -fno-exceptions`，所以**不要用 `dynamic_cast` / RTTI**，
  类型判断用虚函数（见 `View::asBox()`）
- 不生成 `imgui.ini`：绘制全在 viewport 绘制列表上，`io.IniFilename` 置空
- 日志用 `BKLV_LOG_INFO(...)` 等宏（带 文件:行号）。日志同时进控制台与
  `Logger` 的环形缓冲，界面上的日志窗口（`views/debug_window.hpp`）读缓冲渲染。
  **不要**为每条日志创建 View —— borealis 那样做会不断触发 Yoga 重排
- ImGui 窗口在 `App::onDrawWindows` 里下发（视图树画完之后、`ImGui::Render()` 之前）
- 鼠标点击默认**只聚焦不执行**动作。想点一下就执行，在控件构造函数里设
  `onPointerAction = true`（按钮、tab 都这么做）；列表项保持默认，避免误触发

## 目录

```
src/bklv/core       接口层：view / box / app / platform / graphics / input / ...
src/bklv/views      控件：label / image / scrolling_frame / recycler / ...
src/bklv/backends   图形 API 实现：opengl / gles3 / null / imgui 后端
src/bklv/platforms  平台实现：sdl3 / switch
demo/               示例
scripts/            构建脚本
notes/              设计笔记与评估
```

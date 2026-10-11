# ImGui / Yoga / Tweeny 学习项目

用 SDL3 创建窗口、用 Dear ImGui 绘制界面的 C++17 学习项目：一边练 ImGui 的控件与布局，
一边用 Yoga 和 Tweeny 练自动布局与动画插值，逐步长出一层自己写的视图层 `bklv`。

也是 [wiliwili](https://github.com/xfangfang/wiliwili) 那套界面想在桌面上复用的一个试验：
`src/bklv` 是从 [borealis](https://github.com/xfangfang/borealis) 按需移植的视图层，
绘制后端从 NanoVG 换成 ImGui 的 `ImDrawList`。

## 这个项目里有什么

- **七个示例 + 一个总览程序**，从最基础的控件到完整的虚拟列表：

  | 程序 | 内容 |
  |---|---|
  | `demo_01_widgets` | 基础控件 |
  | `demo_02_layout` | ImGui 布局 API |
  | `demo_03_custom_widget` | 自定义控件绘制 |
  | `demo_04_yoga` | 用 Yoga 算 Flexbox 布局，再用 ImGui 画出来 |
  | `demo_05_tweeny` | Tweeny 缓动、多段关键帧、循环动画 |
  | `demo_06_bklv_layout` | 用 `src/bklv` 移植层搭布局 |
  | `demo_07_bklv_showcase` | 虚拟列表 + 滚动 + 图片 + 焦点导航 + 调试层 |
  | `imgui_main` | `src/main.cpp`，按 Tab 展示已实现的能力 |

- **一层与图形 API 无关的视图层**：`View` 持有一个 `YGNode`，布局交给 Yoga 递归算，
  绘制由 `View::draw` 递归完成。上层只认 `ImDrawList` 和 `ImTextureID`，
  换后端（OpenGL / Vulkan / GLES3 / 空后端）不用动 view 层。
- **每个 demo 都编 GL 和 Vulkan 两个版本**。两条渲染路径的后端代码、链接库、
  shader 版本都不同，只在一个后端上验证过的界面代码不算验证过。
  后端是编译期决定的，所以靠两个构建目录各编一份，没有运行时开关。
- **桌面三平台 + Switch**：macOS / Linux / Windows 走 SDL3，Switch 走 libnx + EGL/GLES3。

## 平台支持

| 平台 | 实现方式 | 图片解码 | 状态 |
|---|---|---|---|
| macOS | SDL3（`platforms/sdl3_platform.*`） | ImageIO（系统） | 已跑通 |
| Linux | 同上 | stb_image | 已在 Ubuntu 24.04 容器验证构建 |
| Windows | 同上（MSYS2 / MinGW-w64） | stb_image | 已构建并跑通（GL / Vulkan / 空后端都验证过） |
| Nintendo Switch | libnx + EGL/GLES3（`platforms/switch_platform.*`） | stb_image | 编译产出 .nro 通过；画面待真机确认 |
| Android / iOS | 待定（SDL3 支持，需各自工程骨架） | stb_image | 未开始 |

Switch 上不用 SDL3：SDL3 上游没有 Switch 后端，devkitPro 的 portlibs 里只有 SDL2。
所以 Switch 走原生——窗口用 libnx 的 `NWindow`，图形用 EGL + OpenGL ES 3，
输入用 `padUpdate` + 触摸屏，ImGui 的平台后端也得自己写。

## 拉取与编译

```bash
git clone --recursive https://github.com/beiklive/BekUI.git
cd BekUI
```

已经 clone 过、子模块还是空的就补一下：

```bash
git submodule update --init --recursive
```

只需要 `cmake`，SDL3 作为子目录一起编，**不需要 `brew install sdl3`**：

```bash
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl
```

产物在 `dist/`。想编 Vulkan 版就换一个构建目录，两版可以并存：

```bash
cmake -S . -B build/vk -DBKLV_GRAPHICS=vk && cmake --build build/vk   # 产物在 dist/vk/
```

不需要 Vulkan 就别碰 `build/vk`：只有它需要额外的 Vulkan SDK（见下）。
还有两个可选开关：

```bash
# 空后端：开窗但不绘制，用于没有图形环境时自检（产物在 dist/null/）
cmake -S . -B build/null -DBKLV_NULL_BACKEND=ON && cmake --build build/null

# 只编主程序，跳过 7 个 demo（CMake 有增量编译，改一个文件本来就只重编一两个目标，
# 这个开关主要是首次配置时省时间）
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl -DBKLV_TARGETS=main && cmake --build build/gl
```

### 运行

```bash
./dist/demo_07_bklv_showcase      # 加 -v 启动即开调试层
./dist/imgui_main
```

`resources/testcard.png` 是相对**工作目录**读的，所以请在项目根目录下运行。

### Linux

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
    libgl1-mesa-dev libglx-dev libx11-dev libxext-dev \
    libwayland-dev libxkbcommon-dev
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl
```

SDL3 在 Linux 的可选依赖很多（X11 的 xcursor/xi/xfixes/xrandr、Wayland、各类音频后端），
`CMakeLists.txt` 用 `pkg_check_modules` 自动检测：有对应开发包就编，没有就关掉，
不需要把它们装全。

### Windows（MSYS2 / MinGW-w64）

用根目录的 `build.bat`，它只负责找到 MSYS2 与 MinGW 工具链、把工具链放进 `PATH`，
构建逻辑仍然只有 `CMakeLists.txt` 一份。

**一次性准备**（装好 MSYS2 后在其终端里执行）：

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
# 只有要编 Vulkan 时才需要这两个
pacman -S mingw-w64-x86_64-vulkan-headers mingw-w64-x86_64-vulkan-loader
```

```bat
build.bat                 :: GL，产物 dist\*.exe
build.bat --vk            :: Vulkan，产物 dist\vk\*.exe
build.bat --null          :: 空后端，开窗但不绘制
build.bat --only=main     :: 只编主程序，跳过 7 个 demo
build.bat --gl --gui      :: 不弹控制台窗口（默认保留控制台，方便看日志）
build.bat --sdl-force     :: 删掉构建目录重新配置
```

```bat
dist\demo_07_bklv_showcase.exe
dist\imgui_main.exe
```

几点 Windows 专属的注意：

- **直接用 cmake 的话，要在「MSYS2 MINGW64」终端里。** 默认的 MSYS 终端
  `MINGW_PREFIX` 是空的，CMake 找不到编译器；`CMakeLists.txt` 只在 `MINGW_PREFIX`
  存在时兜底，找不到就打印一句提示，不会去猜路径。用 `build.bat` 不受影响。
- **OpenGL 是运行时加载的。** `opengl32.dll` 只导出 GL 1.1，GL 3.2 Core 的函数必须取地址，
  `backends/gl_loader.cpp` 复用 ImGui 自带的 imgl3w 完成，不引入 glad/gl3w 之类的依赖。
- **中文字体走系统字体**（优先微软雅黑，其次黑体、宋体），一个都找不到才退回 ImGui 内置字体，
  那时中文会显示成方块。
- **Vulkan 的颜色格式选 UNORM，不选 sRGB**，否则整屏会发灰。细节见 `docs/bklv-api.md`
  与 `AGENTS.md`。
- **MSVC 未验证**，这套构建走的是 MinGW-w64。

### Switch（devkitPro）

Switch 不能并进 CMake：工具链（devkitA64）与编译参数和桌面完全不同，也链接不了 SDL3，
所以它走独立脚本。

```bash
./scripts/build_switch.sh            # 默认 demo_07，产出 dist/switch/demo_07.nro
./scripts/build_switch.sh demo_06    # 指定 demo
./scripts/build_switch.sh demo_07 --clean
nxlink -s dist/switch/demo_07.nro    # 推到机器（需机器上开着 hbmenu 的网络加载）
```

> **`dist/` 与 `build/` 都是平台相关的，不要跨平台共用同一个工作区。**
> 在容器里验证 Linux 时请在容器内构建，别把容器的 `dist/` 留在宿主的工作区，
> 否则回到 macOS 会报 `archive member not a mach-o file`。
> GL 与 Vulkan 也各用一个构建目录，不要共用一个目录反复改 `BKLV_GRAPHICS`。

### Vulkan 构建需要 SDK

| 平台 | 安装 |
|---|---|
| macOS | `brew install vulkan-headers vulkan-loader molten-vk` |
| Linux | `apt install libvulkan-dev` |
| Windows | `pacman -S mingw-w64-x86_64-vulkan-headers mingw-w64-x86_64-vulkan-loader`（或装 LunarG SDK 并设 `VULKAN_SDK`） |

缺 SDK 时构建会明确报错。想开校验层调试，设 `BKLV_VULKAN_VALIDATION=1`；
MSYS2 装的层放在 `mingw64\bin`，还要同时设 `VK_LAYER_PATH`。

## 用到自己的项目里

构建是「源码级依赖」：第三方一律走 git submodule，由构建自己编译，不依赖系统安装的库。

### 只要 ImGui 那部分

`demo/demo_01_widgets.cpp` ~ `demo_03_custom_widget.cpp` 只依赖 `demo/demo_common.h`
和 `3rd/imgui`，不涉及 `src/bklv`。把它们连同 ImGui 源码加进自己的 CMake 目标即可：

```cmake
add_executable(my_app
    my_app.cpp
    3rd/imgui/imgui.cpp
    3rd/imgui/imgui_draw.cpp
    3rd/imgui/imgui_tables.cpp
    3rd/imgui/imgui_widgets.cpp
    3rd/imgui/backends/imgui_impl_sdl3.cpp
    src/bklv/backends/imgui_impl_opengl3_wrapper.cpp   # 见下面的说明
)
target_include_directories(my_app PRIVATE 3rd/imgui src)
target_compile_definitions(my_app PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
target_link_libraries(my_app PRIVATE SDL3::SDL3-static)
```

### 带上 bklv 视图层

`src/bklv` 分三层，**view 层不接触图形 API**：

```
src/bklv/views/*            Image / Label / ScrollingFrame / RecyclerFrame
                            只认 ImDrawList 和 ImTextureID
        ↓
src/bklv/core/graphics.hpp  RenderBackend 接口 + 纹理管理
src/bklv/core/platform.hpp  Platform 接口（窗口 / 事件循环 / 系统能力）
        ↓
src/bklv/backends/*         OpenGLBackend / VulkanBackend / GLES3Backend / NullBackend
src/bklv/platforms/*        PlatformSDL3 / PlatformSwitch
```

所以「换后端」和「换平台」都是加一个子类、在 `createBackend()` 里多一个 `#if`，
view 层和业务代码不用动。想按自己的需求裁剪，照这几条来：

- **只要视图层、不要 demo**：把 `CMakeLists.txt` 里 `bklv_core` 那一串源文件
  和 `imgui_impl` 拿来当起点，`bklv_app`（`src/app/`）是总览程序专用的，可以整层不要。
- **换后端**：实现 `RenderBackend` 的那十个方法，在 `platforms/*.cpp` 的
  `createBackend()` 里加一个分支。步骤和两个后端都踩过的坑见
  [`notes/adding-render-backend.md`](notes/adding-render-backend.md)。
- **换平台**：实现 `Platform` 子类。Switch 那份（`platforms/switch_platform.*`）
  是最小参考实现——没有 SDL3，窗口、事件循环、电源/网络/语言能力都自己接。
- **别直接改 `3rd/` 下的子模块**，它们是指定提交的第三方代码，改动会在更新子模块时丢失。
  需要绕过的都在项目自己的头文件里处理（例如 Windows 的 GL 加载、
  `gl_loader.hpp` 里 `#undef ERROR` 那个 `wingdi.h` 宏冲突）。

三个容易踩的实现细节，`CMakeLists.txt` 里都有注释：

- **Yoga 不要 `add_subdirectory`**：它自己的 `cmake/project-defaults.cmake` 会
  `add_compile_options(-Wall -Werror -fno-rtti)` 并设 `CMAKE_CXX_STANDARD 20`，
  作用域是目录级，会污染整个工程，`-Werror` 直接破坏零警告约定。
  正确做法是把它编成自己的 `yogacore` 目标、单独用 C++20 编译。
- **ImGui 编成 OBJECT 库而不是 STATIC 库**：原来是内联进每个目标的，改成静态库后
  链接器可能丢掉没人引用的成员，连带丢掉 ImGui 的静态初始化注册。
- **`imgui_impl_opengl3_wrapper.cpp` 这个包装文件不能省**：ImGui 1.92 的
  `IMGUI_IMPL_OPENGL_LOADER_CUSTOM` 只是「不要让 ImGui 自己加载」，不会替你提供一个加载器，
  所以要有一个 TU 先引入自己的 GL 入口、再 `#include "backends/imgui_impl_opengl3.cpp"`。

改构建时还有两条：加了新的 `.cpp` 必须同时加进对应的源文件列表（漏一个会以
`undefined reference` 的形式在链接期才暴露），以及**不要在 MSYS 终端下依赖 `WIN32` 宏做平台判断**
（那里 `CMAKE_SYSTEM_NAME` 是 `MSYS` 而 `WIN32` 为空）。完整约定见 [`AGENTS.md`](AGENTS.md)。

## 文档

- [`docs/bklv-api.md`](docs/bklv-api.md) —— bklv 速查：文件职责、Box 布局调用、
  交互与内容控件、日志约定、已知的坑
- [`docs/README.md`](docs/README.md) —— 文档索引
- [`notes/adding-render-backend.md`](notes/adding-render-backend.md) —— 加渲染后端的步骤
- [`AGENTS.md`](AGENTS.md) —— 开发约定

## 三方依赖

| 子模块 | 固定版本 | 用途 | 许可证 |
|---|---|---|---|
| [`3rd/imgui`](https://github.com/ocornut/imgui) | 上游固定提交 | 即时模式 GUI、控件和绘制；Windows 的 GL 运行时加载也复用它自带的 imgl3w | MIT |
| [`3rd/yoga`](https://github.com/facebook/yoga) | `v3.2.1` | Flexbox 自动布局、尺寸约束和间距 | MIT |
| [`3rd/tweeny`](https://github.com/mobius3/tweeny) | `v4.1.0` | C++17 头文件动画插值、缓动和关键帧 | MIT |
| [`3rd/SDL`](https://github.com/libsdl-org/SDL) | `release-3.4.16` | 窗口、输入事件、手柄和 OpenGL 上下文（源码编译成静态库） | zlib |
| [`3rd/stb`](https://github.com/nothings/stb) | 上游 master | `stb_image.h`：Linux / Windows / Switch 的图片解码 | public domain / MIT |

Yoga 固定在 `v3.2.1` 对应提交 `042f501`，Tweeny 固定在 `v4.1.0` 对应提交 `68a7f44`。
Git 子模块记录精确提交，因此克隆项目后可复现相同版本。

感谢以上项目的作者与维护者。`src/bklv` 移植自
[borealis](https://github.com/xfangfang/borealis)（Apache-2.0），
界面灵感来自 [wiliwili](https://github.com/xfangfang/wiliwili)。

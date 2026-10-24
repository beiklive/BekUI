# bklv

**bklv** 是一个跨平台 C++17 界面库：布局交给 [Yoga](https://github.com/facebook/yoga)
算 Flexbox，绘制交给 [Dear ImGui](https://github.com/ocornut/imgui) 的 `ImDrawList`。
本仓库既是这个库本身，也带一个演示程序（`demo/`）与一组宿主示例（`examples/`）。

- **视图即布局节点**：一个 `View` 持有一个 `YGNode`，尺寸与位置全部由 Yoga 递归算出，
  不需要手写坐标；绘制由 `View::draw()` 递归完成。
- **上层不认图形 API**：控件只接触 `ImDrawList` 和 `ImTextureID`，
  换后端（OpenGL / Vulkan / OpenGL ES 3）不用改一行业务代码。
- **桌面三平台 + Switch**：macOS / Linux / Windows 走 SDL3，
  Switch 走 libnx + EGL/OpenGL ES 3（SDL3 上游没有 Switch 后端）。
- **手柄与键鼠同一套焦点系统**：方向键导航、动作注册、底部按键提示，Switch 与桌面行为一致。

bklv 是从 [borealis](https://github.com/xfangfang/borealis)（[wiliwili](https://github.com/xfangfang/wiliwili)
的界面框架）按需移植的，绘制后端从 NanoVG 换成 ImGui 的 `ImDrawList`。
两者的取舍、以及后续开发计划见 [`TODO.md`](TODO.md)。

## 仓库里有什么

### 库：`bklv/`

**自包含**：库代码、它依赖的第三方（`bklv/3rd/`）、以及它自己的 `CMakeLists.txt`
都在这一层。把 `bklv/` 整个目录拷到别的项目里 `add_subdirectory` 就能用
（见「[用到自己的项目里](#用到自己的项目里)」）。

| 目录 | 文件数 | 内容 |
|---|---|---|
| `foundation/core/` | 28 | 基础机制：`View` 基类与生命周期、别名与类型转换（`asBox()`）、`App` 帧循环、`Context`、外观与 `ThemeData`、`Platform` 抽象、`RenderBackend` 接口、任务与事件、`Logger`、全局状态登记表（`globals.*`，可列举·可重置）、解码队列、i18n、`ui_scale` |
| `foundation/geom/` | 2 | `Point` / `Size` / `Rect` 与绘制期变换 |
| `foundation/json/` | 6 | JSON 工具：取值与诊断、变量块展开（`&` / `@`）、错误策略 |
| `foundation/anim/` | 5 | 时间源、缓动与补间、视图转场、外观 transitions 播放器 |
| `graphics/paint/` | 6 | 渐变顶点网格、柔化阴影（外投影 / 内阴影 / cutout）、焦点框形状与 LUT |
| `graphics/render/` | 36 | `RenderBackend` 接口 + `gl/` `vk/` `gles3/` `libnx/` 四个后端；`glass/` 是液态玻璃子系统（跨后端部分在顶层，`gl/` `vk/` 是后端实现） |
| `ui/view/` | 10 | `View` / `Box`（Yoga Flexbox 与节点管理）/ `ViewTemplate` 两段式、JSON 数据与动作绑定 |
| `ui/theme/` | 20 | 外观与主题：`ThemeData` 与主题解析、`Appearance` 编译（box / layers / states / import）、图层类型表与参数表 |
| `ui/layer/` | 8 | 六个内置层的绘制实现 + 第三方层的注册模板 |
| `ui/widgets/` | 42 | 控件：`appearance/`（外观驱动基类与 `layer_draw_*`）、`controls/`（Button 家族：`TextButton` / `IconButton` / `IconTextButton`）、`basic/`（Label / Image）、`container/`、`list/`（Recycler）、`debug/` |
| `ui/input/` | 8 | 键盘 / 手柄 / 手势、焦点与模态栈、IME |
| `ui/app/` | 4 | `App` 帧循环、`Context`、通知层（组合根） |
| `ui/shell/` | 4 | `Shell` 五层容器（背景 / 内容 / 覆盖 / 浮层 / 调试）+ 焦点框 `FocusRingView` |
| `system/media/` | 15 | 字体与图标字体、图片解码与写出（PNG/JPEG/GIF/WebP/SVG）、音频与音效表 |
| `system/platform/` | 9 | SDL3（桌面三平台）、libnx（Switch）、两套手柄实现与 IME |

四个大类各自回答一个问题：`foundation/` 是"不认识界面的基础"，`graphics/` 是"只认
`ImDrawList` 与渲染接口的绘制"，`ui/` 是"视图树 / 外观 / 控件 / 输入"，`system/` 是
"素材与平台落地"。新增文件先判断职责属于哪一类，再落到对应模块。

目录之间的依赖方向有约定、也有机械检查（`python3 scripts/check_layers.py`，见
[`AGENTS.md`](AGENTS.md) 的「分层依赖方向」）；`docs/architecture-review.html` 里有完整的
分层图与实测的反向依赖清单。

### 演示程序：`demo/`

`demo/` 是重做后的最小框架：一个外壳（顶栏返回 + 标题、内容区、底栏提示）+ 页面栈，
新页面照着 `demo/pages/` 里的两个抄就能加。它只用公开 API，GL 与 Vulkan 两版都编。

```
demo/main.cpp               参数解析：--theme / --page / --screenshot / --frames / --size
demo/demo_app.*             外壳：视图树、页面栈、导航（pushPage / goBack 都推迟到下一帧）
demo/page.hpp               页面接口：getTitle() + build(DemoApp&)
demo/pages/home_page.*      首页：打开设置 / 切换主题 / 发一条通知
demo/pages/settings_page.*  设置页：切换主题 / 返回上一页
demo/style.hpp              从主题取页面底色与文字色
```

产物是 `dist/demo`（GL）与 `dist/vk/demo`（Vulkan），用法见 [`demo/README.md`](demo/README.md)。

### 宿主示例：`examples/`

```
examples/embed_imgui_gl/   把 bklv 嵌进宿主自己的 SDL3 + GL + ImGui 循环
examples/multi_context/    同一个 ImGui 循环里跑两个 bklv Context（default / glass）
examples/switch/           Switch 的最小宿主，scripts/build_switch.sh 编它
examples/cmake_consumer/   安装后 find_package(bklv) 的消费示例
```

前两个由根 `CMakeLists.txt` 编出来（只编 GL），第三个只由 Switch 脚本使用；
它们自带宿主循环，所以验证 Vulkan 路径要用 `demo/` 的 VK 版本。

## 平台支持

| 平台 | 实现方式 | 图片解码 | 状态 |
|---|---|---|---|
| macOS | SDL3（`bklv/system/platform/sdl3_platform.*`） | ImageIO（系统） | 已跑通 |
| Linux | 同上 | stb_image | 已在 Ubuntu 24.04 容器验证构建 |
| Windows | 同上（MSYS2 / MinGW-w64） | stb_image | 已构建并跑通（GL 与 Vulkan） |
| Nintendo Switch | libnx + EGL/GLES3（`bklv/system/platform/switch_platform.*`） | stb_image | 编译产出 .nro 通过；画面待真机确认 |
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

**需要 CMake ≥ 3.24**（SDL3 用 `$<LINK_LIBRARY:FRAMEWORK,...>` 链接 macOS 系统框架、
tweeny 要 ≥ 3.23）。版本不够时配置阶段会直接告诉你缺什么、怎么升级。
除此之外只需要 `cmake`，SDL3 作为子目录一起编，**不需要 `brew install sdl3`**。

推荐用仓库里的 `CMakePresets.json`（CLI 与 VS Code / CLion 共用同一份配置）：

```bash
cmake --preset gl && cmake --build --preset gl     # 产物在 dist/
cmake --preset vk && cmake --build --preset vk     # 产物在 dist/vk/
```

不想用 preset 就手写，效果一样：

```bash
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl
```

> **别配到 `<仓库>/build` 本身。** 本项目的约定是"一个后端一个构建目录"（`build/gl`、`build/vk`），
> `build/` 只是它们的父目录、从来没有 CMakeCache。VS Code 的 CMake Tools 默认会去构建
> `build/`，于是报 `not a CMake build directory (missing CMakeCache.txt)` ——
> 仓库里的 `.vscode/settings.json` 已经把它指到 `gl` preset；
> 手动改的话是 `F1` → `CMake: Select Configure Preset` → 选 `gl`。

### VS Code / CLion 一键编译与运行

四个 preset 自己带生成器，所以**不受 IDE 里选的 kit 影响**：

| preset | 生成器 | 构建目录 | 产物 |
|---|---|---|---|
| `gl` | Visual Studio 17 2022 | `build/gl` | `dist/` |
| `vk` | Visual Studio 17 2022 | `build/vk` | `dist/vk/` |
| `gl-mingw` | Ninja + MSYS2 mingw64 | `build/gl-mingw` | `dist-mingw/` |
| `vk-mingw` | Ninja + MSYS2 mingw64 | `build/vk-mingw` | `dist-mingw/vk/` |

- **为什么必须写死生成器**：不写的话 CMake Tools 会按当前 kit 传 `-G`。kit 是 MinGW/Ninja
  而 `build/gl` 里是 VS 2022 的缓存时，配置直接失败：
  `Error: generator : Ninja / Does not match the generator used previously: Visual Studio 17 2022`。
  写死之后换 kit 也不影响，工具链由 preset 决定。
- **只保留 Release 配置**（`CMAKE_CONFIGURATION_TYPES=Release`）：多配置生成器默认编 Debug，
  而 Debug 与 Release 的产物都往同一个 `dist/` 写 —— 先编 Debug 再编 Release 时，链接器
  认为"exe 比 obj 新"直接跳过，跑起来还是那个 Debug 版（`dist/demo.exe` 从 4.7 MB 变成 14.8 MB）。
  要 Debug 就自己加一个 preset 并把 `CMAKE_CONFIGURATION_TYPES` 写回 `Debug;Release`。
- **编译**：`Ctrl+Shift+B` 走 `.vscode/tasks.json` 的默认任务（= `cmake --build --preset gl`）；
  或者用底部状态栏 CMake Tools 的 Build 按钮（选的就是 `gl` preset）。
- **运行**：`F5` 走 `.vscode/launch.json` —— 三个配置分别对应 MSVC、MinGW、Vulkan 产物，
  都带 `preLaunchTask`（先编再起）。`cwd` 指到产物目录，因为资源按 `<exe 目录>/resources` 找。
  MinGW 那个用 `cppdbg` + `E:/bin/msys64/mingw64/bin/gdb.exe`，路径不同就改 `miDebuggerPath`。
- **CLion**：直接 `File → Open` 仓库根，它会读 `CMakePresets.json`，挑 `gl` 即可。
- **两套工具链的产物分开**：MSVC 的 preset 写 `dist/`，MinGW 的 preset 用 `-DBKLV_OUT_DIR` 指到 `dist-mingw/`。否则同一份 `dist/demo.exe` 会被两套工具链轮流覆盖，配错调试器时很难看出来。
- 需要装的扩展见 `.vscode/extensions.json`：`ms-vscode.cmake-tools` + `ms-vscode.cpptools`。

产物在 `dist/`。想编 Vulkan 版就换一个构建目录，两版可以并存：

```bash
cmake -S . -B build/vk -DBKLV_GRAPHICS=vk && cmake --build build/vk   # 产物在 dist/vk/
```

不需要 Vulkan 就别碰 `build/vk`：只有它需要额外的 Vulkan SDK（见下）。
`-DBKLV_TARGETS=<完整名字>` 可以只编指定目标（逗号分隔多个），目前只有一个目标。

### 图片格式与可选解码器

| 格式 | 解码器 | 备注 |
|---|---|---|
| PNG / JPEG / BMP | macOS 用系统 ImageIO，其余平台用 stb_image | 基础位图 |
| GIF（静态 + 动图） | stb_image（全平台同一份实现） | 逐帧播放，帧已在解码器里合成好 |
| WebP（静态 + 动图） | libwebp（`bklv/3rd/webp`） | 用 `-DBKLV_WITH_WEBP=OFF` 关掉 |
| SVG | lunasvg + plutovg | 用 `-DBKLV_WITH_SVG=OFF` 关掉；按视图尺寸栅格化 |
| HEIC / TIFF / ICO 等 | 只有 macOS 的 ImageIO 能解 | 非 macOS 上会明确报"不支持该格式" |

两个可选解码器**缺子模块也不会让配置失败**：没拉下来时对应功能自动关闭，
调用时返回一句明确的错误（而不是编译不过）。Switch 那边用
`./scripts/build_switch.sh --no-webp / --no-svg` 关。

**像素统一是非预乘 RGBA8**（ImGui 的混合方程是直通 alpha：GL 用
`SRC_ALPHA/ONE_MINUS_SRC_ALPHA`，Vulkan 用 `VK_BLEND_FACTOR_SRC_ALPHA`）。
macOS 的 ImageIO 与 lunasvg 的位图都是预乘的，两条路都在解码后做了反预乘，
否则半透明图会整体偏暗。解码前还会按头部卡尺寸上限（单边 8192、单帧 4K 像素）。

### 运行

```bash
./dist/embed_imgui_gl      # 宿主嵌入示例（默认编出来的两个目标之一）
./dist/multi_context
```

`embed_imgui_gl` 会同时显示 bklv 的界面和宿主自己的一小块 ImGui 窗口，
用来验证"两个 ImGui 使用者共用一个帧"；`multi_context` 在同一次循环里跑两棵
bklv 页面树（default 与 glass 主题），日志里能看到两次主题装载。

### 资源文件

`resources/` 下的素材（示例用了 `testcard.png`）按**资源目录**解析，不是工作目录，
所以从哪个目录启动都能找到：

| 平台 | 资源怎么到程序旁边 | 运行时路径 |
|---|---|---|
| macOS / Linux / Windows | 构建后 `resources/` 自动复制到可执行文件目录（CMake 的 POST_BUILD） | `<可执行文件目录>/resources/...` |
| Switch | 用 `build_romfs` 打包，再由 `elf2nro --romfs` 嵌进 nro | `romfs:/resources/...`（`romfsInit()` 挂载） |

代码里统一写相对路径，由 `bklv::resourcePath()` 按平台解析（见 `bklv/foundation/core/resource.hpp`）：

```cpp
image->setImageFromFile("resources/testcard.png");
```

`Image::setImageFromFile()` 内部就会解析，不需要自己在调用点拼路径。

界面也可以用 JSON 描述（解析层在 `bklv/ui/view/view_parse.*`）。能用哪些类型取决于
`bklv/ui/widgets/appearance/widget_types.cpp` 里注册了什么 —— 清理后注册的是 Button 家族（`"type": "Button"` 等价于 `TextButton`）：

```cpp
bklv::View* view = bklv::createFromJSONFile("resources/views/xxx.json");  // 路径相对资源目录
```

语法与全部字段见 [`docs/json-syntax.md`](docs/json-syntax.md)。仓库里暂时没有随包的
示例 JSON（`resources/views/` 目前没随仓库提供，照语法写一份就能用）。

文案也走同一套资源目录：`resources/i18n/zh-Hans.json`、`resources/i18n/en-US.json`，
用 `bklv::translate("hints/ok")` 取（`Button` 自己的动作名就来自这条）；
桌面可用 `BKLV_LOCALE=en-US` 覆盖系统语言。

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
build.bat --only=embed_imgui_gl  :: 只编指定目标（可写 a,b 多个）
build.bat --gl --gui      :: 不弹控制台窗口（默认保留控制台，方便看日志）
build.bat --sdl-force     :: 删掉构建目录重新配置
```

```bat
dist\embed_imgui_gl.exe
```

几点 Windows 专属的注意：

- **直接用 cmake 的话，要在「MSYS2 MINGW64」终端里。** 默认的 MSYS 终端
  `MINGW_PREFIX` 是空的，CMake 找不到编译器；`CMakeLists.txt` 只在 `MINGW_PREFIX`
  存在时兜底，找不到就打印一句提示，不会去猜路径。用 `build.bat` 不受影响。
- **OpenGL 是运行时加载的。** `opengl32.dll` 只导出 GL 1.1，GL 3.2 Core 的函数必须取地址，
  `bklv/graphics/render/gl/gl_loader.cpp` 复用 ImGui 自带的 imgl3w 完成，不引入 glad/gl3w 之类的依赖。
- **中文字体走系统字体**（优先微软雅黑，其次黑体、宋体），一个都找不到才退回 ImGui 内置字体，
  那时中文会显示成方块。
- **Vulkan 的颜色格式选 UNORM，不选 sRGB**，否则整屏会发灰。细节见 `docs/bklv-api.md`
  与 `AGENTS.md`。
- **MSVC 也能编**（VS 2022 / MSVC 14.44 验证过）。注意生成器是**配置时**定下的：
  PATH 里有 MinGW 时 `cmake` 默认会选 Ninja + gcc，要 MSVC 就得显式要生成器
  （新的构建目录才行，换生成器必须删目录；不要加 `-A`，默认就是 x64）：

  ```bat
  cmake -S . -B build\gl -G "Visual Studio 17 2022"
  cmake --build build\gl --config Release
  ```

  Vulkan 换 `build\vk`，**`-DBKLV_GRAPHICS` 必须显式给**（默认是 gl，漏了会在
  `build\vk` 里编出一份 GL 版、产物落到 `dist\` 而不是 `dist\vk\`）：

  ```bat
  cmake -S . -B build\vk -G "Visual Studio 17 2022" -DBKLV_GRAPHICS=vk
  cmake --build build\vk --config Release
  ```

  两种后端编出来都是零 error 零 warning。与 MinGW 的差异都是移植性问题：
  `unistd.h` / `setenv` / `std::rename` 覆盖语义 / GCC 专用选项 / 源码里的 UTF-8 中文
  （已加 `/utf-8`、`LUNASVG_BUILD_STATIC`）。

  vendored `tweeny` 有一处 MSVC 不接受的 lambda 写法。补丁**只在 MSVC 下、由 CMake 在
  配置阶段自动打**（`bklv/CMakeLists.txt` 里 `if(MSVC)` 那一段），打的是子模块的工作树
  文件，gitlink 仍指向上游 `68a7f44`（所以 `git status` 会显示子模块 modified，但不必、
  也不该把改过的子模块提交进去）。补丁内容就是 `notes/tweeny-msvc-render.patch`，
  打过了会跳过，打不上会直接报错并提示 `git submodule update --force bklv/3rd/tweeny`。

### Switch（devkitPro）

Switch 不能并进 CMake：工具链（devkitA64）与编译参数和桌面完全不同，也链接不了 SDL3，
所以它走独立脚本。

```bash
./scripts/build_switch.sh            # 产出 dist/switch/bklv_switch.nro
./scripts/build_switch.sh --clean    # 丢掉中间产物重新编译
nxlink -s dist/switch/bklv_switch.nro  # 推到机器（需机器上开着 hbmenu 的网络加载）
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

`bklv/` 是自包含的，把整个目录拷进你的项目，然后 `add_subdirectory`：

```
your_project/
├── CMakeLists.txt
├── third_party/bklv/          ← 从本仓库整个拷过来（含它自己的 3rd/）
│   ├── foundation/ graphics/ ui/ system/
│   ├── 3rd/                   imgui / yoga / tweeny / SDL / stb
│   └── CMakeLists.txt
└── src/main.cpp
```

```cmake
add_subdirectory(third_party/bklv)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE bklv::bklv)
```

包含路径不用自己加：`bklv::bklv` 会把 `bklv/` 的父目录一起带出来，所以直接写

```cpp
#include "bklv/ui/app/app.hpp"
```

`-DBKLV_GRAPHICS=gl|vk` 选图形后端；它不设默认值时会自己判成 `gl`。
图形后端是**编译期**决定的，所以同一个工程要出两版就是两个构建目录。

### 全局状态

库里的进程级状态（事件总线、输入管理器、纹理仓库、几个注册表、当前 `App`）都登记在
一张表里：`bklv::globals::getGlobalStore()`（见 `bklv/foundation/core/globals.hpp`）。三件事因此可做：

```cpp
auto& store = bklv::globals::getGlobalStore();
store.setNumber("myApp.uiScale", 1.5f);      // 放自己的全局变量，省一个没人管的 static
for (const std::string& line : store.describe())  // 列举：这台进程里都有什么
    BKLV_LOG_INFO("%s", line.c_str());
bklv::globals::resetGlobals();               // 收尾：一轮结束（换 App / 测试之间）
```

约定是"新增进程级状态一律登记到这里"，细节与三条注意事项见 `AGENTS.md` 的「全局状态」。
库本身仍是**单实例**（一个进程一套界面，有全局 ImGui 上下文）；`resetGlobals()` 解决的是
"状态能清干净"，不是"同时跑两个"。

### 主题（两套）

- `default`：**纯扁平风格** —— 无阴影、无玻璃、无渐变，靠 1px 描边分面
- `glass`：**玻璃质感** —— 只覆盖少量 tokens（配色 / 圆角），外观与 `files[]` 留空，缺的逐键回退 `default`；是最小主题的样板

`resources/theme/themes.json` 的 `available` 列的就是这两套，`app.setTheme("glass")` 运行时切换。
要做自己的皮肤：复制一套改名改 JSON、登记进 `themes.json`（名字 = 目录名，清单写在各自
`theme.json` 的 `files[]` 里）。规矩见 `AGENTS.md` 的「主题 / 外观」与 `docs/base-widgets-map.md`。

### 帧预算（界面永不卡）

`App::setFrameBudgetMs(4)`（默认 4ms）给"可以延后"的主线程工作一个每帧时间上限：后台回调、
纹理上传、动图换帧、SVG 栅格化、纹理缓存淘汰用完就停手、下一帧继续；布局 / 输入 / 绘制不吃预算。
`App::getFrameStats()` 给 p50 / p95 / p99 / 最坏帧 / 掉帧数，每个程序退出时也会自动打一行帧时间流水。
耗时入口（帧内同步加载图片）会给一条节流警告；设计与四条约定见 `AGENTS.md` 的「帧预算」。

### 分层

```
demo/                                 演示程序（最小框架：外壳 + 页面栈 + 两个页面）
examples/                             宿主示例（嵌入 / 双 Context / Switch / 安装后消费）
        ↓
bklv/ui/shell/*                            应用外壳：五层容器 + 焦点框（只认识 View 与焦点接口）
bklv/ui/widgets/*                          外观驱动控件：Button 家族 / Notification（样式来自主题 JSON）
        ↓                               （样式来自主题 JSON，代码只写行为）
bklv/ui/widgets/*                            Image / Label / ScrollingFrame / RecyclerFrame
                                        只认 ImDrawList 和 ImTextureID
        ↓
bklv/ui/theme/*                          解析层：JSON → 外观 / 主题 / 视图模板
        ↓
bklv/foundation/core/*                             View / Box / App / 外观与主题 / 输入 / 解码 / i18n
  ├ graphics.hpp                        RenderBackend 接口 + 纹理管理
  ├ platform.hpp                        Platform 接口（窗口 / 事件循环 / 系统能力）
  └ glass/                              液态玻璃子系统（服务 + 请求队列 + 模糊计划 + 调参）
        ↓
bklv/graphics/render/*                         OpenGLBackend / VulkanBackend / GLES3Backend
                                        （玻璃的模糊实现在 backends/gl_glass.* 与 vk_glass.*）
bklv/system/platform/*                        PlatformSDL3 / PlatformSwitch
```

**依赖方向是单向的、并且有检查脚本**：`widgets → views → core`、
`compile → core`、`backends → core`、`platforms → core/backends`。
反过来（例如 `core` include `widgets`）属于违规，`scripts/check_layers.py` 会拦下来 ——
因为所有层都编进同一个 OBJECT 库，编译器管不了这件事，只能靠脚本兜住。
规则、允许方向与存量清单见 [`AGENTS.md`](AGENTS.md) 的「分层依赖方向」。

**view 层不接触图形 API**，所以「换后端」和「换平台」都是加一个子类、
在 `createBackend()` 里多一个分支，业务代码不用动：

- **换后端**：实现 `RenderBackend` 的那 11 个纯虚方法。步骤和三个后端都踩过的坑见
  [`notes/adding-render-backend.md`](notes/adding-render-backend.md)。
- **换平台**：实现 `Platform` 子类。Switch 那份（`bklv/system/platform/switch_platform.*`）
  是最小参考实现——没有 SDL3，窗口、事件循环、电源/网络/语言能力都自己接。
- **只要一部分**：`bklv/CMakeLists.txt` 里的 `BKLV_CORE_SOURCES` 就是库的全部源文件，
  想裁剪就从那里删；`bklv/graphics/render/` 与 `bklv/system/platform/` 里没用到的后端可以整个去掉
  （它们在源码里也有 `BKLV_SWITCH` 之类的条件编译守卫）。
- **别直接改 `bklv/3rd/` 下的子模块**，它们是指定提交的第三方代码，改动会在更新子模块时丢失。
  需要绕过的都在项目自己的头文件里处理（例如 Windows 的 GL 加载、
  `gl_loader.hpp` 里 `#undef ERROR` 那个 `wingdi.h` 宏冲突）。

### 库里几条容易踩的实现约定

`bklv/CMakeLists.txt` 里都有注释，这里列出来是因为改这个文件时最容易中招：

- **Yoga 不要 `add_subdirectory`**：它自己的 `cmake/project-defaults.cmake` 会
  `add_compile_options(-Wall -Werror -fno-rtti)` 并设 `CMAKE_CXX_STANDARD 20`，
  作用域是目录级，会污染整个工程，`-Werror` 直接破坏零警告约定。
  正确做法是把它编成自己的 `bklv_yoga` 目标、单独用 C++20 编译。
- **ImGui 编成 OBJECT 库而不是 STATIC 库**：编进静态库后链接器会按需抽取目标文件，
  可能丢掉没人引用的成员，连带丢掉 ImGui 里靠静态初始化注册的东西。
- **OBJECT 库的目标文件不会透过 INTERFACE 库传递**：`bklv::bklv` 是 INTERFACE 目标，
  接口里必须显式写 `$<TARGET_OBJECTS:bklv_core>` / `$<TARGET_OBJECTS:bklv_imgui>`，
  否则调用方链接时一个库对象都拿不到。
- **`imgui_impl_opengl3_wrapper.cpp` 这个包装文件不能省**：ImGui 的
  `IMGUI_IMPL_OPENGL_LOADER_CUSTOM` 只是「不要让 ImGui 自己加载」，不会替你提供一个加载器，
  所以要有一个 TU 先引入自己的 GL 入口、再 `#include "backends/imgui_impl_opengl3.cpp"`。
- **C++17 声明在 `bklv_opts` 上**，不要只写在可执行目标上：OBJECT 库不受可执行目标的
  `target_compile_features` 影响，会退回编译器默认标准（Apple clang 21 的默认是 C++14）。

完整约定见 [`AGENTS.md`](AGENTS.md)。

## 文档

- [`TODO.md`](TODO.md) —— 开发计划：库化、控件补全、技术债、与 borealis 的取舍
- [`docs/bklv-api.md`](docs/bklv-api.md) —— bklv 速查：文件职责、Box 布局调用、
  交互与内容控件、日志约定、已知的坑
- [`docs/README.md`](docs/README.md) —— 文档索引
- [`notes/adding-render-backend.md`](notes/adding-render-backend.md) —— 加渲染后端的步骤
- [`AGENTS.md`](AGENTS.md) —— 开发约定

## 三方依赖

| 子模块 | 固定版本 | 用途 | 许可证 |
|---|---|---|---|
| [`bklv/3rd/imgui`](https://github.com/ocornut/imgui) | 上游固定提交 | 即时模式 GUI、控件和绘制；Windows 的 GL 运行时加载也复用它自带的 imgl3w | MIT |
| [`bklv/3rd/yoga`](https://github.com/facebook/yoga) | `v3.2.1` | Flexbox 自动布局、尺寸约束和间距 | MIT |
| [`bklv/3rd/tweeny`](https://github.com/mobius3/tweeny) | `v4.1.0` | C++17 头文件动画插值、缓动和关键帧（库当前未用，保留给后续的属性动画） | MIT |
| [`bklv/3rd/SDL`](https://github.com/libsdl-org/SDL) | `release-3.4.16` | 窗口、输入事件、手柄和 OpenGL 上下文（源码编译成静态库） | zlib |
| [`bklv/3rd/stb`](https://github.com/nothings/stb) | 上游 master | `stb_image.h`：Linux / Windows / Switch 的位图解码 + GIF（含逐帧动画） | public domain / MIT |
| [`bklv/3rd/webp`](https://github.com/webmproject/libwebp) | `v1.5.0` | WebP 解码（静态 + 动图），只编解码器与 demux | BSD-3-Clause |
| [`bklv/3rd/lunasvg`](https://github.com/sammycage/lunasvg) | `v3.1.0` | SVG 解析与栅格化 | MIT |
| [`bklv/3rd/plutovg`](https://github.com/sammycage/plutovg) | 上游 main（固定提交） | lunasvg 的光栅化后端 | MIT |

Yoga 固定在 `v3.2.1` 对应提交 `042f501`，Tweeny 固定在 `v4.1.0` 对应提交 `68a7f44`。
Git 子模块记录精确提交，因此克隆项目后可复现相同版本。

感谢以上项目的作者与维护者。`bklv/` 移植自
[borealis](https://github.com/xfangfang/borealis)（Apache-2.0），
界面灵感来自 [wiliwili](https://github.com/xfangfang/wiliwili)。

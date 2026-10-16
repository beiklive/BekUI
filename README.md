# bklv

**bklv** 是一个跨平台 C++17 界面库：布局交给 [Yoga](https://github.com/facebook/yoga)
算 Flexbox，绘制交给 [Dear ImGui](https://github.com/ocornut/imgui) 的 `ImDrawList`。
本仓库既是这个库本身，也是一组用它写的 demo。

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
| `core/` | 79 | `View` / `Box`（Flexbox 与节点管理）/ 外观与 `ThemeData` / 焦点与输入 / `App` 帧循环 / `Platform` 抽象 / `RenderBackend` / 解码 / i18n / 任务与事件 / `Logger` / 全局状态登记表（`globals.*`，可列举·可重置）；**`core/glass/`** 是液态玻璃子系统（请求与队列、模糊计划、几何、调参、服务，以及对后端的抽象接口 `glass_backend.hpp`），它的后端实现在 `backends/gl_glass.*`、`backends/vk_glass.*`，由各图形后端持有并交给服务 |
| `views/` | 24 | `Label`（换行·省略号·对齐）、`Image`（缩放·动图）、`ScrollingFrame`（惯性 + 滚动条）、`HScrollingFrame`、`RecyclerFrame`（对象池虚拟列表）、`Padding`、`ProgressSpinner`、`DebugLayer`/`DebugLogView`（日志面板）、`data_source`/`item_binder`（纯数据注册表） |
| `widgets/` | 53 | **外观驱动的控件**：`AppearanceView`（基类）、`Shell`（四层外壳）、`Pager`、`Header`/`BottomBar`/`Sidebar`/`TabFrame`、`Button`/`SelectorButton`/`Slider`、Cell 家族（6 种）、`Dialog`/`Dropdown`/`EditTextDialog`/`Notification`、`HintsBar`/`StatusArea`/`AccountWidget`、`RecyclerView`（JSON 桥接）、六个 `layer_draw_*` 绘制钩子 |
| `compile/` | 18 | **解析层**：`view_parse`（JSON → 视图树，两段式模板）、`theme_parse`（JSON → `ThemeData`）、外观的 box / imports / layer_fields / layer_meta / children / gradient、`layer_registry`（图层类型的唯一事实来源）+ `layer_register`（注册模板） |
| `backends/` | 21 | OpenGL 3.2 Core、Vulkan、OpenGL ES 3、Windows 的 GL 运行时加载 |
| `platforms/` | 9 | SDL3（桌面三平台）、libnx（Switch）、两套手柄实现与 IME |

目录之间的依赖方向有约定、也有机械检查（`python3 scripts/check_layers.py`，见
[`AGENTS.md`](AGENTS.md) 的「分层依赖方向」）；`docs/architecture-review.html` 里有完整的
分层图与实测的反向依赖清单。

### demo：`demo/`

一个展示程序 `demo`，按 Tab 展示库的能力：**控件 / 文本 / 布局 / 列表**。
右上角「显示日志」按钮打开 ImGui 日志窗口（可拖动、可缩放、按级别过滤）。

```
demo/main.cpp    入口
demo/app/        应用层：页面宿主、tab 栏、自定义控件、各页面
```

这一层属于 demo 而不属于库：它用 bklv 的控件拼出「能力总览」这个具体界面。

demo 编 GL 和 Vulkan 两个版本。两条渲染路径的后端代码、链接库、shader 版本都不同，
只在一个后端上验证过的界面代码不算验证过；后端是编译期决定的，所以靠两个构建目录
各编一份，没有运行时开关。

## 平台支持

| 平台 | 实现方式 | 图片解码 | 状态 |
|---|---|---|---|
| macOS | SDL3（`bklv/platforms/sdl3_platform.*`） | ImageIO（系统） | 已跑通 |
| Linux | 同上 | stb_image | 已在 Ubuntu 24.04 容器验证构建 |
| Windows | 同上（MSYS2 / MinGW-w64） | stb_image | 已构建并跑通（GL 与 Vulkan） |
| Nintendo Switch | libnx + EGL/GLES3（`bklv/platforms/switch_platform.*`） | stb_image | 编译产出 .nro 通过；画面待真机确认 |
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
ctest --preset gl                                  # 无窗口测试（37 个）
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
./dist/demo
```

`demo` 右上角有「显示日志」按钮，点开是 ImGui 日志窗口（可拖动、可缩放、
按级别过滤）。没有快捷键开关。

### 资源文件

`resources/` 下的素材（demo 用了一张 `testcard.png`）按**资源目录**解析，不是工作目录，
所以从哪个目录启动都能找到：

| 平台 | 资源怎么到程序旁边 | 运行时路径 |
|---|---|---|
| macOS / Linux / Windows | 构建后 `resources/` 自动复制到可执行文件目录（CMake 的 POST_BUILD） | `<可执行文件目录>/resources/...` |
| Switch | 用 `build_romfs` 打包，再由 `elf2nro --romfs` 嵌进 nro | `romfs:/resources/...`（`romfsInit()` 挂载） |

代码里统一写相对路径，由 `bklv::resourcePath()` 按平台解析（见 `bklv/core/resource.hpp`）：

```cpp
image->setImageFromFile("resources/testcard.png");
```

`Image::setImageFromFile()` 内部就会解析，不需要自己在调用点拼路径。

界面也可以用 JSON 描述（`resources/views/*.json`），demo 的 JSON 页就是整块界面来自 JSON，
列表项模板也来自 JSON、数据由 C++ 侧的回调绑定：

```cpp
bklv::View* view = bklv::createFromJSONFile("resources/views/page_json.json");
```

文案也走同一套资源目录：`resources/i18n/zh-Hans.json`、`resources/i18n/en-US.json`，
用 `bklv::getStr("demo/welcome")` 取；桌面可用 `BKLV_LOCALE=en-US` 覆盖系统语言。

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
build.bat --only=demo     :: 只编指定目标
build.bat --gl --gui      :: 不弹控制台窗口（默认保留控制台，方便看日志）
build.bat --sdl-force     :: 删掉构建目录重新配置
```

```bat
dist\demo.exe
```

几点 Windows 专属的注意：

- **直接用 cmake 的话，要在「MSYS2 MINGW64」终端里。** 默认的 MSYS 终端
  `MINGW_PREFIX` 是空的，CMake 找不到编译器；`CMakeLists.txt` 只在 `MINGW_PREFIX`
  存在时兜底，找不到就打印一句提示，不会去猜路径。用 `build.bat` 不受影响。
- **OpenGL 是运行时加载的。** `opengl32.dll` 只导出 GL 1.1，GL 3.2 Core 的函数必须取地址，
  `bklv/backends/gl_loader.cpp` 复用 ImGui 自带的 imgl3w 完成，不引入 glad/gl3w 之类的依赖。
- **中文字体走系统字体**（优先微软雅黑，其次黑体、宋体），一个都找不到才退回 ImGui 内置字体，
  那时中文会显示成方块。
- **Vulkan 的颜色格式选 UNORM，不选 sRGB**，否则整屏会发灰。细节见 `docs/bklv-api.md`
  与 `AGENTS.md`。
- **MSVC 未验证**，这套构建走的是 MinGW-w64。

### Switch（devkitPro）

Switch 不能并进 CMake：工具链（devkitA64）与编译参数和桌面完全不同，也链接不了 SDL3，
所以它走独立脚本。

```bash
./scripts/build_switch.sh            # 产出 dist/switch/demo.nro
./scripts/build_switch.sh --clean    # 丢掉中间产物重新编译
nxlink -s dist/switch/demo.nro       # 推到机器（需机器上开着 hbmenu 的网络加载）
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
│   ├── core/ views/ backends/ platforms/
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
#include "bklv/core/app.hpp"
```

`-DBKLV_GRAPHICS=gl|vk` 选图形后端；它不设默认值时会自己判成 `gl`。
图形后端是**编译期**决定的，所以同一个工程要出两版就是两个构建目录。

### 全局状态

库里的进程级状态（事件总线、输入管理器、纹理仓库、几个注册表、当前 `App`）都登记在
一张表里：`bklv::globals::getGlobalStore()`（见 `bklv/core/globals.hpp`）。三件事因此可做：

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

### 分层

```
demo/  designer/                       应用与工具（都用库，互不依赖）
        ↓
bklv/widgets/*                          外观驱动控件：Shell / Pager / Button / Cell / Dialog
        ↓                               （样式来自主题 JSON，代码只写行为）
bklv/views/*                            Image / Label / ScrollingFrame / RecyclerFrame
                                        只认 ImDrawList 和 ImTextureID
        ↓
bklv/compile/*                          解析层：JSON → 外观 / 主题 / 视图模板
        ↓
bklv/core/*                             View / Box / App / 外观与主题 / 输入 / 解码 / i18n
  ├ graphics.hpp                        RenderBackend 接口 + 纹理管理
  ├ platform.hpp                        Platform 接口（窗口 / 事件循环 / 系统能力）
  └ glass/                              液态玻璃子系统（服务 + 请求队列 + 模糊计划 + 调参）
        ↓
bklv/backends/*                         OpenGLBackend / VulkanBackend / GLES3Backend
                                        （玻璃的模糊实现在 backends/gl_glass.* 与 vk_glass.*）
bklv/platforms/*                        PlatformSDL3 / PlatformSwitch
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
- **换平台**：实现 `Platform` 子类。Switch 那份（`bklv/platforms/switch_platform.*`）
  是最小参考实现——没有 SDL3，窗口、事件循环、电源/网络/语言能力都自己接。
- **只要一部分**：`bklv/CMakeLists.txt` 里的 `BKLV_CORE_SOURCES` 就是库的全部源文件，
  想裁剪就从那里删；`bklv/backends/` 与 `bklv/platforms/` 里没用到的可以整个去掉
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

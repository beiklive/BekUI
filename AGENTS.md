# AGENTS.md — 本项目的开发约定

## 图形后端：demo 一律双版本

**每个 demo 都要能编出 GL 和 Vulkan 两个版本**，不是二选一。

原因：GL 和 Vulkan 是两条完全不同的渲染路径（上下文创建、shader、资源生命周期都不同），
只在一个后端上验证过的界面代码不算验证过。双版本让「上层与图形 API 无关」这条设计
随时可被检验——如果某个 demo 换后端跑不起来，说明它漏了抽象。

### 构建

构建入口只有一个：根目录的 `CMakeLists.txt`。后端是编译期决定的，所以
「每个 demo 都能编 GL 和 Vulkan 两版」靠**两个构建目录**实现：

```bash
# GL
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl

# Vulkan
cmake -S . -B build/vk -DBKLV_GRAPHICS=vk && cmake --build build/vk

# 只编某个目标（BKLV_TARGETS 按完整名字匹配，可逗号分隔多个）；
# 目前只有一个目标 demo，这个开关留着是为了以后加示例程序时不用改构建
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl -DBKLV_TARGETS=demo && cmake --build build/gl
```

Windows（MSYS2 / MinGW-w64）用根目录的 `build.bat`，它是同一批参数：
　　
```bat
build.bat                       :: GL
build.bat --vk                  :: Vulkan
build.bat --only=demo           :: 只编主程序
build.bat --gl --gui            :: 不弹控制台（会看不到 BKLV_LOG_*）
build.bat --sdl-force           :: 删掉构建目录重新配置
```

`build.bat` 只负责找 MSYS2 和 MinGW 工具链、把工具链放进 `PATH`，真正的构建逻辑
仍然只有 `CMakeLists.txt` 一份。

**Windows 上要用「MSYS2 MINGW64」终端。** 在默认的 MSYS 终端里 `MINGW_PREFIX`
是空的，CMake 找不到编译器；`CMakeLists.txt` 会用 `MINGW_PREFIX` 兜底，找不到时
只打印一句提示，不会去猜别的路径。

产物分目录，互不覆盖：

| 构建 | 输出目录 | Windows |
|---|---|---|
| GL | `dist/` | `dist/*.exe` |
| Vulkan | `dist/vk/` | `dist/vk/*.exe` |
| Switch | `dist/switch/` | — |

因为分目录，可执行文件名不带后缀（Windows 上带 `.exe`）。

### 加新 demo 时

在 `CMakeLists.txt` 的 `BKLV_TARGET_LIST` 里加一行，两个后端会自动各编一份；
不要写任何依赖具体图形 API 的代码。需要图形 API 能力时，走 `RenderBackend` 接口。

```cmake
"demo_08_xxx|demo/demo_08_xxx.cpp|core"
#                                ↑ core = 只依赖 bklv 库；app = 还要 demo/app 那层
```

每个目标都链接 `bklv::bklv`；SDL3 / OpenGL / Vulkan / ImGui 都由它带出来，
不要在目标上重复列。

### 改构建时注意

- 加了新的 `.cpp` 要加进对应的源文件列表：**库**的加 `bklv/CMakeLists.txt`
  （`BKLV_CORE_SOURCES` / `BKLV_IMGUI_SOURCES`），**demo** 的加根 `CMakeLists.txt`
  （`BKLV_APP_SOURCES`）。漏一个会以 `undefined reference` 的形式在链接期才暴露。
  Switch 的 `scripts/build_switch.sh` 对 core / views 是按目录收集的，
  只有 backends / platforms 需要手列——新增文件时留意它是否也要加进去。
- `bklv/3rd/yoga/yoga` **不要** `add_subdirectory`：它自己的 `cmake/project-defaults.cmake`
  会 `add_compile_options(-Wall -Werror -fno-rtti)` 并设 `CMAKE_CXX_STANDARD 20`，
  作用域是目录级，会污染整个工程（`-Werror` 直接破坏零警告约定）。
  项目里是把它编成自己的 `bklv_yoga` 目标、用 C++20 编译。
- ImGui 编成 OBJECT 库而不是 STATIC 库：编进静态库后链接器会按需抽取目标文件，
  可能丢掉没人引用的成员，连带丢掉 ImGui 里靠静态初始化注册的东西。
- **OBJECT 库的目标文件不会透过 INTERFACE 库传给调用方**。`bklv::bklv` 是
  INTERFACE 目标，所以它的接口里必须显式写 `$<TARGET_OBJECTS:bklv_core>` 与
  `$<TARGET_OBJECTS:bklv_imgui>`；只写 `target_link_libraries(... bklv_core)` 的话
  调用方链接时一个库对象都拿不到，报一堆 `bklv::*` / `ImDrawList::*` undefined。

### 后端选择是编译期的事

见 `notes/adding-render-backend.md`。选择点在 `bklv/platforms/*.cpp` 的
`createBackend()`，不在 App 里做运行时切换。

---

## 平台

| 平台 | 构建 | 状态 |
|---|---|---|
| macOS | `cmake -S . -B build/gl` | 已完成 |
| Linux | `cmake -S . -B build/gl`（g++ + pkg-config） | 已完成 |
| Windows | `build.bat`（找 MSYS2/MinGW，再驱动 CMake） | 已完成 |
| Switch | `./scripts/build_switch.sh` | 编译通过，画面待真机验证 |
| Android / iOS | 未做 | 待办 |

`Switch` 走独立脚本：工具链（devkitA64）与编译参数和桌面完全不同，且不能链接 SDL3，
CMake 覆盖不了，所以 `scripts/build_switch.sh` 是唯一保留的构建脚本。

桌面（macOS / Linux / Windows）上不再有 shell 构建脚本，`build.sh` 与
`scripts/build_sdl3.sh` 已删除；SDL3 作为 `add_subdirectory` 的子项目随 CMake 一起编。

Windows 上有几条平台约定，改相关代码时要留意：

- **在 MSYS2 里要用「MINGW64」终端**：默认的 MSYS 终端 `MINGW_PREFIX` 为空，
  CMake 找不到编译器。`build.bat` 会自己把工具链放进 `PATH`，不受此影响。
- CMake 里的平台判定不能只看 `WIN32`：在 MSYS 终端里 `CMAKE_SYSTEM_NAME` 是 `MSYS`
  而 `WIN32` 为空。需要跨平台分支时用 `WIN32`，需要区分 MSYS 用 `CMAKE_SYSTEM_NAME`。
- `opengl32.dll` 只导出 GL 1.1，GL 3.2 Core 的函数必须在 `loadGLFunctions()`
  里运行时取地址（`bklv/backends/gl_loader.cpp`，复用 ImGui 自带的 imgl3w）。
  **任何在 GL 上下文就绪后直接调 `gl*` 的地方，都要先确保它被调用过。**
- 不要在 `REM` 行里写右括号：cmd 会把它当成语句块结束，让后面的命令被吞掉。
- JSON 用 `bklv/3rd/json`（nlohmann/json **v3.11.3**）。**不要降到 v3.11.2**：
  那版在 Clang 下会对 `operator "" _json` 报 `-Wdeprecated-literal-operator`，
  4 处警告都在库头里，直接破坏零警告约定（v3.11.3 已修）。
- **资源文件按"资源目录"解析，不是工作目录**（`bklv/core/resource.hpp`）：
  桌面由 CMake 的 POST_BUILD 把 `resources/` 复制到可执行文件目录；
  Switch 用 `build_romfs` + `elf2nro --romfs` 打进 nro，运行时 `romfsInit()` 挂成 `romfs:/`。
  代码里统一写 `"resources/xxx"`，`Image::setImageFromFile()` 会自己解析。
  **不要用 elf2nro 的 `--romfsdir`**：这个工具链里的 elf2nro 会静默忽略它
  （参数能解析，但同时给 `--romfs` 与 `--romfsdir` 会报冲突；而只用 `--romfsdir`
  时产物与不带参数逐字节相同，NRO 里既无 ASET 段也无 IVFC）。必须两步：
  `build_romfs <dir> <img>` 再 `elf2nro ... --romfs=<img>`。
- 中文字体各平台拿法不同（`bklv/core/font.cpp`），别只写 macOS 的系统字体路径：
  桌面读系统字体文件；**Switch 上没有可读的系统字体目录**，要用 `pl` 系统服务
  （`plInitialize(PlServiceType_User)` + `plGetSharedFontByType`）拿内置共享字体，
  再把 Standard / 简中扩展 / 繁中 / 韩文合并成 fallback 链。
  注意 `AddFontFromMemoryTTF` 默认接管内存所有权，共享内存必须设
  `FontDataOwnedByAtlas = false`，否则销毁图集时会 free 掉系统内存。

Vulkan 后端（`bklv/backends/vulkan_backend.cpp`）有几条不好从代码一眼看出的约定：

- `appInfo.apiVersion` 与 `ImGui_ImplVulkan_InitInfo::ApiVersion` **不要写
  `VK_API_VERSION_1_0`**：实例请求的版本决定 loader 能返回哪些 core 函数名，
  1.0 时它只给 `vkCmdBeginRenderingKHR` 这类后缀名，ImGui 探 core 名探不到会直接断言。
- 动态渲染按 API 版本二选一启用：>= 1.3 走 core
  （`VkPhysicalDeviceVulkan13Features::dynamicRendering`），1.1/1.2 走扩展
  （把 `VK_KHR_dynamic_rendering` 加进设备扩展列表）。
- 同步信号量按"谁保证它能被复用"分组：等待图像可用的按 frame-in-flight，
  交给 present 的那个必须按**交换链图像**（帧栅栏证明不了呈现引擎已释放它）。
- 调试用 `BKLV_VULKAN_VALIDATION=1`；MSYS2 的校验层装在 `mingw64\bin`，
  不在 `share/vulkan/explicit_layer.d`，所以要同时设 `VK_LAYER_PATH`。
- 交换链像素格式必须选 **UNORM**，不能选 `_SRGB`。ImGui 给出的颜色分量就是
  最终要显示的 8 位值（相当于已做过 sRGB 编码），目标若是 `_SRGB` 格式硬件会再
  编码一次：64 -> 137、128 -> 188，深色背景 `(26,28,33)` 被抬到 `(89,92,100)`，
  整个界面发灰。OpenGL 后端的默认帧缓冲是 UNORM 且没开 `GL_FRAMEBUFFER_SRGB`，
  选 UNORM 才和它一致。改完可以用纯色块（`IM_COL32(64,64,64,255)` 等）实测比对。

## 依赖

第三方都走 git submodule，构建脚本自己编译，不依赖系统安装：

| 子模块 | 用途 |
|---|---|
| `bklv/3rd/imgui` | GUI 与绘制（Windows 的 GL 运行时加载也复用它自带的 imgl3w） |
| `bklv/3rd/yoga` | Flexbox 布局 |
| `bklv/3rd/tweeny` | 补间动画（库本身不用，保留给后续的属性动画） |
| `bklv/3rd/SDL` | 桌面的窗口/输入/GL 上下文（源码编译成静态库） |
| `bklv/3rd/stb` | Linux/Windows/Switch 的图片解码 |
| `bklv/3rd/json` | nlohmann/json：JSON 视图描述与 i18n 文案（单头文件，全 inline，无需编源文件） |

系统只要求：`cmake`（编 SDL3）、以及平台本身的工具链。
**不要引入需要手动安装的开发库**，除非在 README 里写清楚安装命令。
Windows 上只有 Vulkan 构建需要额外装包（`mingw-w64-x86_64-vulkan-headers` /
`-vulkan-loader`），已在 README 写明命令。

## 代码

- C++17；`bklv/3rd/yoga` 单独用 C++20 编译
- `-Wall -Wextra -Wpedantic` 必须零警告
- Switch 构建带 `-fno-rtti`（**不关异常**），所以**不要用 `dynamic_cast` / RTTI**，
  类型判断用虚函数（见 `View::asBox()`）。
  异常保留的原因：`std::filesystem` 出错只能 terminate，JSON 解析这类"输入不可信"的代码
  需要可控失败路径。代价已实测：NRO +24576 字节。
  约定：**库公开 API 不向外抛异常**，失败用返回值 + 日志；内部可用 try/catch，
  但跨越「创建视图树」这类中途可能失败的操作必须 RAII（用 `std::unique_ptr<View>` 持有，
  交给父节点后再 release），否则抛出时会泄漏半棵树。
- **文案（i18n）**：资源 `resources/i18n/<locale>.json`，key 用 `/` 分层，取值走
  `bklv::getStr("a/b")`（带 `{}` 占位符则 `getStr("a/b", 12)`）。
  三条约定：① 文案在控件构造时解析成字符串，**切语言不会自动重译已有视图**；
  ② 语言名经 `canonicalLocale()` 归一（桌面 `zh-CN` 与 Switch `zh-Hans` 都归到 `zh-Hans`），
  资源名只用规范名；③ **不要扫目录**列语言文件——romfs 上的目录迭代真机未验证，
  要拆文件就在主文件里用 `"$files"` 清单显式列出。
  装载点是 `App::App()`（早于视图树构建），探测语言不依赖 `Platform` 实例。
- **JSON 视图描述**（`core/json_view.*` + `core/attributes.*`）：JSON 用 `"type"` 指定
  类型、其余键是属性、`"children"` 是子节点、`"include"` 引别的文件。
  三条必须记住的：
  ① **两段式**：`ViewTemplate`（可缓存、与语言无关）→ `instantiate()`（解析 `@i18n/` 等）。
     解析期就替换 `@i18n/` 会把缓存里的语言烤死，列表 cell 复用同一模板时必现。
  ② **路径要写出 `resources/` 这一层**（和图片一致）：`createFromJSONFile("resources/views/x.json")`；
     属性里的 `@res/x` 则是**相对 `resources/` 目录**的（与 borealis 的 BRLS_RESOURCES 一致）。
  ③ **属性表按类静态**：新控件要支持 JSON 就重写 `attributes()` 返回自己的静态表，
     handler 写成不捕获的 lambda 或普通函数（会转成函数指针）；公共属性在 `view.cpp` 里。
  结构错误返回 `nullptr` + 日志，未知属性只警告（带拼错提示）。上限：深度 32 / 节点 4096 / include 栈 32。
- **单行 Label 的截断判断留了 0.5px 容差**（`views/label.cpp`）：文本宽度由测量函数算、
  可用宽度由 Yoga 算，父容器又常把子节点定成"恰好等于文字宽度"，不留容差时一点浮点误差
  就会把够短的文字截成一个省略号（tab 标题踩过：中文全变 `…`，ASCII 反而正常）。
- 不生成 `imgui.ini`：绘制全在 viewport 绘制列表上，`io.IniFilename` 置空
- 日志用 `BKLV_LOG_INFO(...)` 等宏（带 文件:行号）。日志同时进控制台与
  `Logger` 的环形缓冲，界面上的日志窗口（`views/debug_window.hpp`）读缓冲渲染。
  **不要**为每条日志创建 View —— borealis 那样做会不断触发 Yoga 重排
- ImGui 窗口在 `App::onDrawWindows` 里下发（视图树画完之后、`ImGui::Render()` 之前）
- 鼠标点击默认**只聚焦不执行**动作。想点一下就执行，在控件构造函数里设
  `onPointerAction = true`（按钮、tab 都这么做）；列表项保持默认，避免误触发

## 测试

无窗口测试在 `tests/`（只断言数据层面行为，不开窗口）：

```bash
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl -DBKLV_BUILD_TESTS=ON
cmake --build build/gl -j 8
./dist/i18n_test          # 或 cd build/gl && ctest
```

产物与 demo 同目录（`dist/`）——资源路径按可执行文件目录解析（见
`bklv/core/resource.hpp`），测试要能读到 `resources/`。
加测试只需在 `tests/` 放一个 `*_test.cpp`，会被 CMake 自动收集。

## 目录

```
bklv/               库本体，自包含：代码 + 3rd/ + 自己的 CMakeLists.txt
  core/             接口层：view / box / app / platform / graphics / input / ...
  views/            控件：label / image / scrolling_frame / recycler / ...
  backends/         图形 API 实现：opengl / vulkan / gles3 / imgui 后端
  platforms/        平台实现：sdl3 / switch
  3rd/              vendored 依赖：imgui / yoga / tweeny / SDL / stb
  CMakeLists.txt    编出 bklv::bklv（INTERFACE 目标，见上）
demo/               展示程序：main.cpp + app/（页面 / tab / 列表项）
CMakeLists.txt      仓库入口：判定平台、定输出目录、编 demo
scripts/            构建脚本（只剩 Switch 的）
docs/               速查文档：bklv 文件职责、API 用法、已知的坑
notes/              设计笔记与移植评估
resources/          demo 用的图片等素材（桌面复制到可执行文件目录，Switch 打进 romfs）
```

`bklv/` 要能被整个目录拷到别的项目里用，所以**它自己的构建逻辑必须留在
`bklv/CMakeLists.txt`**，不要往根 `CMakeLists.txt` 里加库相关的东西。

README 只讲项目介绍、拉取编译、怎么用到别的项目里；**代码层面的说明（文件职责表、
API 速查、踩坑记录）写在 `docs/`**，别往 README 里堆。

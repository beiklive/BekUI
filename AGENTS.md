# AGENTS.md — 本项目的开发约定

## 图形后端：演示程序一律双版本

**每个演示程序都要能编出 GL 和 Vulkan 两个版本**，不是二选一。

原因：GL 和 Vulkan 是两条完全不同的渲染路径（上下文创建、shader、资源生命周期都不同），
只在一个后端上验证过的界面代码不算验证过。双版本让「上层与图形 API 无关」这条设计
随时可被检验——如果某个演示换后端跑不起来，说明它漏了抽象。

> 现状：`demo/` 已按这条重做成最小框架，GL 与 VK 两版都编（`dist/demo`、`dist/vk/demo`）；
> `examples/` 下的宿主示例自带 SDL3 + GL 循环，所以在 VK 构建下不编
> （`if(BKLV_BUILD_EXAMPLES AND BKLV_GRAPHICS STREQUAL "gl")`）。

### 构建

构建入口只有一个：根目录的 `CMakeLists.txt`。后端是编译期决定的，所以
「每个 demo 都能编 GL 和 Vulkan 两版」靠**两个构建目录**实现。
仓库根有 `CMakePresets.json`，CLI 与 IDE 都用它（**不要再手写 `-B build`**）：

```bash
# GL
cmake --preset gl && cmake --build --preset gl      # 等价 -B build/gl -DBKLV_GRAPHICS=gl

# Vulkan
cmake --preset vk && cmake --build --preset vk      # 等价 -B build/vk -DBKLV_GRAPHICS=vk

# 不想用 preset 时手写也行（效果一样）
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl
cmake -S . -B build/vk -DBKLV_GRAPHICS=vk && cmake --build build/vk

# 只编某个目标（目标名就是 add_executable 的名字；现在是 embed_imgui_gl / multi_context）
cmake --build build/gl --target embed_imgui_gl
```

Windows（MSYS2 / MinGW-w64）用根目录的 `build.bat`，它是同一批参数：
　　
```bat
build.bat                       :: GL
build.bat --vk                  :: Vulkan
build.bat --only=embed_imgui_gl :: 只编指定目标（a,b 可多个）
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

### 加新示例时

给 `demo/` 加页面不需要新目标：把新 .cpp 加进根 `CMakeLists.txt` 的
`add_executable(demo ...)` 那一行（`demo/` 没有自己的 CMakeLists）。
加一个**新的可执行程序**时，在根 `CMakeLists.txt` 末尾的 examples 段里加一个
`add_executable`，链接 `bklv::bklv`，
再用 `bklv_set_output_dir(<目标>)` 把产物钉到 `BKLV_OUT_DIR`（不然 VS 多配置生成器会
落到 `dist/Release/`，资源就找不到了），最后 `bklv_copy_resources(<目标>)`。
SDL3 / OpenGL / Vulkan / ImGui 都由 `bklv::bklv` 带出来，不要在目标上重复列。

```cmake
add_executable(my_demo examples/my_demo/main.cpp)
target_link_libraries(my_demo PRIVATE bklv::bklv)
bklv_set_output_dir(my_demo)
bklv_copy_resources(my_demo)
```

示例里**不要写任何依赖具体图形 API 的代码**；需要图形 API 能力时走 `RenderBackend` 接口。
要在 Switch 上跑就再加 `examples/switch/` 那种最小宿主，并让 `scripts/build_switch.sh`
的 `HOST_DIR` 指过去（它只编一个宿主，不认多个）。

### 改构建时注意

- **`<仓库>/build` 本身不是构建目录**：我们的构建目录是 `build/gl` / `build/vk`，
  `build/` 只是父目录。VS Code 的 CMake Tools 默认去构建 `build/`，会报
  `not a CMake build directory (missing CMakeCache.txt)` —— 仓库里的
  `.vscode/settings.json` 已经把它指向 `gl` preset（`cmake.useCMakePresets: always`），
  手动选的话是 `F1` → `CMake: Select Configure Preset` → `gl`。
- **preset 必须自带生成器**：`CMakePresets.json` 的四个 preset 都写了 `generator`
  （`gl`/`vk` 是 Visual Studio 17 2022，`*-mingw` 是 Ninja + 写死 gcc/g++ 路径）。
  不写的话 CMake Tools 会按当前 kit 传 `-G`，kit 是 Ninja 而 `build/gl` 里是 VS 缓存时直接报
  `generator : Ninja does not match the generator used previously: Visual Studio 17 2022`。
  改 preset 时别把 `generator` 删掉。
- **VS 的多配置生成器只保留 Release**（preset 里 `CMAKE_CONFIGURATION_TYPES=Release`）：
  Debug 与 Release 都钉在同一个 `dist/`，先编 Debug 再编 Release 时链接器认为 exe 比 obj 新、
  跳过链接，跑起来是那个 14.8 MB 的 Debug 版。要 Debug 就另开 preset 并把输出目录也换掉。
- **多个构建目录共用同一个 `dist/`**：CMake 的输出目录默认是 `${CMAKE_SOURCE_DIR}/dist`
  （VK 是 `dist/vk/`，MinGW preset 用 `-DBKLV_OUT_DIR` 指到 `dist-mingw/`），所以任何额外
  构建目录（`build/asan`、`build/dbg`、`/tmp/xxx`）都会把可执行文件写进同一个 `dist/`。
  它们的产物时间戳比 `build/gl` 的目标文件新，make 会认为"已是最新"而**不重新链接** ——
  于是你以为在验收 A 配置，实际跑的是 B 的二进制（这次踩到：用 `-DBKLV_WITH_WEBP=OFF`
  在 /tmp 配了一次，`dist/` 里就变成"没有 WebP"的版本）。
  跑完额外构建目录后要 `rm -f dist/embed_imgui_gl && cmake --build build/gl -j 8` 强制重链。
- 清单漏项在 configure 阶段就会报错（`bklv/CMakeLists.txt` 按目录查 core/views/widgets），
  不用等链接期。
- 加了新的 `.cpp` 要加进对应的源文件列表：**库**的加 `bklv/CMakeLists.txt`
  （`BKLV_CORE_SOURCES` / `BKLV_IMGUI_SOURCES`），**示例**的加根 `CMakeLists.txt`
  的 examples 段（那个 `add_executable`）。漏一个会以 `undefined reference` 的形式在链接期才暴露。
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

见 `notes/adding-render-backend.md`。选择点在 `bklv/system/platform/*.cpp` 的
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
两条容易断的经验（都踩过）：
① **SVG 在 Switch 上默认关**（脚本 `WITH_SVG=0`）：lunasvg 的矢量后端 plutovg 用了
   `sys/mman.h`，devkitA64 没有这个头，一开就编译失败。桌面仍默认开，Switch 要用得显式 `--svg`。
② 改了脚本里的 `DEFINES`（比如开关可选解码器）**必须 `--clean` 重编**：脚本按时间戳增量
   编译，不会因为宏变了重编旧目标文件，会以"undefined reference to lunasvg::…"的形式在
   链接期暴露（那些 .o 是用 `BKLV_USE_SVG` 编的）。
产物大小（2026-10 实测，移除测试、设计器与 demo、两套主题之后）：`bklv_switch.nro`
**9.46 MB** = 代码+数据 8.66 MB（strip 后 ELF）+ romfs 0.86 MB（63 个资源文件：
字体 524 KB / 文案 191 KB / 主题 57 KB / 音效 19 KB / 两张示例图 46 KB）。

桌面（macOS / Linux / Windows）上不再有 shell 构建脚本，`build.sh` 与
`scripts/build_sdl3.sh` 已删除；SDL3 作为 `add_subdirectory` 的子项目随 CMake 一起编。

Windows 上有几条平台约定，改相关代码时要留意：

- **在 MSYS2 里要用「MINGW64」终端**：默认的 MSYS 终端 `MINGW_PREFIX` 为空，
  CMake 找不到编译器。`build.bat` 会自己把工具链放进 `PATH`，不受此影响。
- CMake 里的平台判定不能只看 `WIN32`：在 MSYS 终端里 `CMAKE_SYSTEM_NAME` 是 `MSYS`
  而 `WIN32` 为空。需要跨平台分支时用 `WIN32`，需要区分 MSYS 用 `CMAKE_SYSTEM_NAME`。
- `opengl32.dll` 只导出 GL 1.1，GL 3.2 Core 的函数必须在 `loadGLFunctions()`
  里运行时取地址（`bklv/graphics/render/gl/gl_loader.cpp`，复用 ImGui 自带的 imgl3w）。
  **任何在 GL 上下文就绪后直接调 `gl*` 的地方，都要先确保它被调用过。**
- 不要在 `REM` 行里写右括号：cmd 会把它当成语句块结束，让后面的命令被吞掉。
- JSON 用 `bklv/3rd/json`（nlohmann/json **v3.11.3**）。**不要降到 v3.11.2**：
  那版在 Clang 下会对 `operator "" _json` 报 `-Wdeprecated-literal-operator`，
  4 处警告都在库头里，直接破坏零警告约定（v3.11.3 已修）。
- **资源文件按"资源目录"解析，不是工作目录**（`bklv/foundation/core/resource.hpp`）：
  桌面由 CMake 的 POST_BUILD 把 `resources/` 复制到可执行文件目录；
  Switch 用 `build_romfs` + `elf2nro --romfs` 打进 nro，运行时 `romfsInit()` 挂成 `romfs:/`。
  代码里统一写 `"resources/xxx"`，`Image::setImageFromFile()` 会自己解析。
  **不要用 elf2nro 的 `--romfsdir`**：这个工具链里的 elf2nro 会静默忽略它
  （参数能解析，但同时给 `--romfs` 与 `--romfsdir` 会报冲突；而只用 `--romfsdir`
  时产物与不带参数逐字节相同，NRO 里既无 ASET 段也无 IVFC）。必须两步：
  `build_romfs <dir> <img>` 再 `elf2nro ... --romfs=<img>`。
- 中文字体各平台拿法不同（`bklv/system/media/font.cpp`），别只写 macOS 的系统字体路径：
  桌面读系统字体文件；**Switch 上没有可读的系统字体目录**，要用 `pl` 系统服务
  （`plInitialize(PlServiceType_User)` + `plGetSharedFontByType`）拿内置共享字体，
  再把 Standard / 简中扩展 / 繁中 / 韩文合并成 fallback 链。
  注意 `AddFontFromMemoryTTF` 默认接管内存所有权，共享内存必须设
  `FontDataOwnedByAtlas = false`，否则销毁图集时会 free 掉系统内存。

Vulkan 后端（`bklv/graphics/render/vk/vulkan_backend.cpp`）有几条不好从代码一眼看出的约定：

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
| `bklv/3rd/stb` | 位图解码（PNG/JPEG/BMP）+ GIF（含动画） |
| `bklv/3rd/webp` | WebP 解码（静态 + 动图），可选：`BKLV_WITH_WEBP` |
| `bklv/3rd/lunasvg` + `bklv/3rd/plutovg` | SVG 栅格化，可选：`BKLV_WITH_SVG` |
| `bklv/3rd/json` | nlohmann/json：JSON 视图描述与 i18n 文案（单头文件，全 inline，无需编源文件） |

系统只要求：`cmake`（编 SDL3）、以及平台本身的工具链。
**不要引入需要手动安装的开发库**，除非在 README 里写清楚安装命令。
Windows 上只有 Vulkan 构建需要额外装包（`mingw-w64-x86_64-vulkan-headers` /
`-vulkan-loader`），已在 README 写明命令。

## 代码

> **2026-10 控件大清理**：`bklv/widgets` 现在只剩基础件 —— `appearance_view` /
> `appearance_children` / `button` / `notification` / 六个 `layer_draw_*`。
> 下面提到 `ui/widgets/shell.*`、`ui/widgets/pager.*`、`ui/widgets/header.*`、`ui/widgets/bottom_bar.*`、
> `ui/widgets/sidebar.*`、`ui/widgets/dropdown.*`、`ui/widgets/selector_button.*`、
> `ui/widgets/edit_text_dialog.*`、`ui/widgets/cell.*`、`ui/widgets/tab_frame.*`、`ui/widgets/slider.*`、
> `ui/widgets/toast.*`、`ui/widgets/hints_bar.*`、`ui/widgets/status_area.*`
> 的段落，都是对**已移除控件**的历史约定（连同它们在 `resources/theme/default/base/**`
> 下的外观一起删了）。重做这些控件时按这些约束实现，别再假设文件还在。

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
  `bklv::translate("a/b")`（带 `{}` 占位符则 `translate("a/b", 12)`）。
  三条约定：① 文案在控件构造时解析成字符串，**切语言不会自动重译已有视图**；
  ② 语言名经 `canonicalLocale()` 归一（桌面 `zh-CN` 与 Switch `zh-Hans` 都归到 `zh-Hans`），
  资源名只用规范名；③ **不要扫目录**列语言文件——romfs 上的目录迭代真机未验证，
  要拆文件就在主文件里用 `"$files"` 清单显式列出。
  装载点是 `App::App()`（早于视图树构建），探测语言不依赖 `Platform` 实例。
- **JSON 视图描述**（`foundation/core/json_view.*` + `foundation/core/attributes.*`）：JSON 用 `"type"` 指定
  类型、其余键是属性、`"children"` 是子节点、`"include"` 引别的文件。
  三条必须记住的：
  ① **两段式**：`ViewTemplate`（可缓存、与语言无关）→ `instantiate()`（解析 `@i18n/` 等）。
     解析期就替换 `@i18n/` 会把缓存里的语言烤死，列表 cell 复用同一模板时必现。
  ② **路径要写出 `resources/` 这一层**（和图片一致）：`createFromJSONFile("resources/views/x.json")`；
     属性里的 `@res/x` 则是**相对 `resources/` 目录**的（与 borealis 的 BRLS_RESOURCES 一致）。
  ③ **属性表按类静态**：新控件要支持 JSON 就重写 `attributes()` 返回自己的静态表，
     handler 写成不捕获的 lambda 或普通函数（会转成函数指针）；公共属性在 `view.cpp` 里。
  结构错误返回 `nullptr` + 日志，未知属性只警告（带拼错提示）。上限：深度 32 / 节点 4096 / include 栈 32。
- **Label 的宽度比较都留了 0.5px 容差**（`ui/widgets/basic/label.cpp`）：文本宽度由测量函数算、
  可用宽度由 Yoga 算，父容器又常把子节点定成"恰好等于文字宽度"。不留容差时：
  ① 单行截断会把够短的文字截成一个省略号（tab 标题踩过：中文全变 `…`，ASCII 反而正常）；
  ② 多行换行会把"两个字一行"的标题拆成两行（侧栏标题"导航"踩过）。
- **基础色板与原始色值都在 `color.common.*`**（每套主题 `colors.json` 的**第一组**，语义组跟在后面）：
  ① 色板：`red` / `orange` / `yellow` / `green` / `cyan` / `blue` / `purple` / `pink` / `brown` / `grey`，
  **每个都有 `XxxLight` / `XxxDark` 两档**（Material 三档，如 `common.blue` = #2196f3）；
  ② 原色：`black` / `white` / `transparent` / `overlay`（模态遮罩）/ `divider`（1px 半透明线）/
  `shadow`（阴影基准色）/ `focusRing` / `textMuted`。
  写"透明"就用 `@theme/color.common.transparent`，别再散 `"#00000000"`（曾经在 60 处重复）。
  注意 `colors.json` **不支持 key 之间互相引用**（`@theme/` 只在视图/外观 JSON 里解析），
  所以语义键（`button.primaryBg` 这类）仍是字面量；想改整体配色就一起改。
- **JSON 变量块：`@` 跨文件、`&` 同文件**（`compile/json_vars.*`）：
  一份文件在顶层写**一个** `vars`（只有 `colors` / `numbers` 两个子块），正文用 `&color.x` /
  `&number.y` 引用。**`$` 前缀只用于键**（`$comment` / `$files` 这类元数据，i18n 装载器按
  "跳过 `$` 开头的键"处理）；值位置写 `$color.x` 会**报错并提示改用 `&`**，
  以 `&` 开头但不合法的值同样报错（不静默当普通字符串）。五条：① 只有一个、必须在顶层（嵌套 → 报错）；② 值可以是字面量或 `@theme/...`；
  ③ `vars` 之间可链式引用（≤4 层，循环 → 报错）；④ `&` 的作用域是**整份文档**——`box` /
  `layers` / `states` / `transitions` / `children` 里都能直接用，别在子节点里再写一遍 vars；
  ⑤ 展开与"直接写那个值"**完全等价**（缩放/报错口径都交给 `resolveColor` / `resolveNumber`）。
  **`&` 绑"写它的那份文件"**：`import` / `include` 进来的文件按它自己的 vars 展开后再合并，
  想让调用方调被引入方的值就直接覆盖那个字段。**不做算式**（`"-&number.gap"`、`"&x * 2"` 都不行，
  负数写进变量本身）、不做结构复用（那是 `import` 的活）、不做字符串/枚举变量。
  外观与视图描述两条解析路径都接了（`Appearance::compile` / `ViewTemplate::fromString`），
  两边共用同一套展开实现，"用 vars"与"直接写字面量"编译结果完全一致。
- **JSON 错误策略**（`compile/json_policy.*`）：解析失败后按 `JsonErrorPolicy` 选恢复动作 ——
  `STRICT`（默认，整份失败）/ `SKIP_FIELD`（字段级错误跳过该字段、保留默认值）/
  `FALLBACK_DEFAULT`（文件级错误整份退到 default 主题的同名外观，`@theme` 仍按当前主题解析）。
  三条边界：① 能跳过的只有**字段级**（颜色/数值/`@theme` key/属性类型）；文件级（语法、缺
  `layers`、`import`/`vars` 循环、渐变结构校验）只能整份回退；② **不管哪种策略都要留诊断**
  （`getJsonDiagnostics()`：file/path/message/recovered），`skip` 不是"没发生"；
  ③ 枚举拼错仍是"警告 + 保持原值"，不受策略影响（改成致命会破坏第三方主题）。
  环境变量 `BKLV_JSON_POLICY=strict|skip|fallback`，显式 API 优先。
  同一套策略还兜住两处以前**静默忽略**的写法：外观**顶层键白名单**
  （`vars`/`import`/`box`/`layers`/`states`/`transitions`/`children`，`$` 开头放过；拼错 `state`
  会报"是否想写 states"）与 **`theme.json` 的 `files[]` 少了 `.json`**（打 error + 诊断，
  该项回退 default —— 以前只一条 warning，整份外观静默变空）。
- **主题引用的写法是 `@theme/color.a.b` / `@theme/metric.a.b`**（点是分层分隔符，
  对应 `colors.json` 的 `a.b`）。写成 `@theme/color/a.b` 会被容错成点分
  （带一条警告提示正确写法），
  **但真正不存在的 key 仍然编译失败**（不静默回退成洋红）—— 改主题后要逐个编译
  两套主题的全部外观兜底。
- **布局是延迟的，一帧最多算一次**（`ui/view/view.cpp` 的 `invalidate()` + `ui/app/app.cpp`）：
  `invalidate()` 只冒泡到根标记"要重算"，真正那一次由 `App::frame` 做。三个触发条件：
  画布尺寸变了 / 首帧 / 有人 invalidate 过。
  这样静态页面每秒 0 次布局（以前 60 次），回收列表 1 次/帧（以前十几个 cell
  各触发一次 = 10 次/帧）。
  几条相关约定：
  ① 视图自己的 setter 已经 invalidate 了，不用手动管；改的是"影响布局的全局状态"
     （比如换主题、UI 缩放变了）才需要 `App::requestLayout()`。
  ② **不要在同一帧里"改完立刻读布局尺寸"**——延迟一帧生效。回收器不受影响，
     因为 cell 的位置是用 `setTranslation` 摆的（见 `RecyclerFrame::layoutCell`），
     不依赖本帧的 Yoga 结果。
  ③ 漏置脏会让界面不刷新。排查用 `BKLV_FORCE_RELAYOUT=1` 退回"每帧全量重算"，
     现象消失就说明有地方改了布局状态却没 invalidate。
  ④ `App::getLayoutCount()` 是累计布局次数，调试窗口的状态行里能看到：
     界面静止时这个数应该停住。
- **UI 尺寸按 720p 写**（`foundation/core/ui_scale.*`）：主题里所有尺寸/字号都按 **1280x720** 写
  （和 borealis 的 `ORIGINAL_WINDOW_WIDTH/HEIGHT` 一致），运行时 `uiScale = 画布宽 / 1280`。
  缩放点在**取值**这一层：
  ① `ThemeData::number()` 里 `metric.*` 会乘缩放（`metric.anim.*` 是毫秒、
     `metric.font.lineHeight` 是系数，都不缩放）；
  ② 外观 JSON 里的 radius/strokeWidth/width/height/fontSize/inset/offset 会乘缩放
     （opacity 是系数、duration 是毫秒，不缩放）；
  ③ **视图描述 JSON 的像素量也乘**（`AttributeDef::pixels` 标记，在 `applyFromTable` 里统一乘）：
     width/height/min*/margin*/top/left/right/bottom/borderThickness/cornerRadius/fontSize/
     图片的 offsetX/offsetY 等。**不加这一位就不缩放**（百分比、auto、grow/shrink/alpha、
     图片显示比例都是这样）。三处必须用同一个基准，否则换分辨率就错位。
  代码里写死的像素值**不缩放**——想跟随比例就用主题 metric 或把数字写进外观 JSON。
  画布尺寸变了要重新套用外观（外观值在 `applyTheme()` 时就取定了）并重排，
  这个由 `App::frame` 里调 `updateUiScaleFromCanvas()` 统一处理。
- **外观层序：底必须在内容层之前**（`bg` / `border` 排在 `text`/`image`/`custom` 之前，
  `shadow` 解析期提到最前）。排反了的话，聚焦时那块**不透明的底会把内容盖住**
  —— 开关/单选的图形是 `custom` 层，曾因此"聚焦后开关消失"（14 个 Cell 外观全踩了）。
- **按钮有 5 种 style**（对标 borealis 的 `BUTTONSTYLE_*`）：`button`（=DEFAULT 灰底白字）、
  `button.primary`（强调色底 + 深色字）、`button.highlight`（灰底 + 强调色字）、
  `button.bordered`（**只有边框没有底**）、`button.borderless`（无底无框）；
  另有我们自己的 `button.ghost`（更淡的一种）。
  注意 borealis 的 `hideHighlightBackground`：**primary/highlight/default 聚焦时不出高亮底**，
  只有 bordered/borderless 出 —— 在 JSON 里就是 focused 状态要不要写 `hlBg.fill`。
- **Cell 家族要照 borealis 的实际行为**（`library/lib/views/cells/*.cpp`），别凭感觉画：
  BooleanCell 是**文字 开/关** + 切换时字号弹一下（1→0.8→1，各 100ms；开用
  `color.text.value`、关用 `color.text.disabled`），**没有图形开关**——想要的图形开关是我们
  额外的 `BooleanCellStyle::SWITCH`；RadioCell 是**填充圆 + 对勾、只选中时显示**、选中时标题用
  value 色；SelectorCell 右侧是**当前值的纯文本**、点击**弹 Dropdown**（无 App 退化成切下一项）；
  DetailCell 只有两个 Label、**没有箭头**。几何按 `style.cpp`：行高 70、字号 22、
  左右内缩 16、上下 12.5、对勾半径 15。
- **通用层都在 `base/layer/` 下**，外观按需 `import` 再覆盖字段：`bg`（圆角底）、`border`
  （透明底 + 描边，叠在 bg 之上）、`shadow`（阴影）、`glass`（半透明底 + 内缘高光；扁平
  主题里就是半透明 rect）。**没有 `hl_bg` 这层了** —— 聚焦反馈改由浮层的焦点框负责
  （见上面那条"焦点框画在浮层上"）：控件外观的 `focused` 状态按需改 `border.stroke`
  （= `color.highlight.border`）或 `bg.fill`（= `color.highlight.background`）；
  默认灰底按钮聚焦不出高亮底（对标 borealis 的 `hideHighlightBackground`）。
- **显式设置优先于主题/外观**（`setCornerRadius(r, explicit=true)`）：
  代码里显式设过的值，外观 JSON 不能覆盖它；外观套用一律走 `explicit=false`。
  Label 的 `setFontSizeExplicit` / `setTextColorExplicit` 是同一套约定的老成员。
  踩过的坑：外观 JSON 里写了 `radius`，代码里 `setCornerRadius(60)` 就完全不起作用
  （想画圆形头像，画出来是圆角方块）。
- **`update()` 路径不许读 `ImGui::GetIO()`**：无窗口（测试）时没有 ImGui 上下文会直接空指针。
  时间统一用 `foundation/anim/time.hpp` 的 `getFrameDeltaSeconds()`；确实需要鼠标状态的地方
  用 `ImGui::GetCurrentContext() != nullptr` 判一下再取（`ScrollingFrame`/`HScrollingFrame` 就是这么做的）。
  这个坑在 `Animatable`、`ScrollingFrame`、`ProgressSpinner` 上先后踩过三次。
- **回收列表可以分段**（`views/recycler.*`）：单段只重写 `getItemCount()`；
  分段重写 `getSectionCount()` + `getItemCountInSection()` / `getSectionTitle()` /
  `getItemIdentifierInSection()`。**段标题返回空串表示这一段不占表头行**。
  接口刻意不叫 `getItemCount(section)`（borealis 的重载写法）——派生类只重写一个
  会把另一个名字隐藏掉，属于典型 C++ 坑。
- **Recycler 可由 JSON 描述**（`"type": "Recycler"`，桥接层 `ui/widgets/recycler_view.*`）：
  **结构归 JSON、数据与行为归 C++** —— JSON 只写容器、**一份** item 模板、列规则、
  以及"名字"：`data`（数据源名）/`itemBinder`（绑定函数名）/`onEnd`（增量加载挂点名）。
  数据源走 `bklv::data::registerSource("名字", 工厂)`（纯数据层，不认识 JSON/ImGui），
  绑定函数走 `bklv::binding::registerItemBinder("名字", 函数)`（形状 `(View&, size_t 行号)`）。
  **行为写进绑定函数，不要 `static_cast` 回某个 cell 具体类**（那会在 JSON 建的 item 上是 UB）。
  属性表/模板实例化都在 widgets 桥接层；`views/recycler.*` 只管纯数据源接口与布局能力
  （`setColumns` / `visualTopOf`）。JSON 属性清单与踩过的坑见 `docs/json-syntax.md` 的
  「Recycler」一节；增量追加/预加载/缓存销毁的设计见 `docs/list-incremental-loading-design.md`。
  段头 cell 继承 `RecyclerSectionHeader`，标题通过虚函数 `onSectionTitle` 下发
  （Switch 带 `-fno-rtti`，不许用 `dynamic_cast`）。
  **C++ 侧的 cell 也必须重写 `setSectionIndex(section, index)`**：回收器的分段路径走它，
  只重写 `setIndex(index)` 的话绑定**永远不会被调用**（现象是列表项只有构造时的空控件）。
  这条已经在两个地方各踩过一次（JSON 建的 `RecyclerItemCell`、demo 手写的 `ListCell`/`DexCell`），
  所以两个入口都实现、共用一个 `applyIndex` 是标准写法。
  另外：recycler 的 cell 是**绝对定位**的（`layoutCell` 手动摆位置、跳过 Yoga），
  给它塞子节点时尺寸/内缩要用显式值或 `setPadding`，别指望 `setMargin` 生效。
  增量加载的旋钮（`RecyclerLoadConfig` → JSON 键 `pageSize` / `loadThreshold` /
  `maxPerIdentifier` / `estimatedItemHeight` / `preFetchLine`）都有安全默认值，
  **不写也能跑**；数值一律夹到 >= 0。
- **枚举值大小写与 `_`/`-` 都不敏感**（`enumMatches`，两套 JSON 一致）：
  `flexEnd` == `flex_end` == `FLEX_END`。视图描述 JSON 里拼错 → 不建控件 + 报错（列出可选值）；
  外观 JSON 里拼错 → 警告 + **保持原值不动**（`justifyFromName` 这类返回 bool，别退回默认值）。
  加新枚举字段时用 `enumMatches` + `warnUnknownEnumValue`，别写 `== "flexEnd"` 这种精确比较。
- **JSON 语法细节查 `docs/json-syntax.md`**（四类 JSON 的完整键表 / 取值 / 枚举拼写 / 报错行为）；
  下面几条是**约定**，别改错了。
- **控件的样式全在 JSON 里**（`resources/theme/<主题>/base/<控件>[.<变体>].json`）：
  一份外观由三段组成——
  ① `box`：盒模型/布局（width/height/minWidth/minHeight/padding/margin/gap/grow/shrink/
     justifyContent/alignItems/backgroundColor/cornerRadius/borderThickness/borderColor）。
     长度写数字（按 720p 缩放）、`"auto"`、`"50%"` 或 `@theme/metric.a.b`；
     另有 `axis`（row/column）——AppearanceView 默认是 column，页头/底栏这类横向的要写明；
     `padding`/`margin` 可以写一个值（四边同值）或 `[上,右,下,左]`。
     只有写了的字段才会被套用，所以空 box 不会冲掉控件的默认值。
  ② `layers`：绘制结构，op 有 `rect` / `text` / `image` / **`shadow`** / `custom`。
  `shadow` 层是柔化投影（`fill` 浓度、`feather` 羽化、`radius` 圆角、`offset` 位移、`spread` 外扩、
  **`cutout` 挖空开关**），
  **必须写在自己的底色之前**（排反了阴影会被底色压掉），绘制档位由全局开关
  `bklv::shadow::setQuality(FAST/SMOOTH)` 或 `BKLV_SHADOW_QUALITY=smooth` 决定。
  ③ `states`：状态动效。
  **控件在代码里只写行为**（有哪些子节点、数据从哪来、语义绘制）；
  尺寸/间距/颜色/圆角一律从 JSON 来。容器类控件（不是 AppearanceView 的）
  调 `applyWidgetBox(this, "<控件名>")` 也能吃到同一份 box 段。
  同一控件要多套尺寸就加变体文件（如 `header.applet.json`、`tab_cell.sidebar.json`），
  代码里用 `setAppearanceVariant("applet")` 切，而不是在代码里改高度。
- **主题只有一套：JSON 文件**（`foundation/core/theme_data.*`）。没有并存的 C++ 主题表了——
  视图 JSON 的 `@theme/`（取色，走 `color.*`）与 `@style/`（取数值，走 `metric.*`）
  和外观 JSON 的 `@theme/...` 解析的是同一份数据；前缀可省略
  （`@theme/accent.base` == `@theme/color.accent.base`），key 不存在会**报错**而不是回退成洋红。
- **主题与外观**（`foundation/core/theme_data.*` + `foundation/core/appearance.*` + `ui/widgets/`）：
  换肤 = 复制 `resources/theme/default/` 改名 + 改 JSON，**不改 C++**。
  四条约定：
  ① **不扫目录，清单提前写**：`themes.json` 列主题；各主题 `theme.json` 的 `files[]` 列
     本主题的全部文件。目录分层与名字规则：
     ```
     <主题>/theme.json        元信息（name/author/description）+ files[] 清单
     <主题>/tokens/           colors.json / metrics.json（色板与数值）
     <主题>/base/             基础控件外观（单个容器/元素）
     <主题>/base/widget/           二级控件外观（引用 base 组合出来的）
     <主题>/view/             页面与视图描述（结构 + 外观）
     ```
     **名字 = 相对主题根的路径去掉扩展名**（`base/button.json` → `"base/button"`），
     变体继续用后缀拼（`base/button` + variant `"primary"` → `base/button.primary`）。
     挪文件 = 改名字，C++ 里的引用（`AppearanceView("base/button")`、
     `applyWidgetBox(this,"base/widget/sidebar")`）要一起改。
     视图描述用 `createViewFromTheme("page_json")` / `loadViewTemplateFromTheme(...)` 装载
     （主题内 `ui/view/`，缺了回退 default 主题）。
  ② **装饰层走 JSON，语义绘制留 C++**：底/边框/文字/图片/状态动效由 `layers` 描述；
     Label 的换行、Image 的缩放、进度弧、滑块轨道走 `custom` 层
     （`AppearanceView::drawCustomLayer`）。把语义绘制也塞进 JSON 等于写解释器，别做。
  ③ **draw 里不查主题**：外观值在构造 / `applyTheme()` 时取到 `AppearanceInstance`，
     绘制只读当前值。换肤靠 `App::setTheme()` → 重新取外观 + `applyThemeRecursive()` + 重排。
  ④ **主题只覆盖未被显式设置过的值**：调过 `setFontSize/setTextColor` 的控件，
     换肤不再动它；要跟随主题就 `setXxxExplicit(false)`。
  ⑤ **装饰件用显式 `width`/`height`**：层默认铺满"控件范围按 inset 收缩后"的区域，
     给了 `width`（如色条 4）或 `height`（如下边线 1）就按 `align`/`valign` 摆放。
  ⑥ **`import` 让外观引用外观**（`"import": "base/layer/bg"` 或字符串数组）：
     逐字段合并、**当前文件优先**——对象递归合并，`layers` 按 `id` 合并（同 id 的层合并字段，
     新 id 追加），其它数组整体替换，标量覆盖。所以"改 base/layer/bg 的圆角，
     所有 import 它的外观一起变"，而外观自己写的字段仍然赢。
     循环引用、深度 > 8、父级不存在都会**报错**（不静默降级）。
  ⑦b **换外观时"没写的字段不会清掉旧值"**：`applyAppearanceBox` 只覆盖 JSON 里出现的字段，
     所以从 `base/button/button`（有 `minWidth: @theme/metric.control.minWidth`）切到
     图标形态（`base/button/button.icon`）之后，**旧的 minWidth=120 会留在节点上** ——
     图标按钮明明写了 `width: 48`，布局宽度却一直是 120（踩过：量了半天布局才追到这个 token）。
     要固定尺寸的组合外观必须**显式写 `minWidth: 0`**（`base/button/button.icon` 就是这么修的）。
  ⑦a **亚克力/液态玻璃用 `op: "glass"` 层声明**（`docs/glass-layer.md`）。
     玻璃层由解析期**提到最前**，所以做"玻璃变体"的标准写法是
     `import` 基外观 + 覆盖 `bg.fill` 为透明 + 追加一行 `{ "import": "base/layer/glass" }`；
     **不要另开演示页面** —— 做法是给每个控件追加一行玻璃版本。

     外观能覆盖的只有**样式**（`fill` 染色 / `opacity` / `radius` / inset 几何）；
     `blurRadius`、`quality` 与总开关只由 `Shell` 决定 —— 两条后端的"这一帧背景模糊"是整帧共用一份。
     两个清单坑：`theme.json` 的 `files[]` 条目要**带 `.json` 后缀**（`base/glass_panel.json`，
     少了后缀会以"主题里没有控件外观"的形式静默回退）；`box.padding` 只接受**一个数**或
     **四个数**（`[上,右,下,左]`），写两个数会编译失败。

     **主题分工（用户拍板，2026-10）**：只有两套主题 ——
     · `default`：**纯扁平风格**。不再有 `shadow` / `glass` 层，玻璃变体（`base/widget/*.glass.json`、
       `base/glass_*.json`）里的 `glass` 层已扁平化成实心/半透明 `rect`；
       `color.common.shadow` 改成全透明（代码驱动的软阴影 —— 滑块把手、侧栏 —— 随之关掉），
       渐变示例外观也从主题里撤出、改成页面内联（同 stripe / quad 的做法）。
     · `ios`：苹果风（系统色、分组列表、玻璃上下栏）。`op: "glass"` 现在是"ios 在用"的能力。
     原 `ocean` / `my-skin` 两套已删除，仓库里所有引用都改成了 `ios`。
  ⑦a2 **容器的玻璃统一走 `base/layer/glass`**（`docs/glass-layer.md` §2b）：
     容器外观 `"import": "base/layer/glass"`，自己的 `bg.fill` 改透明 —— 玻璃层的 `fill`
     既是染色、也是玻璃不可用时的降级底。**层序由解析期固定成 阴影(0) → 玻璃(1) → 其余(2)**：
     玻璃不能排到真正的第一个，否则半透明的阴影层会把玻璃压暗（踩过）。
     `default`（扁平）主题里这一层就是半透明 `rect`（染色照旧）。
     玻璃层的 **`stroke` / `strokeWidth` = 内缘高光**（沿内缘描一圈，半径跟控件走）——
     玻璃质感主要靠这一笔。
  ⑦d **视图生命周期动画写在外观 JSON 的 `transitions` 段**（`docs/json-syntax.md`）：
     四个事件 `enter` / `exit` / `show` / `hide`；**不做预设**（没有 `type: slideFade`），
     效果直接写通道 —— 视图级 `translateX/Y`、`scaleX/Y`、`alpha`（整树一起动，绘制期变换零重排），
     层内 `layers.<层id>.<属性名>`（和 states 同一批属性）。
     每条通道可写 `from`（可省）/`to`（必写）/`duration`/`easing`/`delay`；层内条目还能写
     `when: with|after`（after = 延后到视图级动画结束，解析期折算成 delay）。
     **没写 transitions = 默认淡入淡出**；**没写某个层 = 那层不动**；`duration: 0` = 瞬变。
     `when: before` 报错（看不见的时段做动画没意义），出场/隐藏写 `after` 只警告并按 with 处理。
     `spec.totalMs` = max(事件时长, 各条目 delay+duration)，**调用方用它决定摘页/摘弹层的时机**
     （报早了动画被砍，和"关弹层要等动效播完"是同一个坑）。
     运行时：外观里写了走 `TransitionPlayer`（视图级通道写 `View` 的绘制期变换 = 零重排，
     层内通道写 `AppearanceInstance`，并**先撤掉同通道的状态补间**）；**没写就默认淡入淡出**
     （时长取主题 `metric.anim.normal`）；没写 `from` 的通道起点取当前值。
     `ViewTransition::getActiveDuration()` 是本次**实际**时长，`Shell/Pager::getTransitionDuration()`
     报它 —— 出场播完才摘页/摘弹层。`playShow/playHide` 只有"外观写了"或"默认淡入淡出"两种，
     隐藏后要不要真的 GONE/摘掉由调用方在回调里做（提前 GONE 动画播不出来）。
     spec 路径收尾**不复位**（终值是外观里写的），预设路径照旧复位。
     改 `ViewTransition` 时留意：有效时长要在 `suspended()` **之前**算（配置值 0 会被当成"不动"，
     而 0 的语义是"用主题时长"，想真不动用 `ContentTransition::NONE`）。
  ⑦c **`rect` 层可以写 `gradient`**（`docs/rect-gradient.md` / `docs/json-syntax.md`）：
     语法 `gradient{ type(linear|radial), angle, center, stops[2..4]{at,color} }`，
     `at` 必须 0..1 且严格递增，五类写错都**报错**（不静默降级）。
     写法优先于 `fill`；**渐变不做补间**，`stroke` 渐变不支持。
     实现是 **ImDrawList 顶点色网格**（`bklv/foundation/core/gradient.*` 的 `addGradientRectMesh`）——
     三条后端零改动；采样/参数/网格都是纯函数（「线性渐变投影范围按四个角取 min/max」：
     45° 写错时 t 会到 1.5）。
  ⑦ **一行里放多个内容层要用 inset 排队，并写 `measureContent: "sum"`**：
     绘制层不是流式布局（`align: left` 的两个层会叠在左边，参考 `base/hint_item` 给
     文字层写 `inset: [0,0,0,34]` 跳过图标）；默认固有尺寸取"最宽的一层"，
     一行里的图标 + 文字 + 信息必须按行**求和**才不会溢出（`measureContent: "sum"`）。
  缺项回退链：当前主题 → default → 控件空外观（警告，不崩）。
- **文本输入（IME）**（`foundation/core/ime.*` + `ui/widgets/edit_text_dialog.*`）：
  平台决定走哪条路——`ImeManager::open()` 返回 true 表示平台接管
  （Switch 用 libnx 的 `swkbd` 弹系统软键盘，同步阻塞，主循环会停住），
  返回 false 表示没人接手，控件自己读键盘（桌面：ImGui 字符队列 + Backspace/Del/方向键/回车/Esc）。
  **编辑逻辑放在 `TextEditModel`（纯数据、不碰 ImGui）**，所以能无窗口测；
  光标按**字符**走（中文/emoji 不会被切坏），闪烁在绘制期按时间算、不进模型。
- **裸 `AppearanceView` 也会在构造时 `applyTheme()`**：不这么做的话，
  直接 `new AppearanceView("xxx")` 的地方会拿到空外观，一笔都画不出来
  （EditTextDialog 的遮罩/面板踩过：结构对、颜色对，就是什么都不显示）。
- **转场：入场 / 出场分开**（`foundation/anim/transition.hpp` 的 `ViewTransition`，
  `ui/widgets/shell.*` 的内容栈与 `ui/widgets/pager.*` 的同级换页共用它）。
  入场 = 新页面板从右滑入 + 淡入；出场 = 旧页面板向左滑出 + 淡出。两段都是**绘制期变换**
  （`setTranslateX`/`setAlpha`，`View::frame` 会把整棵子树一起变）→ 过渡期间**零重排**。
  接口：`setEnterTransition/setExitTransition`（`NONE`/`SLIDE`/`FADE`/`SLIDE_FADE`）+
  `setTransitionDuration` + `isEntering()/isExiting()/isTransitioning()` +
  `finishContentTransition()/finishPageTransition()`（立刻落位）。
  语义要点：
  ① **两段是串行的**：旧页先出场（它还挂在树上，所以看得到），出场播完才摘/删它、
     再挂新页并让它入场 —— 所以下钻/切页都能看到两层各动一次，不是"旧页瞬间消失"。
  ② **回调等出场播完才执行**（和 Dialog"播完再摘"同一约定），此时被弹出的临时页可以释放。
  ③ push 的旧页留在栈里只脱离树；pop 掉的那页在**出场播完时**释放。
  ④ 需要确定性步进（测试）用 `stepContentTransition(dt)`／`stepPageTransition(dt)`。
- **异步回调要带生命周期护栏**（`foundation/core/alive.hpp`）：`task::run*` 的 `done` 在主线程执行，
  但发起时捕获的 `this` 可能已经析构（换页、重建视图树、回收器释放）→ 裸捕获就是
  use-after-free。做法是成员放一个 `AliveFlag`，回调捕获 `alive.handle()`（`weak_ptr`），
  进来先 `if (guard.expired() || !guard.lock()->isAlive()) return;`
  （borealis 用 `ASYNC_RETAIN`/`ASYNC_RELEASE`/`ASYNC_TOKEN` 三个宏手写引用计数做同一件事，
  这里换成无宏的小对象）。`Image::setImageFromFileAsync` 已经这么做了，有测试覆盖
  "视图先销毁、回调后到"。
- **延迟任务**（`foundation/core/task.hpp` 的 `task::delay(ms, cb)`）：cb 由 `pumpMainThread()`
  （`App::frame` 每帧）在**主线程**执行，不占线程、不用额外定时器；`DelayHandle` 拿来
  显式 `cancel()`（重复触发前先取消上一个，就是"重置计时器"的写法）。
  **析构不取消**：`task::delay(1000, [] {...});` 这种 fire-and-forget 必须能生效
  （当初做成"析构即取消"，临时句柄在语句结束就销毁 → 回调静默不触发，测试抓到的）。
  回调里碰视图照样按上一条加 `AliveFlag` 护栏。
- **事件总线**（`foundation/core/event.hpp`）：`getEventBus().subscribe<事件类型>(...)` 返回 **RAII 句柄**，
  析构自动退订（换根/关页面时也可以 `clear()`）。框架自己发 `FocusChangedEvent` 与
  `ActionTriggeredEvent`。
  **不许用 `typeid`/`std::type_index`**（Switch 带 `-fno-rtti`）——总线内部用
  "每个事件类型一个静态 token，取地址当 key"；派发期间增删订阅按快照处理。
- **弹层与模态输入**（`ui/input/input.cpp` 的模态栈 + `ui/app/app.cpp` 的 `pushOverlay`）：
  `App::pushOverlay(view)` 把弹层挂到根视图最上层并切模态；`popOverlay` 摘下并恢复焦点。
  三条：
  ① **模态生效时指针命中从模态根开始**（点到外面等于点空白，不穿透），
     方向导航也会被挡在模态外（`isInsideModal`）。
  ② **App 销毁 / 换根视图时必须 `resetFocusState()`**：模态栈是进程级静态，
     根视图一删里面的指针就是野指针，之后的导航/命中会踩崩（测试里踩过）。
  ③ **关掉弹层要等动效播完再摘**：摘掉就脱离视图树，`update()` 不再跑，
     关闭动效根本播不出来。Dialog 是"先切状态、播完再 popOverlay"。
- **音效只在用户操作时播**（`ui/input/input.cpp`）：全库只有 `playUiSound()` 一处播音，三个触发点
  —— 方向导航（focus 音，含"没有焦点时取默认焦点"那一支）、确认键 / 点击（click 音）、返回键（back 音）。
  `InputManager::giveFocus(view, playSound = false)` **默认静默**：换根 / 换页 / 弹层进出 /
  焦点兜底 / 焦点记忆恢复都属于程序化赋值，传 false；只有 `navigate()` 的两处传 true。
  踩过：以前 giveFocus 无条件播 focus 音，宿主每加载一页都会"叮"一声；鼠标点按钮时还会
  focus + click 双响（`focusFromPointer` 不播 focus，点击只留 click）。
- **状态动效的取消要撤掉旧补间**（`ui/theme/appearance.cpp` 的 `setState`）：
  重算目标时**先撤该通道正在跑的补间，再决定要不要建新的**。
  不能等"目标与当前值不同"才撤——刚打开又马上关掉时当前值还等于初值，
  旧补间会被漏掉、继续往旧目标跑（弹窗面板整块补到透明就是这么来的）。
- 不生成 `imgui.ini`：绘制全在 viewport 绘制列表上，`io.IniFilename` 置空
- 日志用 `BKLV_LOG_INFO(...)` 等宏（带 文件:行号）。日志同时进控制台与
  `Logger` 的环形缓冲，界面上的日志窗口（`ui/widgets/debug/debug_log_view.hpp` 的 `DebugLogView`）读缓冲渲染。
  它**是视图树里的 View**（挂在外壳的调试层 `Shell::getDebugLayer()` 上，在 HudLayer 之上、
  永远最后绘制），直接用 `ImDrawList` 画文本、**不为每条日志建视图**，所以日志再多也不重排。
  **不要**为每条日志创建 View —— borealis 那样做会不断触发 Yoga 重排。
  **不支持滚动**：永远显示最新的一批（"即时显示"），要翻历史就开 `Logger::setFileOutput()`
  去文件里看 —— 所以没有 scrollTo/scrollBy/滚轮那一套（`getFirstVisibleRow()` 永远 = 总行数 − 可见行数）。
  它的绘制开销是**硬要求**，四条约束（改这里要守住，否则日志一多就掉帧）：
  ① 缓存里只存 `const LogEntry*`（不复制日志正文），只在「版本/过滤/字号/字体」变化时重建；
  ② 行高固定 + 不滚动 → 可见窗口就是"最后 N 行"，一次除法算出来，不从头遍历；
  ③ 每行只一次 `AddText`，拼接走复用缓冲（稳态零分配）；④ 面板不可见时 `View::frame` 直接返回。
  实测：30 条与 1000 条日志的顶点数/命令数**完全相同**（162/3），一帧约 2 微秒。
- ImGui 窗口在 `App::onDrawWindows` 里下发（视图树画完之后、`ImGui::Render()` 之前）。
  日志窗口本身已经是视图（`DebugLogView`）；这条路留给"需要自己的 ImGui 面板"的宿主。
- **按键重复（长按连续移动）**：`ui/input/input.hpp` 的 `KeyRepeater`——首次按下**立刻**响应，
  按住则等 `repeatDelay`（默认 250ms）后每 `repeatInterval`（默认 100ms）一次，
  和 borealis 的 `BUTTOM_REPEAT_TRIGGER` / `BUTTON_REPEAT_DELAY` 一致。
  时间来自 `getFrameDeltaSeconds()` 累加，键盘/手柄/各平台共用一套，不用平台 API；
  A/B 也走同一套（borealis 的 A/B 会重复）。
  **一个物理键一个重复状态**：以前两个键（方向键 + WASD）共用一个"边沿状态"，
  互相把对方清掉 → 按住方向键每帧都判成"刚按下"，导航飙到帧率。
- **侧栏（`ui/widgets/sidebar.*`）的面板观感只吃 `box` 段**：`applyPanelLook()` 走的是
  `applyWidgetBox(this, "base/widget/sidebar")` —— 也就是**只应用 box**（宽高/内边距/圆角/
  边框/底色/margin），文件里的 `layers` 段**不参与绘制**（但**必须存在**：解析器要求有
  `layers` 数组，删了会报「外观 JSON 缺少 layers 数组」）。所以要让面板有圆角/细边/半透明面，
  改的是 `base/widget/sidebar.json` 的 **box 段**，别去写 layers。
  投影没有对应的 box 字段，所以在 `Sidebar::draw()` 里补一层 `shadow::drawSoft(..., cutout=true)`
  （cutout = 只画外圈，不会把面板自己的底色压暗）。
- **侧栏条目 = 图标 + 文字 + 选中底**（`base/tab_cell.sidebar.json`）：
  条目是 `TabCell : AppearanceView`，图标走**现成的** `setIcon()` + 外观里的 `"icon"` 层
  （`"source": "icon"` → `getLayerText`/`getLayerFont` 自动认它、自动用图标字体），
  文字层用 `inset` 给图标让位（`metric.sidebar.titleInset`）。
  选中态要写 `bg.fill`（**别只有 accent + 文字变色** —— 15 条里太不显眼）。
  分组用 `Sidebar::addSeparator()`（不可聚焦、不进条目索引；用上 `metric.sidebar.separatorHeight`
  与 `color.sidebar.separator`），别塞一条假 tab 去占位。
- **页头与底部栏是外壳级的（全局），内容避让是显式的**（`ui/widgets/shell.*` + `ui/widgets/pager.*`）：
  宿主自己建 `Header` / `BottomBar` 交给 `Shell::setHeaderView/setFooterView`（挂覆盖层，
  中间用 `Padding` 顶开），页面再按 `getHeaderInset()/getFooterInset()` 自己留白。
  其中 `Header = [图标][标题][右侧区]`（base/widget/header*.json + base/header_icon.json），
  `BottomBar = [提示(左) | 状态区(右)]`（base/widget/bottom_bar.json），
  Header 的左右外边距用 `appletFrame.paddingSides`、底栏用 `hints.footerMarginSides`。
  显隐用**实例方法** `Shell::setHeaderVisible(bool)` / `setFooterVisible(bool)`：隐藏时看 `GONE`
  （不占位）；**栏里不能留固定外边距**（页头下边的留白写在页头自己的 padding 里），
  否则隐藏后会留下空隙。
- **大标题随滚动收起（iOS 风格）写在 `Header` 里**：`setLargeTitle("设置")` +
  `setCollapseSource(scroller)`，之后 `Header::update()` 每帧读
  `ScrollingFrame::getContentOffsetY()` 算进度（`setCollapseDistance` 默认 44px 算收起），
  展开时只显示大标题、收起后只显示页头里的小标题。**全程只改绘制期量**
  （alpha / `Label::setFontScaleX/Y` / `setTranslateY`）→ 零重排；不绑滚动源时也能
  `setCollapseProgress()` 手动驱动（测试走这条）。大标题是**绝对定位**在页头下沿之外的子视图，
  所以页头高度不受影响。字号取 `metric.font.size.title`（ios 主题 30、default 28），
  位置取 `metric.appletFrame.headerHeight` + `metric.spacing.sm`。JSON 侧对应属性 `largeTitle`。
  **content 避让不自动**：`Shell::setHeaderInset/setFooterInset` 由宿主给（动画期间给**目标值**，
  这样内容只重排一次），宿主再把 insets 加到内容层的 padding 上（宿主就是这么做的）。
  栏的"折叠补间"是**宿主**的事：`Animatable` 逐帧改两栏高度 + `App::requestLayout()`，
  补间结束才真正 GONE（demo 里由宿主自己实现）。
- **占位挤压用 `Padding`**（`views/padding.*`，对标 borealis 的 `brls:Padding`）：
  空视图、auto 尺寸、`grow=1`，把后面的兄弟挤到另一端；页头就是
  `[图标][标题][Padding][右侧区]`。JSON 里写 `{ "type": "Padding" }` 也能用。
  比"给某一侧加 grow + justifyContent"更直白，也不吃焦点。
- **加插值属性要往 `ALL_PROPS` 补一条**：`PROP_COUNT` 是从 `ALL_PROPS` 的表长**推导**的
  （以前写死 13，加两个属性后 per-layer 数组少两格，越界写把堆写坏、崩在 malloc 里）。
  加 `AppearanceProp` + `ALL_PROPS` + `PROP_NAMES` + 每层初值 + `getNumber` 回落，
  五处要同步，`static_assert` 与顺序自检会兜住。
- **焦点框（流光框）画在浮层上，控件只声明形状**（`graphics/paint/focus_ring.*` +
  `ui/shell/focus_ring_view.*` + `View::getFocusOutline()`）：
  ① **单向依赖**：`FocusRingView` 每帧问 `getInputManager().getCurrentFocus()`，再问那个视图
     `getFocusOutline()` → 控件完全不知道谁在画框，换画法/换类型/开关都不用动控件。
     形状用**圆角矩形**统一表达（半径自动钳到短边一半）：矩形 r=0、胶囊 r=高/2、正圆 r=短边/2，
     所以"聚焦到圆形控件上就变圆"是自动的（`AppearanceView` 从 bg 层读出圆角补上）。
  ② **颜色只来自 LUT 贴图**（`resources/focus_ring_gradient.png`，512×4 横向色谱）：
     U = 弧长 + 相位，V = 描边宽度方向；跨接缝的段按 U 比例**拆成两段**画，否则 UV 被拉花。
     这是 GBAStation `nvgBoxGradientLUT` 的同款做法（那边改 NanoVG paint，这边用 ImDrawList）。
     ⚠️ 贴图没加载成功时**什么都不画**，不要退回"自己算的颜色"。
  ③ **所有者是顶层外壳**：`Shell::setFocusRingEnabled/setFocusRingType/getFocusRingStyle()`，
     挂在 HUD 层（永远最上层）。**样式不跟主题 JSON 走**（用户要求），默认值写死在代码里。
  ④ **外观不再画聚焦的框**：聚焦状态里原来的描边层（`hlGlow` / `hlBorder`）与高亮底
     （`hl_bg` / `hlBg.fill`）在 2026-10 清理里一并删掉了（现状见上面"通用层都在 base/layer 下"）；
     聚焦反馈改成**轻微放大**（`AppearanceView` 的 1.05 / 130ms，代码写死，可用
     `setFocusScaleEnabled(false)` 关掉）。改回"外观画框"之前先想清楚：框在浮层上才能
     跨层、才能不被父容器裁剪。

- **`shadow` 的 `cutout`**：打开后只画外圈（把控件自身矩形挖掉，borealis 用 `NVG_HOLE`
  做同一件事），内圈一个顶点都不落 → 后面加的层即使是**半透明底色也不会透出阴影**。
  实现是**壳层网格**：从控件圆角轮廓（alpha 0）沿外法线一圈圈往外长，每圈 alpha 由
  "到 offset 后阴影形状的距离"决定 —— 孔的边界天然是圆角，offset 只影响阴影投在哪，
  控件内部一个顶点都没有（矩形 `PushClipRect` 分块会让圆角处出现方角缺口，踩过）。
  默认关，按需在 shadow 层写 `"cutout": true`。
- **`shadow` 的 `inner`**：同一个 `op`、同一个 `fill`，`"inner": true` 就从外投影变成**内阴影**
  （`shadow::drawInnerSoft`）。四条与"另一种做法"有关的约定：
  ① **不做成新 op**：它是同一种柔和投影、只是方向朝内；想同时要内外两层写两个 shadow 层即可，
     而新 op 会牵动 `Op` 枚举 / 注册表 / 文档里的"内置 6 种"口径，收益为零。
  ② **层序与外投影相反**：外投影吃 `LayerOrder::SHADOW` 档、解析期被提到最前（垫在控件下面）；
     内阴影画的是**内缘**，被底盖住就等于没画，所以 `sortLayers` 里按"常规内置层"处理，
     保持 JSON 书写顺序 —— 作者把 `bg` 写在它前面就行，排反了内阴影会被底盖住。
  ③ **内阴影没有方向**：它就是"从边缘向内扩散"的一圈带子（贴内缘最暗、往内渐透），
     所以 **`offset` 对它无效** —— 别再给它加方向语义（试过 Figma/PS 那套"offset 反侧最暗"，
     用户明确要的是无方向）。想做出某一侧更重的凹陷感就叠两层、其中一层用 `inset` 收窄。
     四角由圆角矩形 SDF 保证**等宽内缩**，不是"到最近边的距离"（后者四角会叠得更黑）。
  ④ **实现是逐顶点算 alpha 的网格**（不是描边）：`PathStroke` 一个路径只有一个颜色，
     而内阴影的 alpha 要按"离轮廓多远"逐点算（圆角上各点不同、直边上相同）。
     顶点 alpha 用圆角矩形 SDF 精确算、圈间线性插值（和 `graphics/paint/gradient.cpp` 的网格同套路），
     所以它天然待在形状内部、不需要裁剪（ImGui 的裁剪只有矩形，圆角处会漏）。
- **自己建 `ImDrawList` 画东西就要用真 ImGui 上下文**，三件事缺一不可（三个坑都踩过）：
  ① `ImGui::CreateContext()` 要有；
  ② 自建列表必须紧跟 `list._ResetForNewFrame()`，否则内部没有 draw cmd，
     `PrimReserve` 越界崩溃；
  ③ `list._FringeScale = list._InvFringeScale = 1.0f` —— 没有帧时它们是 0，
     `_AddPolyline` 会把 `screen_thickness` 算成 0 然后**静默 return**（粗描边什么都画不出来）。
  另外收尾**别调 `ImGui::DestroyContext()`**：没接平台后端的上下文在 `Shutdown()` 里会踩空。
  拿不到上下文时（或不想建）就只断言"会画几层"这类可数事实（`shadow::layerCount()`）。
- **ImGui 1.92.8 起 `PathStroke` 的参数顺序是 `(col, thickness, flags)`**，不是
  `(col, flags, thickness)`。写反了编译器不报错（两者都能隐式转换），但 thickness 变成 1、
  flags 变成垃圾 → **什么都画不出来**，只能靠看图或顶点数发现（`_AddPolyline` 里那段
  注释就是提醒这个改动的）。
- **圆角不要靠"四条带子"拼**：`AddRectFilled` 会把圆角**钳到边长的一半**，带子又矮又窄时
  圆角直接被钳成直角（用户报过"cutout 之后圆角变直角"）。要圆角就沿中线描一圈
  （`PathRect` + `PathStroke`），描边没有这个钳制，而且内缘正好贴合控件的圆角形状。
- **下拉弹窗（`ui/widgets/dropdown.*`）**：点触发器 → `App::pushOverlay(DropdownPopup)`，
  弹层 = **遮罩层 + 弹窗面板**：
  · 遮罩铺满全屏、取 `color.common.overlay`（把底下界面压暗，和 Dialog 的遮罩同一个键）；
  · 面板（`DropdownPanel : AppearanceView`，外观 `base/dropdown_popup.json`）画在遮罩**之上**，
    里面是选项列表（可滚），锚在触发器附近。
  要点：
  ① **外观只能套在那块面板上**，套到全屏弹层上会把整屏刷成面板色（底下界面全看不见，踩过）；
  ② 打开后**焦点落在「当前已选项」**（`listContent->setDefaultFocusedIndex(selected)` +
     显式 `giveFocus`，因为"上次焦点记忆"会盖过默认索引），并 `ensureFocusVisible()` 滚进视野；
  ③ **选中任意一项就关掉整层**（`onSelect` → `dismiss` → `popOverlay`）；
  ④ 弹出位置按可用空间算（`fitToSpace`）：下方放不下且上方更宽裕就翻到触发器上方，
     两边都不够就压缩列表高度；
  ⑤ 关掉的入口：选一项 / 点遮罩（非可聚焦 → `hitTest` 返回 nullptr → 模态**空白点击回调**）/ 按 B。
- **底栏顺序：提示在左、状态在右**（borealis 的 `bottomBarXML` 是 `Hints` 在前，
  外层 `justifyContent=spaceBetween`）。顺序别改成反的。
- **有外边距就别写 `width: 100%`**：`100% + margin` 会多出边距那一段（页头/底栏右边被吃掉、
  内容的右上按钮被推出边界，都踩过）。要"填满父容器再留边距"就用 **auto 宽 + margin**，
  父容器负责 stretch（Shell 的内容层／宿主容器自己 `alignItems: stretch`）。
- **滚动容器用 `flex-basis: 0` + `flex-shrink: 1`**（`views/scrolling_frame.*`）：
  Yoga 的 flexShrink 默认是 **0**，而 basis 默认是内容尺寸。两者不改的话，内容一高，
  Yoga 会按比例把**同一列里的兄弟节点一起压缩**（按钮行 48 → 28），滚动器自己的高度
  也随内容变 → 每次切页内容区高度都变，看着像底栏在上下闪。
  `grow` 配 `basis 0` 才是"只占剩余空间"。
- **图标有两套，"全部字形"要扫字体而不是列手写表**（`foundation/core/font.*` + `foundation/core/icon_font.*`）：
  `resources/font/switch_icons.ttf`（按键/系统图标）与 `MaterialIcons-Regular.ttf` 都是按
  **整个私有区**（`PUA_RANGES = U+E000~U+F8FF`）装进图集的，字体里实际有的字形远多于手写表：
  ```
  bklv::scanFontCodepoints(font)      // 扫一个字体在私有区里实际有的码位（升序）
  bklv::getIconFontCodepoints()       // switch_icons.ttf 的全部码位（带缓存）
  bklv::icon::allGlyphs()             // Material 的全部码位 + UTF-8（手写表 all() 只是子集）
  bklv::icon::utf8Glyph(codepoint)    // 码位 -> UTF-8 字形
  ```
  判断"字体里有没有"用 `ImFont::IsGlyphInFont`（走 stbtt，不是查范围），再配
  `GetFontBaked(大小)->FindGlyphNoFallback()` 滤掉 cmap 里映射到空槽的码位。
  **注意 `ImFont::FindGlyphNoFallback` 在 1.92 里搬到了 `ImFontBaked` 上**，ImFont 上没有。
  `switch_icons.ttf` 里 **U+E154 起全是占位字形**（空框/缺字方块），所以
  `getIconFontCodepoints()` 裁到 U+E153 为止（`SWITCH_ICON_LAST_USABLE`）；
  `scanFontCodepoints()` 保持"原样扫"，要什么范围自己裁。
  另外 `setIconFont(ImFont*)` 可以手动指定图标字体（默认由 `loadFonts()` 装好后设）。
  **"哪一层用哪套"按层声明**：`setLayerIconFontKind(layerId, MATERIAL|SWITCH)`（不指定就跟随
  `setIconFontKind` 的全局那套），取指针走 `AppearanceView::iconFontFor(kind)`；
  `getLayerFont()` 已经会读它，子类**不要**再重写 `getLayerFont` 硬编码字体。
  **码位必须和字体配套**（同一码位在不同字体里是不同图标：选择器箭头踩过"画成笑脸+表格"），
  完整规则、实测对照表与三种典型写错方式见 **`docs/icon-fonts.md`**。
- **图片解码的统一契约**（`foundation/core/image_decoder.*`）：对外只有 `decodeImageFile/Memory`
  与 `decodeAnimationFile/Memory`，内部**按文件内容**分发（`detectImageFormat` /
  `probeImageSize` / `isImageSizeAllowed`）。三条硬约定：
  ① 输出**非预乘（straight）RGBA8** —— ImGui 的混合方程是 `SRC_ALPHA/ONE_MINUS_SRC_ALPHA`
  （GL 与 Vulkan 都是），预乘图会整体偏暗；macOS 的 `CGBitmapContext` 只支持预乘 alpha，
  所以 ImageIO 那条路解码后要**反预乘**（`image_decoder.cpp` 里那段注释就是钉这个）。
  ② 失败必须带 `DecodedImage::error`（人话），不要只返回空图。
  ③ 解码前先按头部卡尺寸/像素上限，畸形文件不能"解完再说"。
  GIF **全平台同一条实现**：`image_decoder_gif.cpp` 单独实例化一份 stb（`STB_IMAGE_STATIC`
  + `STBI_ONLY_GIF`，两份 stb 靠 static 避免符号冲突，第三方警告在该 TU 里局部压掉）。
  `stbi_load_gif_from_memory()` 直接给**合成好的整幅帧** + 每帧延迟（毫秒），
  帧内的局部矩形/透明/disposal 都由它处理 —— 所以 GIF 播放只剩"按时换帧"。
  WebP 同理走 libwebp 的 `WebPAnimDecoder`（`image_decoder_webp.cpp`），帧也是合成好的。
  SVG 在 `image_decoder_svg.cpp`：**必须给像素尺寸**（`decodeSvgFile/Memory(path, w, h)`，
  给 0 用文档自带尺寸）；lunasvg 的位图是 **ARGB32 预乘**，用它的 `Bitmap::convertToRGBA()`
  转成非预乘 RGBA（自己手写容易漏通道顺序）。
  **两个可选解码器都不许让配置失败**：CMake 里 `BKLV_WITH_WEBP` / `BKLV_WITH_SVG`
  先探测子模块是否存在，缺了就只打一条提示并定义不启用 —— 对应的 TU 编成"返回带
  error 的空图"的桩，调用方只看到一句人话错误。Switch 脚本用 `--no-webp/--no-svg` 关。
- **动图播放挂在既有帧时钟上**（`views/image.*`）：`update()` 里 `stepAnimation(getFrameDeltaSeconds())`
  → `frameIndexAt(animation, elapsed)`（对一轮总时长**取模**，不漂移、暂停回来不补播）
  → 帧变了才 `updateTexture()` **就地换内容**（不重建纹理、不参与布局）。
  只在**可见**时推进（离屏不占上传带宽）。纹理是视图自己上传的（`ownsTexture`），
  析构/换图时必须 `releaseTexture()`，别漏。
- **按钮家族**（`ui/widgets/controls/button.*`）：抽象基类 `Button` **只描述行为与基础外观** ——
  行为是点击 / 禁用 / 开关 / 按下反馈 / 属性表套路；基础外观是 `base/button/button`，只画
  背景 / 边框 / 阴影 / 高亮底，**没有任何内容层**，所以子类不用管外观。
  具体形态 `TextButton` / `IconButton` / `IconTextButton` 只声明"我有哪些内容位"，外观是
  `base/button/button.<形态>`（`button.text` / `button.icon` / `button.icon_text`）；
  换皮肤就把 variant 换成别的后缀（`TextButton("确定", "bordered")` 走 `button.bordered`）。
  内容位 id 是按钮这一层的约定，写在 `button_slot` 里，外观 JSON 的 `"data_id"` 与它逐字一致：
  `m_icon`（字体图标）/ `m_text`（主文本）/ `m_subtext`（次文本）/ `m_infotext`（信息文本）。
  新形态从这四个里挑，别另造名字 —— 名字就是槽，换名字等于换槽，皮肤里会取不到内容。
  视图 id 是另一个维度，走 `View::setId()`（JSON 里写 `"id"`，之后 `findViewById("ok")` 取控件）。
  选择器按钮已随控件清理移除，重做时照下面的约定：
  1）**变体 = 组合外观**，代码只给内容和行为。`AppearanceView::setAppearanceName(name)`
     换基名（名字是主题里的完整路径，如 `base/widget/button.icon_text`）；一行放置靠层 inset。
  2）内容槽在 `AppearanceView` 上（按钮只把四个内容位起了名字，见上）：`setIcon()`（图标字形）与
     `setInfoText()`（右侧信息）；`getLayerText` 认 `"text"/"icon"/"info"`，
     `getLayerFont` 对图标层返回图标字体（用正文字体会把图标画成方块）；
     同一个控件里要多套图标字体混排就按层声明 `setLayerIconFontKind(layerId, kind)`
     （见上面"图标有两套"那条与 `docs/icon-fonts.md`），**别重写 getLayerFont 硬编码**。
     **图标字体有两套，用哪套由控件说**：`setIconFontKind(IconFontKind::MATERIAL | SWITCH)`，
     **默认 MATERIAL**（`icon::font()`，控件图标用它；`icon::glyphByName("star")` 取字形）；
     SWITCH 那套（`getIconFont()`，按键/系统图标）**只给提示栏、菜单这类地方**用
     （`HintItemView` 自己 override 成 SWITCH）。`Dropdown` 的触发器也能换这套外观。
  3）开关类（`switch_text` / `switch_icon`）用**外观状态** `on`/`off` 表达布尔态
     （`Button::setSwitched`），过渡动画由主题里那两个状态的 duration/easing 描述 ——
     不要再写一套动画代码；图案开关的滑块位移就是 `knob.offsetX` 补间。
  4）**选择器按钮**：`SelectorMode::NUMBER`（min/max/step/小数位，按步长吸附后**夹取**）
     或 `LIST`（越界**环绕**）。点按/A 进设置态，进入后
     `getInputManager().setFocusCapture(this)` 把焦点钉住：方向键不再导航，
     全部转给 `View::handleDirectionalInput(direction)`（新虚函数，返回 true = 消费）。
     长按连发不用自己写计时器 —— InputManager 的 KeyRepeater 就在这条路上
     （首按立刻、之后 250ms/100ms）。失焦自动 `endEditing()`，不会留下"还在改"的控件。
- **`RenderBackend::updateTexture()`**：动图换帧用的"就地更新"接口（GL/GLES 走
  `glTexSubImage2D`，Vulkan 走 staging + 屏障 + `vkCmdCopyBufferToImage`）。
  没有它的后端返回 false（默认实现），上层当作"这次换帧失败"处理。
  Vulkan 那段和 `createTexture` 的上传步骤同构但**故意不共用** —— 那条路径是验证过的，动它风险更大。
- **页面的控件一行一个**：别把好几个按钮塞一行（挤在一起会截断信息文字）。
  用"一行 + 控件 `grow=1`"铺满；容器/按钮的宽度靠 `measureContent: "sum"` 撑开，
  不要靠手写固定宽度。
- **页面里不要再套 `ScrollingFrame`**（页面约定）：非自滚动页（`selfScrolling=false`）由
  宿主的外层滚动器负责滚动，页面的外层框是 `height: auto` —— 里面再放一个
  `grow/shrink` 的滚动器拿不到确定高度，高度会算成 0，**整页内容都看不见**（图标总览页踩过）。
  要页面自己滚就把 `addPage(..., selfScrolling=true)`，宿主会按滚动器实测高度补外层框高度。
- **页面在 `App::run()` 之前建 = 字体还没装**：`App::run()` 里才建图集并 `loadFonts()`，
  而视图树（`setRootView`）在那之前就建好了。所以任何"构建页面时就去查字体/图标表"的代码
  在首次构建时都会拿到空表（图标总览页曾经整页空白）—— 因此要把**起始页推迟到第一帧**再选
  （`pendingStartPage` 在 `update()` 里消费）。
- **`AppearanceView` 是容器，`update()` 必须先 `Box::update()` 递归子树**：
  底栏里的提示栏/状态区、页头里的标题、弹窗面板里的按钮行都靠这条，
  漏了的话子树永远不会 update（踩过：提示栏一直不刷新、看着像空的）。
- **提示栏用控件而不是覆盖层**（`ui/widgets/hints_bar.*`）：它是底栏的左侧内容，
  条目按当前焦点的动作重建（签名没变就不重建，避免每帧重排）。
  图标是**图标字体的私有区字形**（码位同 borealis 的 `Hint::getKeyIcon`）：
  Switch 靠系统字体 `NintendoExt` 回退，桌面随包带 `resources/font/switch_icons.ttf`。
  文字层要用哪个字体由 `AppearanceView::getLayerFont()` 决定——
  图标层必须返回 `getIconFont()`，用正文字体会量错尺寸并画成方块。
- **底栏/页头 `setShrink(0)`**：内容区要的比剩下的多时，该被压缩的是内容区（它会滚动），
  不是底栏（压了连图标都放不下，踩过：22px 被压成 14px）。
- **动作分发：手柄/键盘沿父链冒泡，指针只认命中视图自己**（`View::triggerAction`）。
  容器级的动作必须能在焦点位于其子节点时生效，否则形同虚设：
  `Shell` 的 "B 返回上一层"（`setOnBackRequested`）、`Dialog` 的 "B 关闭" 都靠这条
  （不冒泡时焦点在内容里的按钮上按 B 完全没反应）。
  指针不冒泡是因为 `hitTest` 只返回可聚焦视图，命中的本来就是控件本身。
- 鼠标点击默认**只聚焦不执行**动作。想点一下就执行，在控件构造函数里设
  `onPointerAction = true`（按钮、tab 都这么做）；列表项保持默认，避免误触发


## 接口命名与调用风格

一套规则，新增 API 时照着写。可执行检查：`python3 scripts/check_naming.py`
（枚举值风格、类型/文件名风格、动词式布尔开关、bool 查询命名，有违例时退出码 1）：

| 类别 | 规则 | 例子 |
|---|---|---|
| 类型 | PascalCase，不用前缀 | `AppearanceView` / `ScrollingFrame` / `TextureRef` |
| 方法 / 自由函数 | camelCase，动词开头 | `setEnabled` / `applyTheme` / `markMeasureDirty` / `resourcePath` |
| 成员变量 | camelCase（私有，不加 `m_`/`_` 后缀） | `headerVisible` / `appearanceName` |
| **枚举值** | **UPPER_SNAKE**（全库统一，含 `Op`/`Kind`/`Sound`/`EasingFunction`） | `Visibility::GONE` / `Sound::CLICK` / `EasingFunction::BOUNCE_OUT` |
| 状态查询（返回 bool） | `is*` / `has*` / `can*` | `isFocusable` / `hasTexture` / `isAppearanceAnimating` |
| 值访问器（返回数据） | `get*` | `getWidth` / `translate` / `getBool`（数据取值，不是状态查询） |
| **动作返回是否成功** | 保留动词式（不加 `is`） | `initContext` / `open` / `pushOverlay` / `step` / `recognize` |
| 布尔开关 | `set<名词>Enabled` / `set<名词>Visible` / `set<名词>Animated` | `setBarsAnimated` / `setHeaderVisible` / `setBatteryVisible` |
| 回调 | `setOnXxx` | `setOnClick` / `setOnChange` / `setOnSubmit` |
| 容器操作 | `addView` / `removeView` / `pushContentView` / `popContentView` | 与容器语义一致 |
| 子类扩展点 | `applyTheme` / `drawCustomLayer` / `getLayerText` / `getLayerImage` / `getDefaultFocus` | 外观与交互的固定钩子 |
| 自由函数分组 | 同一主题的散函数放同名 `namespace` | `bklv::task::run` / `bklv::audio::play` |
| 文件 | snake_case；一个控件一对 `.hpp/.cpp`（同族小类可合并，如 `cell.*`） | `scrolling_frame.*` / `cell.*` |

例外只有两种，都写进注释：① STL 风格的 `std::string::empty()` 这类**标准库**成员照旧；
② 数据取值用 `get*`（`getBool`），即使返回 bool 也不算状态查询。

## 加一种图层效果（外观层的扩展机制）

外观是由一层层 `op` 拼出来的，内置只有 6 种：`rect` / `text` / `image` / `shadow` / `glass` /
`custom`。**加自己的层不用改库任何文件**：写一个文件 + 注册一行。

可执行检查：把自定义层写进一份临时外观 JSON，先看解析报错（参数名/类型/数量），
再建个临时程序把它画出来看形状与顶点数。
语法面向使用者的说明与一份完整可编译的内联样例：`docs/json-syntax.md` 的「扩展图层类型」一节
（`demo/contrib/` 里的旧样例目录已随设计器一起移除）。

```cpp
// 1) 声明这个层是什么（静态成员 + 参数模式 + 绘制）
struct StripeLayer
{
    static constexpr const char* kName  = "stripe";   // JSON 里的 op
    static constexpr const char* kLabel = "斜条纹";    // 显示名（工具/日志用）
    static constexpr auto kOrder = bklv::compile::LayerOrder::DECORATION;
    // kIdPrefix / kHelp 可省（默认 kName / 空串）

    // 参数模式：引擎按它解析（类型 / 720p 缩放 / 枚举校验），工具侧的候选键也由它派生
    static std::vector<bklv::compile::LayerParam> params()
    {
        return { bklv::compile::LayerParam::number("angle", 45.0f, "角度"),
                 bklv::compile::LayerParam::pixels("lineWidth", 2.0f, "线宽") };
    }
    static void draw(const bklv::compile::LayerDrawContext& ctx);
};

// 2) 注册（无宏、无静态初始化魔法；时机由调用方决定）
bklv::compile::registerLayer<StripeLayer>();
```

五条约定：

① **参数一律走参数模式**（`LayerParam::number/pixels/color/boolean/enumeration`），
   别再把自定义字段硬塞进引擎字段 —— 样例当初把角度存进 `feather`、线宽存进 `spread`，
   既容易撞车也读不懂，而且 `feather` 是像素量，角度被顺手缩放了（bug）。
   JSON 里没写参数 = 用模式里的默认值（**不是 0**）。每层上限 `kMaxLayerParams`(8)，超了注册期报错。
② **参数名不许占用引擎保留键**（`op`/`id`/`width`/`height`/`inset`/`align`/`valign`/`offset`/`opacity`）
   —— 引擎会先拿它们算矩形，参数永远收不到值，注册时**直接报错**。
   撞上引擎**会解析**的键（`fill`/`radius`/`angle` 那类，见 `engineLayerKeys()`）只警告：
   那会有两份值，直接用 `ctx.layer.fill` 就好。
③ **标量参数不写 parse 钩子**：引擎按模式全自动解析。只有数组/对象这类结构参数
   （像 `rect` 的 `gradient`）才写 `static parse`（有就自动挂上，检测惯用法）。
④ **`draw` 里只读解析结果**（`ctx.layer` / `ctx.rect` / `ctx.type`），不查 JSON、不查主题 ——
   与内置层同一条约定。取参数用 `numberParam(ctx, "名字")` / `enumParam(ctx, "名字")` 这类
   按名字的访问器，别自己记下标（参数名就是契约）。
   **参数补间目前没有**（补间通道是固定的 `AppearanceProp` 枚举），所以"要动起来"的层得自己算：
   时间用 `foundation/anim/time.hpp` 的 `getSecondsNow()`（单调、无窗口安全），随机用纯哈希/值噪声
   （同输入同输出，可测）—— 下面「扩展图层类型」一节的样例就是这么算的。
   别用 `ImGui::GetTime()`：无窗口时会踩空（见上面"update() 路径不许读 GetIO"那条）。
   自己算出来的位移最好再夹一次边界（`gain` 这类"乘在后面的系数"能绕过前面的夹取）。
⑤ **`void*` 别拆**：`LayerDrawContext` 里的 `drawList` / `view` 是不透明指针，
   为的是让 `compile/` 不依赖 ImGui 与 widgets（`ctx.type` 是唯一例外：它是 compile 自己的类型）。

层序要注意：内置层之间**按 JSON 书写顺序**画（只有阴影、玻璃被提到最前），
声明 `DECORATION` / `OVERLAY` 的扩展层画在所有内置层之上。
**不要**把内置层改成按七档阶梯重排 —— 实测 168 份发布外观里有 121 份把 rect
（`hlGlow`/`hlBorder` 聚焦描边）写在 text 之后，重排会让这些描边掉到文字下面。

## 分层依赖方向

所有模块**编进同一个 OBJECT 库、共用一个 include 根**，所以"分层"是目录约定而不是
编译单元边界 —— 编译器不会因为 `ui/app/app.cpp` 反向 include 了 `ui/widgets/` 而报错。
可执行检查：`python3 scripts/check_layers.py`（有新增反向依赖时退出码 1，`--list` 看全量）。

**目录是"大类 / 模块"两级**：大类表示架构层，模块是职责单元
（`graphics/render/glass/` 是 render 的子系统，不单独算一层；规则按模块判 —— 模块名在整个
`bklv/` 里唯一，所以 `check_layers.py` 的规则表按模块名写）：

```
  foundation/   基础   core（生命周期/事件/日志/资源/任务/服务/平台抽象）
                       geom（几何与颜色换算）    json（JSON 读取、变量块、错误策略）
                       anim（时间/缓动/过渡）
  graphics/     绘制   render（图形接口 + GL/VK/GLES3/libnx 后端 + glass 子系统）
                       paint（渐变/投影/焦点框）
  ui/           界面   view（View / Box / 视图树 / JSON 视图）
                       theme（外观数据模型与解析，层类型表 layer_registry 也在这里）
                       layer（层绘制实现与注册模板）
                       widgets（外观驱动控件、基础件、容器、列表、调试覆盖层）
                       input（键盘/手柄/手势/输入法）
                       app（App / Context，组合根）
                       shell（五层容器 + 焦点框，宿主装页面的地方）
  system/       运行时 media（字体/图标/图片/音频）   platform（SDL3、Switch 宿主适配）
```

大类之间只有一条硬规则：**下层不必认识上层，上层可以整体依赖下层**。
再加一条实践取向：`ui/` 里 view → theme/layer → widgets → shell 是四层递进，
后一层认识前一层，反过来不行：`ui/widgets/` 认识 `ui/view/`，`ui/shell/` 认识 `ui/view/`
但不认识 `ui/widgets/`（外壳只跟 `View` 与焦点接口打交道，不 import 任何控件）。

被禁止的方向逐条列在 `scripts/check_layers.py` 的 `FORBIDDEN` 里，摘几条最容易踩的：
基础机制（core/geom/json）不许认识外观、视图树与控件；渲染后端不许认识外观与视图；
外观解析不许认识视图树（现存两处已记进 `BASELINE`）；视图层不许认识 `ui/app/` 与 `ui/widgets/`；
外壳不许认识控件、外观实现与 `ui/app/`，其余模块一律不许 include `ui/shell/`。

允许的"上行"依赖是刻意保留的**依赖倒置**，不是"底层可以随便 include 上层"：
`foundation/core/platform.hpp` 声明 `RenderBackend*`（图形接口由平台注入）、`foundation/core/services.hpp` 汇总
宿主可替换的音频/输入服务、`foundation/core/frame_budget.cpp` 用 `foundation/anim/time.hpp` 的帧时钟、
`foundation/core/platform.cpp` 是平台工厂（方向由「编译期裁剪 + 工厂」反转）。
脚本另外还查一条**伞头可达范围**：`bklv/bklv.hpp` 顺着 include 不许碰到具体后端目录
（`graphics/render/{gl,vk,gles3,libnx}`、`graphics/render/glass/{gl,vk}`）、`system/platform/` 或 `<vulkan/vulkan.h>`
（曾经 GlassService 直接 include `vk_glass.hpp`，于是每个包含伞头的翻译单元都被拖进 Vulkan 头）。

三条约定：

① **加依赖先问方向**：需要下层能力时，由上层注入（接口/回调/注册表），不要让下层认识上层。
   `App::App()` 调 `registerWidgetViewTypes()`（注册表在 core、类型在 widgets）就是标准写法；
   `platforms/*.cpp` 的 `createBackend()` 是工厂式反转。
   两个"曾经违规、现已收口"的范例，改的时候照着走：
   · 液态玻璃：core 只认 `graphics/render/glass/glass_backend.hpp` 的纯虚接口，GL / Vulkan 的实现
     分别由 `OpenGLBackend` / `GLES3Backend` / `VulkanBackend` 持有，`App::run` 用
     `RenderBackend::getGlassBackend()` 转交给服务（`setBackend`，非拥有指针，退出前摘掉）。
   · 视图类型注册：注册表在 core，具体类型在 widgets，由 `App` 组合根注册。
② **存量违规记账在脚本的 `BASELINE` 里**（目前 4 条，逐条写了原因与消解方向，见
   `docs/architecture-review.html` §2.1）。**消解掉一条就要删掉对应行** ——
   留着不删脚本会报"白名单已过期"，和 CMake 源清单漏项检查同一个取向：记账必须精确。
③ 头文件级**没有环**（102 个头 / 206 条 include 边，实测），上面这些环都在
   `.cpp → 对方模块头` 这一层，所以不阻塞编译；代价是模块无法独立编译/分发/测试。

## 全局状态（`foundation/core/globals.hpp`）

进程级状态**只有一处登记**：`bklv::globals::getGlobalStore()`。它解决的是"列举不出、
重置不掉、交代不清"三个问题（评审 §4.2 P0②）。

```cpp
auto& store = bklv::globals::getGlobalStore();
bklv::EventBus& bus = *store.obtain<bklv::EventBus>("eventBus"); // 懒创建（store 拥有）
store.set<bklv::App>("app", app, false);                         // 借来的指针
store.setNumber("myApp.uiScale", 1.5);                           // 数值槽（number/flag/text）
bklv::globals::resetGlobals();                                   // 一轮结束：全清
```

三条约定：

① **新增进程级状态一律登记到这里**，别再写 `static X instance;` —— 那种东西列举不出来、
   `resetAll()` 也清不掉，"换一个 App / 测试之间复位"就永远做不到。
   子系统自己的静态表（纹理仓库的映射表、四个注册表）搬不进表里，就登记**重置钩子**：
   `store.addResetHook("textureStore", &resetTextureStoreForGlobals);`（`resetAll()` 逆序调用）。
② **清空注册表必须连"已注册"标志一起翻回来**：注册表都是"惰性注册内置项"的写法，
   只 `clear()` 不翻标志的话，重置之后内置的 `Box` / `rect` / `text` 会**静默消失**
   （JSON 报"未知 type/op"，比崩更难查）。现有的 `clearXxxForTest()` 都做了这件事。
③ **`resetAll()` 只析构"拥有"的对象**；借用指针（`App` / 平台 / 图形后端 / 玻璃服务）
   只丢引用 —— 谁借的谁保证它活到最后。进程退出时 `GlobalStore` 的析构**刻意不做清理**
   （它最后析构，那时别的 TU 的静态可能已经先没了：踩过测试程序退出即 abort，
   而日志却是全绿的）。要干净收尾就在一切还活着时显式调 `resetGlobals()`。
   时机上注意：纹理仓库的钩子要在**图形后端还活着**时跑（`App::run` 的收尾已经保证了这点）。

现状（别当成"多实例已完成"）：库仍然是"一个进程一套界面"（全局 ImGui 上下文）。
这一层换来的是**可列举 / 可重置 / 可交代**；真要同时跑两个 App 得先定 App 边界的形态，
见 `docs/architecture-review.html` §0 的 §4.2 P0② 一行。

## 帧预算：UI 线程永不卡顿（`foundation/core/frame_budget.hpp` + `foundation/core/frame_stats.hpp`）

目标（用户拍板）：**界面不能卡，任务可以晚**。做法是给"可延后工作"一个每帧时间预算
（`App::setFrameBudgetMs`，默认 **4ms**），用完之后这些活自己停手、留给下一帧：

| 吃预算（可延后） | 不吃预算（这一帧必须做） |
|---|---|
| 后台任务投回主线程的回调、`task::delay` 到期的任务 | 布局（Yoga） |
| 纹理上传、动图换帧、SVG 栅格化 | 视图 `update()` 里的业务逻辑 |
| 纹理缓存淘汰（`trimTextureCache`） | 输入处理与命中、绘制与提交 |

四条约定：

① **可延后的活先问 `budget::isExhausted()`**：超了就 `budget::noteDeferred()` 再 return。
   已经做完的单元不回滚，没做的单元下一帧自然会被重新捡起来。
② **推迟不能丢活**：`pumpMainThread()` 会把没跑的回调放回队首、把到期的延迟任务放回延迟表。
   新加的"可延后"路径必须同样保证"下一帧还会再来一次"——丢了就是丢事件，不是延迟。
③ **别用 `sleep` 或"每 N 帧做一次"来凑**：预算是时间量；`frameskip` 那类做法在掉帧时更糟
   （同一帧里一次做更多）。要限流就限时间。
④ **帧内同步 API 算违规**：`Image::setImageFromFile()` 在帧内被调用会打一条节流警告
   （它把 IO + 解码 + 上传全压在主线程）。批量 / 滚动场景用 `setImageFromFileAsync()`；
   新增耗时入口照这个做法加护栏。

观测（`App::getFrameStats()`）：

```cpp
const FrameTimeSummary s = app.getFrameStats().getSummary();
// p50 / p95 / p99 / 最坏帧；droppedCount = 超过 16.67ms（60Hz 一帧）的帧数
```

每个用 `App::run()` 的程序**退出时自动打一行帧时间流水**（`帧时间：x(p50) / y(p99) / 最坏 z；掉帧 n/m`），
不用自己加统计。`budget::getDeferredCount()` 是这一帧被推迟的工作单元数：长期为 0 说明没有可延后的活
（或预算给太大），长期大于 0 而 p99 仍然好，说明预算正在起作用。

边界（别误解成硬实时）：预算的粒度是"一个工作单元"，**不能打断**正在跑的那一个（一张超大图的
上传、宿主回调里自己做的重活）。单帧仍可能被不可分的单元拖长；要更硬的保证得先把这类单元切细。
改这里要守着三项：状态机、分位数、以及"推迟不丢活"（推掉的单元下帧还在）。

## 截图（离屏，不用系统录屏权限）

`App::requestScreenshot(path, delayFrames)` 会把"再渲染 N 帧之后"的那一帧从后缓冲读出来
存成 PNG，然后退出。**不要用 macOS 的 `screencapture` 做验证**：没给屏幕录制权限时它只能
拍到壁纸（这个坑在验证页面时踩过，白折腾很久）。

实现：`RenderBackend::readPixels()`（OpenGL 后端已实现，读回时上下翻转）+ `foundation/core/image_writer.*`
（stb_image_write，第三方头文件的警告在这一个编译单元里局部压掉）。

**示例里不要加环境变量调试开关**（曾经有 `BKLV_DEMO_PAGE` / `BKLV_DEMO_LOG(_SPAM)` /
`BKLV_DEMO_SCROLL` / `BKLV_DEMO_CLICK` / `BKLV_DEMO_SWEEP` / `BKLV_SCREENSHOT(_FRAMES)`，
已全部删除）。要验收就写一个临时程序（无窗口，自己 include `bklv/` 的头），要截图就在自己的临时程序里调
`App::requestScreenshot()`；产品程序里不留"只有调试时才走"的分支。

## 设计器（独立项目）

设计器已从本仓库移出，改为独立项目：`designer/` 目录、它的代码、状态文件与预设都不在这里了。
两边只靠**资源格式**与**库 API** 对接：

- **资源走原路径**：主题 / 字体 / 文案仍读 `resources/`（只读），不复制、不挪路径。
- **不往库内塞设计器专用分支**：库需要的能力做成通用 API（`GlassService::getCapabilities()`、
  `layer_registry` 的中文名与参数模式、`ThemeData` 按绝对目录装载都是这么来的）。
  判断标准：把设计器删掉之后，这段代码还说得通吗？

## 测试

本仓库**不再带测试目录**（原 `tests/` 已整体移除，`-DBKLV_BUILD_TESTS` 与 `ctest` 目标
也一并删掉）。现在的验收手段是：

- **纯逻辑回归**：写一个临时程序（自己的 `main.cpp` + 链接 `bklv::bklv`），跑完就删，
  不提交回仓库。数据层行为（装载、查找、回退、格式化、缓存、JSON 解析错误）都能这样验。
- **资源按可执行文件目录解析**（见 `bklv/foundation/core/resource.hpp`）：临时程序要先把
  `resources/` 放到可执行文件旁边，或调 `initResources()` —— 不然路径按工作目录解析。
- **本机门禁**：`bash scripts/check.sh`（命名 → 分层 → GL/VK 双后端零警告构建）。

CI 一个平台一份 workflow（`.github/workflows/`），**只编 GL**：

- `static.yml`：接口命名 + 分层/伞头，不编译、不拉子模块；
- `linux.yml`：`ubuntu-latest` + GCC + Unix Makefiles，先装 SDL3 静态构建要的
  X11 / Wayland / 音频 / DRM 开发包；
- `macos.yml`：`macos-latest` + AppleClang，不装任何包（依赖全是子模块，系统框架自己链）；
- `windows.yml`：两个 job —— `mingw`（MSYS2 MINGW64 + 显式 `-G Ninja`，就是 `build.bat`
  那套工具链）与 `msvc`（VS 2022 生成器，专门盯住 `notes/tweeny-msvc-render.patch`
  这条只在 MSVC 下生效的补丁路径）；
- `switch.yml`：`devkitpro/devkita64` 容器 + `scripts/build_switch.sh`，产物 `dist/switch/bklv_switch.nro`。

每个 workflow 自己带一份触发条件与 `concurrency`（YAML 不能跨文件共享 steps，所以重复写）。
构建都走 `scripts/ci_build.sh gl`（参数与「出现 warning 即失败」的门禁只有那一份）；
**Vulkan 版不跑 CI**，本地用 `bash scripts/ci_build.sh vk` 或 `scripts/check.sh` 验（双后端
齐过才算本地通过，CI 只保一份）。本地想复现某个 CI job，就是
`BKLV_CMAKE_GENERATOR=… bash scripts/ci_build.sh gl`（见脚本头注释）。
"零警告"的判定在 `scripts/check_warnings.sh`：只算 `bklv/` 自己写的代码，`bklv/3rd/` 下
vendored 的 submodule（SDL3 / Yoga / imgui / webp / lunasvg）按自己的 `-Wall` 编，报的警告不算。

## 目录

```
bklv/               库本体，自包含：代码 + 3rd/ + 自己的 CMakeLists.txt
  bklv.hpp          伞头文件：常用类型一次引入（示例只用它；按大类分段）
  foundation/       基础：不认识任何界面概念
    core/           生命周期 / 事件 / 日志 / 资源 / 任务 / 服务 / 平台抽象
                    alive / cache / event / globals / logger / services / resource
                    / file_system / task / frame_budget / frame_stats / decode_queue
                    / ui_scale（主题按 720p 写、运行时等比放大）/ i18n / platform.*
    geom/           几何与颜色换算：geometry / transform
    json/           JSON 读取工具：json_util（取值/错误）/ json_vars（变量块）/ json_policy
    anim/           时间源、缓动、过渡与补间：time / transition / transition_player
  graphics/         绘制：只认 ImDrawList 与图形接口
    render/         接口 graphics.* + gl / vk / gles3 / libnx 四个后端；glass/ 玻璃子系统
    paint/          绘制效果：gradient / shadow / focus_ring
  ui/               界面：视图树、外观、控件
    view/           View / Box / 视图树 / JSON 视图：view / box / json_view / json_data
                    / json_actions / view_parse
    theme/          外观数据模型与解析：appearance（外观编译）/ theme_data / attributes
                    / layer_registry（层类型表）/ appearance_{box,gradient,layer_fields,layer_meta}
    layer/          层绘制实现：layer_draw_{custom,glass,image,rect,shadow,text} + 注册模板
    widgets/        外观驱动的控件
      appearance/   appearance_view / appearance_children / widget_types（JSON 视图类型注册）
      basic/        label / image / padding / progress_spinner
      container/    scrolling_frame / h_scrolling_frame / scroll_box
      controls/     button / notification
      list/         recycler / json_cell / data_source / item_binder
      debug/        debug_layer / debug_log_view / fps_overlay / preset_backdrop
      view_types.cpp 内置视图类型（Box/Row/Label/Image/...）的注册实现
    input/          键盘 / 手柄 / 手势 / 输入法 / 模态栈
    app/            组合根与上下文：app.cpp（帧循环/主题/弹层）、context.*
  system/           运行时支撑
    media/          字体 / 图标 / 图片解码（含 gif、svg、webp）/ 图片写出 / 音频 / 音效表
    platform/       平台实现：sdl3_platform / sdl3_input / sdl3_window / switch_*
  3rd/              vendored 依赖：imgui / yoga / tweeny / SDL / stb / json
                    / webp（可选）/ lunasvg + plutovg（可选，SVG）
  cmake/            安装用的包配置模板（bklvConfig.cmake.in、SDL3Config.cmake.in）
  CMakeLists.txt    编出 bklv::bklv（INTERFACE 目标，见上）
resources/theme/    主题：themes.json + <主题>/{theme.json, tokens/, base/, base/widget/, view/}
                    （名字 = 路径去扩展名；文件清单提前写在 theme.json 的 files[] 里）
demo/               演示程序（最小框架）：外壳 + 页面栈 + 首页 / 设置页
examples/           宿主示例：embed_imgui_gl（嵌进宿主循环）/ multi_context / switch / cmake_consumer
CMakeLists.txt      仓库入口：判定平台、定输出目录、编 demo/ 与 examples/
scripts/            脚本：check.sh（本机门禁）、build_switch.sh（Switch）、命名/分层检查与文档生成
docs/               速查文档：bklv 文件职责、API 用法、已知的坑
notes/              设计笔记与移植评估
resources/          运行期资源：字体 / 文案 / 主题 / 示例用的图片
                    （桌面复制到可执行文件目录，Switch 打进 romfs）
```

`bklv/` 要能被整个目录拷到别的项目里用，所以**它自己的构建逻辑必须留在
`bklv/CMakeLists.txt`**，不要往根 `CMakeLists.txt` 里加库相关的东西。

README 只讲项目介绍、拉取编译、怎么用到别的项目里；**代码层面的说明（文件职责表、
API 速查、踩坑记录）写在 `docs/`**，别往 README 里堆。

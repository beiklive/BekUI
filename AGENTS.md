# AGENTS.md — 本项目的开发约定

## 图形后端：demo 一律双版本

**每个 demo 都要能编出 GL 和 Vulkan 两个版本**，不是二选一。

原因：GL 和 Vulkan 是两条完全不同的渲染路径（上下文创建、shader、资源生命周期都不同），
只在一个后端上验证过的界面代码不算验证过。双版本让「上层与图形 API 无关」这条设计
随时可被检验——如果某个 demo 换后端跑不起来，说明它漏了抽象。

### 构建

构建入口只有一个：根目录的 `CMakeLists.txt`。后端是编译期决定的，所以
「每个 demo 都能编 GL 和 Vulkan 两版」靠**两个构建目录**实现。
仓库根有 `CMakePresets.json`，CLI 与 IDE 都用它（**不要再手写 `-B build`**）：

```bash
# GL
cmake --preset gl && cmake --build --preset gl      # 等价 -B build/gl -DBKLV_GRAPHICS=gl
ctest --preset gl

# Vulkan
cmake --preset vk && cmake --build --preset vk      # 等价 -B build/vk -DBKLV_GRAPHICS=vk

# 不想用 preset 时手写也行（效果一样）
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl
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

- **`<仓库>/build` 本身不是构建目录**：我们的构建目录是 `build/gl` / `build/vk`，
  `build/` 只是父目录。VS Code 的 CMake Tools 默认去构建 `build/`，会报
  `not a CMake build directory (missing CMakeCache.txt)` —— 仓库里的
  `.vscode/settings.json` 已经把它指向 `gl` preset（`cmake.useCMakePresets: always`），
  手动选的话是 `F1` → `CMake: Select Configure Preset` → `gl`。
- **多个构建目录共用同一个 `dist/`**：CMake 的输出目录固定是 `${CMAKE_SOURCE_DIR}/dist`
  （VK 是 `dist/vk/`），所以任何额外构建目录（`build/asan`、`build/dbg`、`/tmp/xxx`）
  都会把可执行文件写进同一个 `dist/`。它们的产物时间戳比 `build/gl` 的目标文件新，
  make 会认为"已是最新"而**不重新链接** —— 于是你以为在验收 A 配置，实际跑的是 B 的二进制
  （这次踩到：用 `-DBKLV_WITH_WEBP=OFF` 在 /tmp 配了一次，`dist/` 里就变成"没有 WebP"的版本，
  check.sh 的检查项数也跟着少了一截）。跑完额外构建目录后要
  `rm -f dist/*_test dist/demo dist/designer && cmake --build build/gl -j 8` 强制重链。
- 清单漏项在 configure 阶段就会报错（`bklv/CMakeLists.txt` 查 core/views/widgets，
  根 `CMakeLists.txt` 查 demo），不用等链接期。
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
| `bklv/3rd/stb` | 位图解码（PNG/JPEG/BMP）+ GIF（含动画） |
| `bklv/3rd/webp` | WebP 解码（静态 + 动图），可选：`BKLV_WITH_WEBP` |
| `bklv/3rd/lunasvg` + `bklv/3rd/plutovg` | SVG 栅格化，可选：`BKLV_WITH_SVG` |
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
- **Label 的宽度比较都留了 0.5px 容差**（`views/label.cpp`）：文本宽度由测量函数算、
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
- **主题引用的写法是 `@theme/color.a.b` / `@theme/metric.a.b`**（点是分层分隔符，
  对应 `colors.json` 的 `a.b`）。写成 `@theme/color/a.b` 会被容错成点分
  （带一条警告提示正确写法），
  **但真正不存在的 key 仍然编译失败**（不静默回退成洋红），
  theme_test 第 8 节逐个编译两套主题的全部外观兜底。
- **布局是延迟的，一帧最多算一次**（`core/view.cpp` 的 `invalidate()` + `core/app.cpp`）：
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
- **UI 尺寸按 720p 写**（`core/ui_scale.*`）：主题里所有尺寸/字号都按 **1280x720** 写
  （和 borealis 的 `ORIGINAL_WINDOW_WIDTH/HEIGHT` 一致），运行时 `uiScale = 画布宽 / 1280`。
  缩放点在**取值**这一层：
  ① `ThemeData::number()` 里 `metric.*` 会乘缩放（`metric.anim.*` 是毫秒、
     `metric.font.lineHeight` 是系数，都不缩放）；
  ② 外观 JSON 里的 radius/strokeWidth/width/height/fontSize/inset/offset 会乘缩放
     （opacity 是系数、duration 是毫秒，不缩放）。
  代码里写死的像素值**不缩放**——想跟随比例就用主题 metric 或把数字写进外观 JSON。
  画布尺寸变了要重新套用外观（外观值在 `applyTheme()` 时就取定了）并重排，
  这个由 `App::frame` 里调 `updateUiScaleFromCanvas()` 统一处理。
- **外观层序：高亮底必须在内容层之前**（`hlBg` 要排在 `text`/`image`/`custom` 之前，
  `hlGlow`/`hlBorder` 排在最后）。排反了的话，聚焦时那块**不透明的高亮底会把内容盖住**
  —— 开关/单选的图形是 `custom` 层，曾因此"聚焦后开关消失"（14 个 Cell 外观全踩了）。
  `theme_test` 现在会遍历两套主题的每一份外观断言这个顺序，别再犯。
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
- **控件外观要还原 borealis 的焦点高亮**：每个可聚焦控件的外观里有三层高亮
  （`hlBg` / `hlGlow` / `hlBorder`），focus 态把它们补到
  `color.highlight.background`、`color.highlight.glow`、`color.highlight.border`，
  描边宽度 `metric.highlight.strokeWidth`（5，borealis 的 `brls/highlight/stroke_width`）、
  圆角 `metric.highlight.radius`（6），并且**向外扩 2.5px**（inset 负数）。
  顺序是：自己的底 → hlBg → 内容 → hlGlow → hlBorder（和 borealis 的
  "高亮底画在内容之前、描边画在内容之后"一致）。
- **显式设置优先于主题/外观**（`setCornerRadius(r, explicit=true)`）：
  代码里显式设过的值，外观 JSON 不能覆盖它；外观套用一律走 `explicit=false`。
  Label 的 `setFontSizeExplicit` / `setTextColorExplicit` 是同一套约定的老成员。
  踩过的坑：外观 JSON 里写了 `radius`，代码里 `setCornerRadius(60)` 就完全不起作用
  （想画圆形头像，画出来是圆角方块）。
- **`update()` 路径不许读 `ImGui::GetIO()`**：无窗口（测试）时没有 ImGui 上下文会直接空指针。
  时间统一用 `core/time.hpp` 的 `getFrameDeltaSeconds()`；确实需要鼠标状态的地方
  用 `ImGui::GetCurrentContext() != nullptr` 判一下再取（`ScrollingFrame`/`HScrollingFrame` 就是这么做的）。
  这个坑在 `Animatable`、`ScrollingFrame`、`ProgressSpinner` 上先后踩过三次。
- **回收列表可以分段**（`views/recycler.*`）：单段只重写 `getItemCount()`；
  分段重写 `getSectionCount()` + `getItemCountInSection()` / `getSectionTitle()` /
  `getItemIdentifierInSection()`。**段标题返回空串表示这一段不占表头行**。
  接口刻意不叫 `getItemCount(section)`（borealis 的重载写法）——派生类只重写一个
  会把另一个名字隐藏掉，属于典型 C++ 坑。段头 cell 继承 `RecyclerSectionHeader`，
  标题通过虚函数 `onSectionTitle` 下发（Switch 带 `-fno-rtti`，不许用 `dynamic_cast`）。
- **枚举值大小写与 `_`/`-` 都不敏感**（`enumMatches`，两套 JSON 一致）：
  `flexEnd` == `flex_end` == `FLEX_END`。视图描述 JSON 里拼错 → 不建控件 + 报错（列出可选值）；
  外观 JSON 里拼错 → 警告 + **保持原值不动**（`justifyFromName` 这类返回 bool，别退回默认值）。
  加新枚举字段时用 `enumMatches` + `warnUnknownEnumValue`，别写 `== "flexEnd"` 这种精确比较。
- **JSON 语法细节查 `docs/json-syntax.md`**（四类 JSON 的完整键表 / 取值 / 枚举拼写 / 报错行为）；
  下面几条是**约定**，别改错了。
- **控件的样式全在 JSON 里**（`resources/theme/<主题>/base|widget/<控件>[.<变体>].json`）：
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
  **必须写在自己的底色之前**（`theme_test` 有断言），绘制档位由全局开关
  `bklv::shadow::setQuality(FAST/SMOOTH)` 或 `BKLV_SHADOW_QUALITY=smooth` 决定。
  ③ `states`：状态动效。
  **控件在代码里只写行为**（有哪些子节点、数据从哪来、语义绘制）；
  尺寸/间距/颜色/圆角一律从 JSON 来。容器类控件（不是 AppearanceView 的）
  调 `applyWidgetBox(this, "<控件名>")` 也能吃到同一份 box 段。
  同一控件要多套尺寸就加变体文件（如 `header.applet.json`、`tab_cell.sidebar.json`），
  代码里用 `setAppearanceVariant("applet")` 切，而不是在代码里改高度。
- **主题只有一套：JSON 文件**（`core/theme_data.*`）。没有并存的 C++ 主题表了——
  视图 JSON 的 `@theme/`（取色，走 `color.*`）与 `@style/`（取数值，走 `metric.*`）
  和外观 JSON 的 `@theme/...` 解析的是同一份数据；前缀可省略
  （`@theme/accent.base` == `@theme/color.accent.base`），key 不存在会**报错**而不是回退成洋红。
- **主题与外观**（`core/theme_data.*` + `core/appearance.*` + `widgets/`）：
  换肤 = 复制 `resources/theme/default/` 改名 + 改 JSON，**不改 C++**。
  四条约定：
  ① **不扫目录，清单提前写**：`themes.json` 列主题；各主题 `theme.json` 的 `files[]` 列
     本主题的全部文件。目录分层与名字规则：
     ```
     <主题>/theme.json        元信息（name/author/description）+ files[] 清单
     <主题>/tokens/           colors.json / metrics.json（色板与数值）
     <主题>/base/             基础控件外观（单个容器/元素）
     <主题>/widget/           二级控件外观（引用 base 组合出来的）
     <主题>/view/             页面与视图描述（结构 + 外观）
     ```
     **名字 = 相对主题根的路径去掉扩展名**（`base/button.json` → `"base/button"`），
     变体继续用后缀拼（`base/button` + variant `"primary"` → `base/button.primary`）。
     挪文件 = 改名字，C++ 里的引用（`AppearanceView("base/button")`、
     `applyWidgetBox(this,"widget/sidebar")`）要一起改。
     视图描述用 `createViewFromTheme("page_json")` / `loadViewTemplateFromTheme(...)` 装载
     （主题内 `view/`，缺了回退 default 主题）。
  ② **装饰层走 JSON，语义绘制留 C++**：底/边框/文字/图片/状态动效由 `layers` 描述；
     Label 的换行、Image 的缩放、进度弧、滑块轨道走 `custom` 层
     （`AppearanceView::drawCustomLayer`）。把语义绘制也塞进 JSON 等于写解释器，别做。
  ③ **draw 里不查主题**：外观值在构造 / `applyTheme()` 时取到 `AppearanceInstance`，
     绘制只读当前值。换肤靠 `App::setTheme()` → 重新取外观 + `applyThemeRecursive()` + 重排。
  ④ **主题只覆盖未被显式设置过的值**：调过 `setFontSize/setTextColor` 的控件，
     换肤不再动它；要跟随主题就 `setXxxExplicit(false)`。
  ⑤ **装饰件用显式 `width`/`height`**：层默认铺满"控件范围按 inset 收缩后"的区域，
     给了 `width`（如色条 4）或 `height`（如下边线 1）就按 `align`/`valign` 摆放。
  ⑥ **`import` 让外观引用外观**（`"import": "base/box.component"` 或字符串数组）：
     逐字段合并、**当前文件优先**——对象递归合并，`layers` 按 `id` 合并（同 id 的层合并字段，
     新 id 追加），其它数组整体替换，标量覆盖。所以"改 base/box.component 的边框，
     所有 import 它的按钮一起变"，而按钮自己写的字段仍然赢。
     循环引用、深度 > 8、父级不存在都会**报错**（不静默降级）。
  ⑦ **一行里放多个内容层要用 inset 排队，并写 `measureContent: "sum"`**：
     绘制层不是流式布局（`align: left` 的两个层会叠在左边，参考 `base/hint_item` 给
     文字层写 `inset: [0,0,0,34]` 跳过图标）；默认固有尺寸取"最宽的一层"，
     一行里的图标 + 文字 + 信息必须按行**求和**才不会溢出（`measureContent: "sum"`）。
  缺项回退链：当前主题 → default → 控件空外观（警告，不崩）。
- **文本输入（IME）**（`core/ime.*` + `widgets/edit_text_dialog.*`）：
  平台决定走哪条路——`ImeManager::open()` 返回 true 表示平台接管
  （Switch 用 libnx 的 `swkbd` 弹系统软键盘，同步阻塞，主循环会停住），
  返回 false 表示没人接手，控件自己读键盘（桌面：ImGui 字符队列 + Backspace/Del/方向键/回车/Esc）。
  **编辑逻辑放在 `TextEditModel`（纯数据、不碰 ImGui）**，所以能无窗口测；
  光标按**字符**走（中文/emoji 不会被切坏），闪烁在绘制期按时间算、不进模型。
- **裸 `AppearanceView` 也会在构造时 `applyTheme()`**：不这么做的话，
  直接 `new AppearanceView("xxx")` 的地方会拿到空外观，一笔都画不出来
  （EditTextDialog 的遮罩/面板踩过：结构对、颜色对，就是什么都不显示）。
- **转场：入场 / 出场分开**（`core/transition.hpp` 的 `ViewTransition`，
  `widgets/shell.*` 的内容栈与 `widgets/pager.*` 的同级换页共用它）。
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
- **异步回调要带生命周期护栏**（`core/alive.hpp`）：`task::run*` 的 `done` 在主线程执行，
  但发起时捕获的 `this` 可能已经析构（换页、重建视图树、回收器释放）→ 裸捕获就是
  use-after-free。做法是成员放一个 `AliveFlag`，回调捕获 `alive.handle()`（`weak_ptr`），
  进来先 `if (guard.expired() || !guard.lock()->isAlive()) return;`
  （borealis 用 `ASYNC_RETAIN`/`ASYNC_RELEASE`/`ASYNC_TOKEN` 三个宏手写引用计数做同一件事，
  这里换成无宏的小对象）。`Image::setImageFromFileAsync` 已经这么做了，有测试覆盖
  "视图先销毁、回调后到"。
- **延迟任务**（`core/task.hpp` 的 `task::delay(ms, cb)`）：cb 由 `pumpMainThread()`
  （`App::frame` 每帧）在**主线程**执行，不占线程、不用额外定时器；`DelayHandle` 拿来
  显式 `cancel()`（重复触发前先取消上一个，就是"重置计时器"的写法）。
  **析构不取消**：`task::delay(1000, [] {...});` 这种 fire-and-forget 必须能生效
  （当初做成"析构即取消"，临时句柄在语句结束就销毁 → 回调静默不触发，测试抓到的）。
  回调里碰视图照样按上一条加 `AliveFlag` 护栏。
- **事件总线**（`core/event.hpp`）：`getEventBus().subscribe<事件类型>(...)` 返回 **RAII 句柄**，
  析构自动退订（换根/关页面时也可以 `clear()`）。框架自己发 `FocusChangedEvent` 与
  `ActionTriggeredEvent`。
  **不许用 `typeid`/`std::type_index`**（Switch 带 `-fno-rtti`）——总线内部用
  "每个事件类型一个静态 token，取地址当 key"；派发期间增删订阅按快照处理。
- **弹层与模态输入**（`core/input.cpp` 的模态栈 + `core/app.cpp` 的 `pushOverlay`）：
  `App::pushOverlay(view)` 把弹层挂到根视图最上层并切模态；`popOverlay` 摘下并恢复焦点。
  三条：
  ① **模态生效时指针命中从模态根开始**（点到外面等于点空白，不穿透），
     方向导航也会被挡在模态外（`isInsideModal`）。
  ② **App 销毁 / 换根视图时必须 `resetFocusState()`**：模态栈是进程级静态，
     根视图一删里面的指针就是野指针，之后的导航/命中会踩崩（测试里踩过）。
  ③ **关掉弹层要等动效播完再摘**：摘掉就脱离视图树，`update()` 不再跑，
     关闭动效根本播不出来。Dialog 是"先切状态、播完再 popOverlay"。
- **状态动效的取消要撤掉旧补间**（`core/appearance.cpp` 的 `setState`）：
  重算目标时**先撤该通道正在跑的补间，再决定要不要建新的**。
  不能等"目标与当前值不同"才撤——刚打开又马上关掉时当前值还等于初值，
  旧补间会被漏掉、继续往旧目标跑（弹窗面板整块补到透明就是这么来的）。
- 不生成 `imgui.ini`：绘制全在 viewport 绘制列表上，`io.IniFilename` 置空
- 日志用 `BKLV_LOG_INFO(...)` 等宏（带 文件:行号）。日志同时进控制台与
  `Logger` 的环形缓冲，界面上的日志窗口（`views/debug_log_view.hpp` 的 `DebugLogView`）读缓冲渲染。
  它**是视图树里的 View**（挂在外壳的调试层 `Shell::getDebugLayer()` 上，在 HudLayer 之上、
  永远最后绘制），直接用 `ImDrawList` 画文本、**不为每条日志建视图**，所以日志再多也不重排。
  **不要**为每条日志创建 View —— borealis 那样做会不断触发 Yoga 重排。
  **不支持滚动**：永远显示最新的一批（"即时显示"），要翻历史就开 `Logger::setFileOutput()`
  去文件里看 —— 所以没有 scrollTo/scrollBy/滚轮那一套（`getFirstVisibleRow()` 永远 = 总行数 − 可见行数）。
  它的绘制开销是**硬要求**，四条约束（`tests/log_view_draw_test.cpp` 钉住）：
  ① 缓存里只存 `const LogEntry*`（不复制日志正文），只在「版本/过滤/字号/字体」变化时重建；
  ② 行高固定 + 不滚动 → 可见窗口就是"最后 N 行"，一次除法算出来，不从头遍历；
  ③ 每行只一次 `AddText`，拼接走复用缓冲（稳态零分配）；④ 面板不可见时 `View::frame` 直接返回。
  实测：30 条与 1000 条日志的顶点数/命令数**完全相同**（162/3），一帧约 2 微秒。
- ImGui 窗口在 `App::onDrawWindows` 里下发（视图树画完之后、`ImGui::Render()` 之前）。
  现在只有设计器还在用这条路（它自己的界面就是 ImGui）；demo 的日志窗口已经是视图了。
- **按键重复（长按连续移动）**：`core/input.hpp` 的 `KeyRepeater`——首次按下**立刻**响应，
  按住则等 `repeatDelay`（默认 250ms）后每 `repeatInterval`（默认 100ms）一次，
  和 borealis 的 `BUTTOM_REPEAT_TRIGGER` / `BUTTON_REPEAT_DELAY` 一致。
  时间来自 `getFrameDeltaSeconds()` 累加，键盘/手柄/各平台共用一套，不用平台 API；
  A/B 也走同一套（borealis 的 A/B 会重复）。
  **一个物理键一个重复状态**：以前两个键（方向键 + WASD）共用一个"边沿状态"，
  互相把对方清掉 → 按住方向键每帧都判成"刚按下"，导航飙到帧率。
- **页头与底部栏是外壳级的（全局），内容避让是显式的**（`widgets/shell.*` + `widgets/pager.*`）：
  宿主自己建 `Header` / `BottomBar` 交给 `Shell::setHeaderView/setFooterView`（挂覆盖层，
  中间用 `Padding` 顶开），页面再按 `getHeaderInset()/getFooterInset()` 自己留白。
  其中 `Header = [图标][标题][右侧区]`（widget/header*.json + base/header_icon.json），
  `BottomBar = [提示(左) | 状态区(右)]`（widget/bottom_bar.json），
  Header 的左右外边距用 `appletFrame.paddingSides`、底栏用 `hints.footerMarginSides`。
  显隐用**实例方法** `Shell::setHeaderVisible(bool)` / `setFooterVisible(bool)`：隐藏时看 `GONE`
  （不占位）；**栏里不能留固定外边距**（页头下边的留白写在页头自己的 padding 里），
  否则隐藏后会留下空隙。
  **content 避让不自动**：`Shell::setHeaderInset/setFooterInset` 由宿主给（动画期间给**目标值**，
  这样内容只重排一次），宿主再把 insets 加到内容层的 padding 上（demo 就是这么做的）。
  栏的"折叠补间"是**宿主**的事：`Animatable` 逐帧改两栏高度 + `App::requestLayout()`，
  补间结束才真正 GONE（demo/app/demo_app.cpp 里有一份，库侧不再提供）。
- **占位挤压用 `Padding`**（`views/padding.*`，对标 borealis 的 `brls:Padding`）：
  空视图、auto 尺寸、`grow=1`，把后面的兄弟挤到另一端；页头就是
  `[图标][标题][Padding][右侧区]`。JSON 里写 `{ "type": "Padding" }` 也能用。
  比"给某一侧加 grow + justifyContent"更直白，也不吃焦点。
- **加插值属性要往 `ALL_PROPS` 补一条**：`PROP_COUNT` 是从 `ALL_PROPS` 的表长**推导**的
  （以前写死 13，加两个属性后 per-layer 数组少两格，越界写把堆写坏、崩在 malloc 里）。
  加 `AppearanceProp` + `ALL_PROPS` + `PROP_NAMES` + 每层初值 + `getNumber` 回落，
  五处要同步，`static_assert` 与顺序自检会兜住。
- **`shadow` 的 `cutout`**：打开后只画外圈（把控件自身矩形挖掉，borealis 用 `NVG_HOLE`
  做同一件事），内圈一个顶点都不落 → 后面加的层即使是**半透明底色也不会透出阴影**。
  实现是沿中线描一圈（`PathRect` + `PathStroke`），`shadow::drawSoft(..., cutout)`。
  默认关，按需在 shadow 层写 `"cutout": true`。
- **测试里要画东西就用真 ImGui 上下文**，三件事缺一不可（shadow_test 三个坑都踩过）：
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
- **下拉弹窗（`widgets/dropdown.*`）**：点触发器 → `App::pushOverlay(DropdownPopup)`，
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
  外层 `justifyContent=spaceBetween`）。`widget_test` 里有断言盯住这个顺序。
- **有外边距就别写 `width: 100%`**：`100% + margin` 会多出边距那一段（页头/底栏右边被吃掉、
  内容的右上按钮被推出边界，都踩过）。要"填满父容器再留边距"就用 **auto 宽 + margin**，
  父容器负责 stretch（Shell 的内容层／宿主容器自己 `alignItems: stretch`）。
- **滚动容器用 `flex-basis: 0` + `flex-shrink: 1`**（`views/scrolling_frame.*`）：
  Yoga 的 flexShrink 默认是 **0**，而 basis 默认是内容尺寸。两者不改的话，内容一高，
  Yoga 会按比例把**同一列里的兄弟节点一起压缩**（按钮行 48 → 28），滚动器自己的高度
  也随内容变 → 每次切页内容区高度都变，看着像底栏在上下闪。
  `grow` 配 `basis 0` 才是"只占剩余空间"。
- **图标有两套，"全部字形"要扫字体而不是列手写表**（`core/font.*` + `core/icon_font.*`）：
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
- **图片解码的统一契约**（`core/image_decoder.*`）：对外只有 `decodeImageFile/Memory`
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
- **组合按钮与选择器**（`widgets/button.*` + `widgets/selector_button.*`，
  外观 `widget/button.{text,icon,icon_text,dropdown,switch_text,switch_icon,info,selector}`）：
  1）**变体 = 组合外观**，代码只给内容和行为。`AppearanceView::setAppearanceName(name)`
     换基名（名字是主题里的完整路径，如 `widget/button.icon_text`）；一行放置靠层 inset。
  2）内容槽在 `AppearanceView` 上（不是 Button）：`setIcon()`（图标字形）与
     `setInfoText()`（右侧信息）；`getLayerText` 认 `"text"/"icon"/"info"`，
     `getLayerFont` 对 id/source 为 `icon` 的层返回图标字体（用正文字体会把图标画成方块）。
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
- **demo 页面的控件一行一个**：别把好几个按钮塞一行（挤在一起会截断信息文字）。
  用"一行 + 控件 `grow=1`"铺满；容器/按钮的宽度靠 `measureContent: "sum"` 撑开，
  不要靠手写固定宽度。
- **页面里不要再套 `ScrollingFrame`**（demo 约定）：非自滚动页（`selfScrolling=false`）由
  宿主的外层滚动器负责滚动，页面的外层框是 `height: auto` —— 里面再放一个
  `grow/shrink` 的滚动器拿不到确定高度，高度会算成 0，**整页内容都看不见**（图标总览页踩过）。
  要页面自己滚就把 `addPage(..., selfScrolling=true)`，宿主会按滚动器实测高度补外层框高度。
- **页面在 `App::run()` 之前建 = 字体还没装**：`App::run()` 里才建图集并 `loadFonts()`，
  而视图树（`setRootView`）在那之前就建好了。所以任何"构建页面时就去查字体/图标表"的代码
  在首次构建时都会拿到空表（图标总览页曾经整页空白）—— demo 因此把**起始页推迟到第一帧**再选
  （`pendingStartPage` 在 `update()` 里消费）。
- **`AppearanceView` 是容器，`update()` 必须先 `Box::update()` 递归子树**：
  底栏里的提示栏/状态区、页头里的标题、弹窗面板里的按钮行都靠这条，
  漏了的话子树永远不会 update（踩过：提示栏一直不刷新、看着像空的）。
- **提示栏用控件而不是覆盖层**（`widgets/hints_bar.*`）：它是底栏的左侧内容，
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
| 值访问器（返回数据） | `get*` | `getWidth` / `getStr` / `getBool`（数据取值，不是状态查询） |
| **动作返回是否成功** | 保留动词式（不加 `is`） | `initContext` / `open` / `pushOverlay` / `step` / `recognize` |
| 布尔开关 | `set<名词>Enabled` / `set<名词>Visible` / `set<名词>Animated` | `setBarsAnimated` / `setHeaderVisible` / `setBatteryVisible` |
| 回调 | `setOnXxx` | `setOnClick` / `setOnChange` / `setOnSubmit` |
| 容器操作 | `addView` / `removeView` / `pushContentView` / `popContentView` | 与容器语义一致 |
| 子类扩展点 | `applyTheme` / `drawCustomLayer` / `getLayerText` / `getLayerImage` / `getDefaultFocus` | 外观与交互的固定钩子 |
| 自由函数分组 | 同一主题的散函数放同名 `namespace` | `bklv::task::run` / `bklv::audio::play` |
| 文件 | snake_case；一个控件一对 `.hpp/.cpp`（同族小类可合并，如 `cell.*`） | `scrolling_frame.*` / `cell.*` |

例外只有两种，都写进注释：① STL 风格的 `std::string::empty()` 这类**标准库**成员照旧；
② 数据取值用 `get*`（`getBool`），即使返回 bool 也不算状态查询。

## 截图（离屏，不用系统录屏权限）

`App::requestScreenshot(path, delayFrames)` 会把"再渲染 N 帧之后"的那一帧从后缓冲读出来
存成 PNG，然后退出。**不要用 macOS 的 `screencapture` 做验证**：没给屏幕录制权限时它只能
拍到壁纸（这个坑在验证 demo 页面时踩过，白折腾很久）。

实现：`RenderBackend::readPixels()`（OpenGL 后端已实现，读回时上下翻转）+ `core/image_writer.*`
（stb_image_write，第三方头文件的警告在这一个编译单元里局部压掉）。

**demo 里不要加环境变量调试开关**（曾经有 `BKLV_DEMO_PAGE` / `BKLV_DEMO_LOG(_SPAM)` /
`BKLV_DEMO_SCROLL` / `BKLV_DEMO_CLICK` / `BKLV_DEMO_SWEEP` / `BKLV_SCREENSHOT(_FRAMES)`，
已全部删除）。要验收就写 `tests/` 里的无窗口测试，要截图就在自己的临时程序里调
`App::requestScreenshot()`（设计器就是这么用的）；产品程序里不留"只有调试时才走"的分支。

## 设计器（designer/）

第二个程序（`dist/designer`），**界面本体用 ImGui 控件、预览用 bklv 的真实视图**渲染，
和 demo 互不依赖。逻辑层 `designer/core` 不含界面代码，能无窗口测
（`tests/designer_core_test.cpp`）。详细用法见 `docs/designer.md`。

- 构建：`BKLV_TARGET_LIST` 里一行（`"designer|designer/main.cpp|designer"`）+ 根
  `CMakeLists.txt` 的 `BKLV_DESIGNER_CORE_SOURCES` / `BKLV_DESIGNER_UI_SOURCES`；
  漏项检查覆盖 `demo/*.cpp` 与 `designer/*.cpp`。
- 主题可以是**别的仓库**里的：装载走 `loadThemeIntoGlobalFromDirectory`（按绝对目录），
  不是只认 `resources/theme/<name>`。
- 预览是"手动驱动一棵不在 App 视图树里的视图"：`YGNodeCalculateLayout` → `notifyLayout`
  → `update` → `setTranslation(画布原点)` → `frame(ctx)`；`View::draw` 是 protected，
  别想直接调它。
- `readOnly` 的基准主题（default / ocean）只能浏览，编辑请走"模板新建"。
- **ImGui 的默认字体必须显式设**（`setDefaultFont` 里设 `io.FontDefault`）：不设就用
  字体表第 0 个，而那可能是先加载的图标字体（只有私用区字形）→ `ImGui::Text` 全画不出来。
- **截图按帧缓冲读**（`App::saveScreenshot` 乘 `io.DisplayFramebufferScale`）：Retina 上
  帧缓冲是逻辑尺寸的 2 倍，按逻辑尺寸读只拿到左下角一块。
- 设计器里要关掉 ImGui 自己的键盘/手柄导航（`io.ConfigFlags &= ~NavEnableKeyboard`），
  否则方向键会被 ImGui 拿去移动它的内部焦点、把面板滚到不知哪里去。

- **液态玻璃只服务设计器外壳，预览区是禁区**（`designer/glass/` + `designer/ui/designer_glass.*`）：
  1）**只在 GL 下启用**：后端是 GLSL 150（`designer/glass/gl/`），跑在 Vulkan 构建里时
     `beginFrame` 会看到后端名不是 GL 就整体关掉（没有 GL 上下文还去调 GL 会崩）。
     拿不到 GL 入口 / 着色器编不过 / 尺寸非法 → 回退普通面板，设计器不会因此不能用。
     开关：`BKLV_DESIGNER_GLASS=0`。
  2）**预览区的像素一个都不许被玻璃影响**：预览面板的矩形进 `GlassRequestQueue::excludeRect()`，
     玻璃请求与它相交时会被裁到"避开的更大那一半"；另外横条不画外阴影
     （阴影会把绘制范围外扩几像素、渗进预览面板）。
     验收方式：开/关玻璃各截一张，**预览面板区域逐像素一致**（实测 0 像素差）。
  3）**采样的是"本帧已经画好的内容"**：所以先在最底层画壁纸（渐变 + 柔光 + 斜条纹，
     纯色底上玻璃看不出来），玻璃面板压在上面。与源项目（Android 覆盖层，画之前抓帧缓冲）
     不同，抓取放在**第一条玻璃命令**里，一帧只抓一次。
  4）**GL 状态必须还回去**：抓背景/模糊都画到离屏 FBO 上，而 framebuffer 绑定是全局状态 ——
     不恢复的话后面所有 ImGui 命令都画进离屏 FBO（整屏四分之三会变透明，这次踩到）。
     `designer/glass/gl/gl_backend.cpp` 里用 `FramebufferGuard` 兜住；
     玻璃命令后面还要插一条 `DrawCallback_ResetRenderState` 让 ImGui 接着画。
  5）**不引入构建期工具**：着色器是 GLSL 150 常量字符串（`gl_shaders.cpp`），
     公共模块（blur_kernel / glass_math / glass_color / glass_lighting）直接内联，
     不需要 glslc / SPIR-V 生成步骤。着色器改自 MIT 项目，版权声明留在文件头。
  6）库侧只加了一个通用钩子：`App::onPrepareDrawData`（ImGui::Render() 之后、
     renderDrawData() 之前），设计器用它注入玻璃命令；库里没有任何玻璃代码。
  7）**参数可调且会存**：`designer/glass/glass_tuning.hpp` 里那份 `GlassTuning`
     是唯一数据源 —— 工具菜单「玻璃调参」/ `F4` 打开滑块面板（面板自己也是玻璃），
     参数随 `designer_state.json` 一起存（`ui_state.glass`）。
     调参面板/日志这类**浮窗**底给 0.35 的 alpha 再叠玻璃：玻璃在预览区会被禁区裁掉，
     那块得有底色才看得清字。
     自查开关：`BKLV_DESIGNER_GLASS=0` 关玻璃、`BKLV_DESIGNER_GLASS_PANEL=1` 开局就展开调参面板
     （**必须在 loadUiState() 之后生效**，否则被状态文件盖掉）。

## 测试

无窗口测试在 `tests/`（只断言数据层面行为，不开窗口）：

```bash
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl -DBKLV_BUILD_TESTS=ON
cmake --build build/gl -j 8
./dist/i18n_test          # 或 cd build/gl && ctest
```

产物与 demo 同目录（`dist/`）——资源路径按可执行文件目录解析（见
`bklv/core/resource.hpp`），测试要能读到 `resources/`。
加测试只需在 `tests/` 放一个 `*_test.cpp`，会被 CMake 自动收集；
断言脚手架在 `tests/test_util.hpp`（`check` / `checkEqual` / `checkFloat` /
`checkColor` / `checkEqualInt` / `checkEqualSize` + `g_checks` / `g_failed`），
不要在每个测试里再抄一份。
**资源按可执行文件目录解析**：每个测试目标都和 demo 一样会把 `resources/` 复制到
`dist/`，测试开头要调 `initResources()`——不调的话路径按工作目录解析，
写出来的 fixture 会落进仓库树（还会被打进 romfs）。

## 目录

```
bklv/               库本体，自包含：代码 + 3rd/ + 自己的 CMakeLists.txt
  bklv.hpp          伞头文件：常用类型一次引入（demo 只用它）
  core/             接口层：view / box / app / platform / graphics / input / ...
  core/ui_scale.*   UI 缩放（主题尺寸按 1280x720 写，运行时按窗口宽等比放大）
  views/            语义控件：label / image / scrolling_frame / recycler / debug_log_view / ...
  widgets/          外壳与外观驱动的控件：shell（四层外壳）/ pager（页面懒加载）/ appearance_view
                    / rectangle / button / header / bottom_bar / slider / tab_frame / sidebar
                    / dialog / dropdown / edit_text_dialog / hints_bar
  backends/         图形 API 实现：opengl / vulkan / gles3 / imgui 后端
  platforms/        平台实现：sdl3 / switch
  3rd/              vendored 依赖：imgui / yoga / tweeny / SDL / stb / json
                    / webp（可选）/ lunasvg + plutovg（可选，SVG）
  CMakeLists.txt    编出 bklv::bklv（INTERFACE 目标，见上）
resources/theme/    主题：themes.json + <主题>/{theme.json, tokens/, base/, widget/, view/}
                    （名字 = 路径去扩展名；文件清单提前写在 theme.json 的 files[] 里）
demo/               展示程序：main.cpp + app/（页面 / 侧栏 / 列表项），只 include bklv/bklv.hpp
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

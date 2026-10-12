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
- **Label 的宽度比较都留了 0.5px 容差**（`views/label.cpp`）：文本宽度由测量函数算、
  可用宽度由 Yoga 算，父容器又常把子节点定成"恰好等于文字宽度"。不留容差时：
  ① 单行截断会把够短的文字截成一个省略号（tab 标题踩过：中文全变 `…`，ASCII 反而正常）；
  ② 多行换行会把"两个字一行"的标题拆成两行（侧栏标题"导航"踩过）。
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
- **控件外观要还原 borealis 的焦点高亮**：每个可聚焦控件的外观里有三层高亮
  （`hlBg` / `hlGlow` / `hlBorder`），focus 态把它们补到
  `color.highlight.background`、`color.highlight.glow`、`color.highlight.border`，
  描边宽度 `metric.highlight.strokeWidth`（5，borealis 的 `brls/highlight/stroke_width`）、
  圆角 `metric.highlight.radius`（6），并且**向外扩 2.5px**（inset 负数）。
  顺序是：自己的底 → hlBg → 内容 → hlGlow → hlBorder（和 borealis 的
  "高亮底画在内容之前、描边画在内容之后"一致）。
- **控件的样式全在 JSON 里**（`resources/theme/<主题>/widgets/<控件>.json`）：
  一份外观由三段组成——
  ① `box`：盒模型/布局（width/height/minWidth/minHeight/padding/margin/gap/grow/shrink/
     justifyContent/alignItems/backgroundColor/cornerRadius/borderThickness/borderColor）。
     长度写数字（按 720p 缩放）、`"auto"`、`"50%"` 或 `@theme/metric.a.b`；
     另有 `axis`（row/column）——AppearanceView 默认是 column，页头/底栏这类横向的要写明；
     `padding`/`margin` 可以写一个值（四边同值）或 `[上,右,下,左]`。
     只有写了的字段才会被套用，所以空 box 不会冲掉控件的默认值。
  ② `layers`：绘制结构。③ `states`：状态动效。
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
  ① **不扫目录**：`themes.json` 列主题，各主题的 `theme.json` 列自己的外观文件。
     romfs 上目录迭代真机未验证，加外观文件要记得登记。
  ② **装饰层走 JSON，语义绘制留 C++**：底/边框/文字/图片/状态动效由 `layers` 描述；
     Label 的换行、Image 的缩放、进度弧、滑块轨道走 `custom` 层
     （`AppearanceView::drawCustomLayer`）。把语义绘制也塞进 JSON 等于写解释器，别做。
  ③ **draw 里不查主题**：外观值在构造 / `applyTheme()` 时取到 `AppearanceInstance`，
     绘制只读当前值。换肤靠 `App::setTheme()` → 重新取外观 + `applyThemeRecursive()` + 重排。
  ④ **主题只覆盖未被显式设置过的值**：调过 `setFontSize/setTextColor` 的控件，
     换肤不再动它；要跟随主题就 `setXxxExplicit(false)`。
  ⑤ **装饰件用显式 `width`/`height`**：层默认铺满"控件范围按 inset 收缩后"的区域，
     给了 `width`（如色条 4）或 `height`（如下边线 1）就按 `align`/`valign` 摆放。
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
  `Logger` 的环形缓冲，界面上的日志窗口（`views/debug_window.hpp`）读缓冲渲染。
  **不要**为每条日志创建 View —— borealis 那样做会不断触发 Yoga 重排
- ImGui 窗口在 `App::onDrawWindows` 里下发（视图树画完之后、`ImGui::Render()` 之前）
- **按键重复（长按连续移动）**：`core/input.hpp` 的 `KeyRepeater`——首次按下**立刻**响应，
  按住则等 `repeatDelay`（默认 250ms）后每 `repeatInterval`（默认 100ms）一次，
  和 borealis 的 `BUTTOM_REPEAT_TRIGGER` / `BUTTON_REPEAT_DELAY` 一致。
  时间来自 `getFrameDeltaSeconds()` 累加，键盘/手柄/各平台共用一套，不用平台 API；
  A/B 也走同一套（borealis 的 A/B 会重复）。
  **一个物理键一个重复状态**：以前两个键（方向键 + WASD）共用一个"边沿状态"，
  互相把对方清掉 → 按住方向键每帧都判成"刚按下"，导航飙到帧率。
- **页头与底部栏是外壳级的（全局），内容区吃剩余高度**（`widgets/applet_frame.*`）：
  结构按 borealis：`AppletFrame[ Header / content(grow) / BottomBar ]`，
  其中 `Header = [图标][标题][右侧区]`（widgets/header*.json + header_icon.json），
  `BottomBar = [提示(左) | 状态区(右)]`（widgets/bottom_bar.json），
  Header 的左右外边距用 `appletFrame.paddingSides`、底栏用 `hints.footerMarginSides`。
  显隐用**实例方法** `showHeader(bool)` / `showBottomBar(bool)`：隐藏时看 `GONE`（不占位），
  所以内容区自动吃掉那部分高度；**栏里不能留固定外边距**（页头下边的留白写在页头自己的
  padding 里），否则隐藏后会留下空隙。
  `setBarsAnimated(true)`（默认）时用 `core/time.hpp` 的 `Animatable` 补间折叠：
  动画期间两栏保持 VISIBLE 并逐帧改高度 + `App::requestLayout()`，补间结束才真正 GONE。
- **有外边距就别写 `width: 100%`**：`100% + margin` 会多出边距那一段（页头/底栏右边被吃掉、
  内容的右上按钮被推出边界，都踩过）。要"填满父容器再留边距"就用 **auto 宽 + margin**，
  父容器负责 stretch（`AppletFrame` 的内容区已经 `alignItems: stretch`）。
- **滚动容器用 `flex-basis: 0` + `flex-shrink: 1`**（`views/scrolling_frame.*`）：
  Yoga 的 flexShrink 默认是 **0**，而 basis 默认是内容尺寸。两者不改的话，内容一高，
  Yoga 会按比例把**同一列里的兄弟节点一起压缩**（按钮行 48 → 28），滚动器自己的高度
  也随内容变 → 每次切页内容区高度都变，看着像底栏在上下闪。
  `grow` 配 `basis 0` 才是"只占剩余空间"。
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
  `AppletFrame` 的 "B 返回上一层"、`Dialog` 的 "B 关闭" 都靠这条
  （不冒泡时焦点在内容里的按钮上按 B 完全没反应）。
  指针不冒泡是因为 `hitTest` 只返回可聚焦视图，命中的本来就是控件本身。
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
  core/ui_scale.*   UI 缩放（主题尺寸按 1280x720 写，运行时按窗口宽等比放大）
  views/            语义控件：label / image / scrolling_frame / recycler / ...
  widgets/          外观驱动的控件：appearance_view / rectangle / button / header / bottom_bar
                    / slider / tab_frame / sidebar / dialog / dropdown / applet_frame
                    / edit_text_dialog / hints_bar
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

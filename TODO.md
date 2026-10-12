# bklv 开发计划

本文件规划 bklv 从「能跑通的学习代码」长成「能被别人拿去用的界面库」。
参考实现是 [borealis](https://github.com/xfangfang/borealis)（9.6 万行），
bklv 只按需移植，不追平；两者定位与技术选型的差异见文末。

条目按优先级分组，每条都写清**要做什么**和**怎么算做完**。
已完成的项不留在本文件里，历史见 `git log`。

---

## 定位

| | |
|---|---|
| 是什么 | 跨平台 C++17 界面库：Yoga 算 Flexbox 布局，ImGui 的 `ImDrawList` 负责绘制 |
| 目标形态 | 可被外部项目 `add_subdirectory` / `find_package` 引入的库，本仓库同时提供 demo |
| 平台 | macOS / Linux / Windows（SDL3）、Nintendo Switch（libnx + EGL/GLES3） |
| 图形后端 | OpenGL 3.2 Core、Vulkan、OpenGL ES 3；编译期选择 |
| 不做 | XML 布局与绑定、音频、视频、PS4/PSV |

---

## 现状

已可用（约 1 万行）：

| 层 | 内容 |
|---|---|
| 视图核心 | `View`（一视图一 `YGNode`）、`Box`（Flexbox 全套 + 节点管理）、`Style`/`Theme`、焦点与导航、`Animatable`/`Ticking`/缓动 |
| 控件 | `Label`（换行 / 省略号 / 对齐 / 缓存）、`Image`（缩放模式 / 圆角）、`ScrollingFrame`（惯性 + 滚动条 + 焦点跟随）、`HScrollingFrame`、`RecyclerFrame`（对象池虚拟列表）、`ProgressSpinner`、`Hints`（底部按键提示） |
| 调试 | `DebugWindow`（ImGui 日志窗口，级别过滤 + 环形缓冲）、`Logger`（带文件行号 / 时间戳） |
| 平台 | `Platform` 抽象 + `PlatformSDL3` / `PlatformSwitch`；手柄抽象 `GamepadSource`；电池 / 网络 / 亮度 / 剪贴板 / 语言等系统能力接口 |
| 后端 | 四个 `RenderBackend` 实现 + Windows 的 GL 运行时加载（imgl3w） |
| 图片 | `image_decoder` 抽象：macOS 走 ImageIO，其余走 stb_image |

---

## P0 —— 库化（做别的事之前的前提）

已经做到：`bklv/` 自包含（代码 + `3rd/` + 自己的 `CMakeLists.txt`），
`demo/` 是展示程序，`add_subdirectory(bklv)` + `target_link_libraries(app PRIVATE bklv::bklv)`
可用。剩下的是「能被 install 出去」这一层。

- [x] **库与 demo 分层** —— 已做：`bklv/` 只含库，demo 在 `demo/`。
- [ ] **`bklv::bklv` 变成真正的库** —— 现在是 INTERFACE 目标：它把几个 OBJECT 库
      的目标文件用 `$<TARGET_OBJECTS:>` 带出去，因为 OBJECT 库的目标文件不会透过
      INTERFACE 传递（踩过）。要做 `install()` 就得解决"静态库会丢掉 ImGui
      静态初始化注册"的问题（见「改构建时注意」与下文技术债）。
      *验收*：`find_package(bklv REQUIRED)` 后能编出可运行程序。
- [ ] **`install()` 规则**
- [ ] **`install()` 规则**
      头文件装到 `include/bklv/`，库装到 `lib/`，导出 `bklvConfig.cmake` /
      `bklvConfigVersion.cmake`。
      *验收*：`cmake --install build/gl --prefix /tmp/bklv` 后目录结构正确，
      且用该前缀的 `find_package` 能编过。
- [ ] **依赖暴露策略要定下来**
      imgui / yoga / stb 现在是 submodule。作为库被引入时，是让使用者自己提供，
      还是 bklv 用自己的副本？建议：**bklv 内部用 vendored 副本，导出时把
      imgui/yoga 作为 PRIVATE**，使用者只包含 `bklv/...` 的头，不直接接触 imgui/yoga。
      但 `View::draw()` 的签名里有 `ImDrawList*`，自定义控件必须能拿到 imgui 头——
      所以需要一个「公开的 imgui 头子集」，或把 imgui 作为 PUBLIC 依赖并说明。
      *验收*：README 里写明规则，并有示例工程验证。
- [x] **umbrella 头**
      `bklv/bklv.hpp` 一次引入常用类型（core / views / widgets，不含按平台选的
      backends / platforms）。*验收*：demo 的全部 16 个源文件现在只 include 它一个。
- [ ] **版本号与变更记录**
      `project(bklv VERSION ...)` + `CHANGELOG.md`，并让
      `bklv::getVersionString()` 可用。
- [ ] **示例工程进仓库**
      `examples/hello`：20 行以内搭出一个能跑的界面，作为接入文档的活代码。

---

## P1 —— 应用骨架（能写多页应用）

这是「用 bklv 做真实应用」和「demo 拼凑」的分界线。

- [ ] **Activity / 页面栈**
      borealis 的 `core/activity`：页面入栈出栈、生命周期回调
      （`onCreate` / `onResume` / `onPause` / `onDestroy`）、返回栈、转场。
      现在 `App::setRootView()` 只有一个根视图，写多页要自己管。
      *验收*：demo 里能实现「列表 → 详情 → 返回」且返回时列表滚动位置保留。
- [ ] **`Button`**
      基础按钮 + 变体（主要 / 次要 / 描边 / 图标 + 文字），焦点与点击态。
      现在 demo 里的按钮是各自手写的 Box + Label。
      *验收*：三种变体在 demo 里展示，鼠标点击与手柄 A 都能触发。
- [ ] **`Rectangle`**
      纯色 / 圆角 / 边框 / 渐变的基础块。现在到处用 `new Box` 配样式代替。
      *验收*：替代 demo 里手写的分隔线与色块。
- [ ] **列表项族 `cells`**
      borealis 有 `ListCell` / `DetailCell` / `RadioCell` / `ToggleCell` /
      `ValueCell` / `SmallHeaderCell` / `BooleanCell`，配合 `RecyclerFrame`
      就是大部分设置页。现在 demo 每个 cell 都从零写。
      *验收*：用现成 cell 拼出一个设置页，代码量比现在少一半。
- [ ] **`Header`**
      顶部栏：左返回 + 标题 + 副标题 + 右侧动作区。现在 demo 手写。
      *验收*：配合 Activity 栈，返回按钮自动出现。
- [ ] **`BottomBar`**
      底部操作栏，和 `Hints` 联动（手柄按键提示 + 可点按钮两用）。
- [ ] **`TabFrame`**
      窗口内分页容器。demo 里的 `TabBar` 是应用层实现，应提升为库控件
      （含 tab 格子、选中态、左右切换、焦点自动选中）。
      *验收*：demo 的 tab 效果由库控件实现，删除 `demo/app/tab_bar.*`。
- [ ] **`Sidebar`**
      侧边导航（Switch 上竖排 tab 用得多）。与 `TabFrame` 共享选中态逻辑。
- [ ] **`Dialog`**
      模态对话框：标题 + 内容 + 按钮组、焦点圈定在对话框内、背板遮罩、
      `B` / `Esc` 关闭。
      *验收*：能弹出一个确认框并正确返回用户选择。
- [ ] **`Dropdown`**
      下拉选择，Switch 上以左右切换形式呈现。
- [ ] **`Slider`**
      连续值滑条（音量 / 亮度），手柄左右调整 + 鼠标拖拽。
- [ ] **`EditText` / `EditTextDialog`**
      文本输入。桌面走 ImGui 的文本输入，Switch 走软键盘（`swkbd`）。
      依赖 IME 支持（见 P2）。
      *验收*：Switch 真机能弹出软键盘并取回输入。

---

## P2 —— 框架能力

- [ ] **事件总线**
      borealis 的 `core/event`：类型化事件订阅 / 派发，跨视图通信不靠层层传指针。
      `DebugLayer` 就是靠它订阅日志的；bklv 现在是 Logger 缓冲 + 界面轮询。
- [ ] **通知 / Toast**
      borealis 的 `notification_manager`：横幅通知、超时、队列。
      *验收*：调用一行 API 弹出"已保存"提示，不打断当前焦点。
- [ ] **纹理缓存与异步加载**
      现在 `Image::setImageFromFile()` 是**同步解码**（`graphics.cpp` 里直接
      `decodeImageFile` + 上传），大图会卡住一帧甚至几帧。需要：
      后台解码 + 主线程上传 + 按路径缓存 + 占位图。
      *验收*：加载 4K 图时帧率不掉，同一路径第二次加载不重复解码。
- [ ] **异步任务队列**
      borealis 的 `core/task`（含 Switch 用的线程封装）。Switch 上
      文件 IO / 网络必须先丢后台线程，主线程只做上传。
      与上一项配套；也是事件总线在跨线程方向的用途。
- [x] **i18n**
      `Platform::getLocale()` 已经有了。最终方案：文案用 JSON（`resources/i18n/<locale>.json`），
      跟随 borealis 的资源布局但**不依赖目录枚举**（romfs 上的目录扫描真机未验证，
      改为单文件 + 可选 `$files` 清单）。
      *验收*：切换语言后界面文案即时更新。
      已完成：`core/i18n.hpp/.cpp` + `resources/i18n/*.json` + `tests/i18n_test.cpp`（30 项断言）。
      语言名经 `canonicalLocale()` 归一，装载不依赖目录枚举（用 `$files` 清单）。
      注意：文案在控件构造时解析，切语言需显式重译已有视图（文本页有按钮演示）。
- [ ] **手势层**
      Switch 触摸屏的原始状态已经在 `bklv/backends/imgui_impl_libnx.cpp` 里取到了，
      缺上层：点击 / 长按 / 滑动 / 捏合，并接入焦点系统（点选、拖拽滚动）。
- [ ] **属性动画**
      有 `Animatable` + tweeny，但缺 borealis `animation.hpp` 那种
      「给 View 的某个属性挂一段补间」。现在动画要手写 `update()`。
      *验收*：`view->animate("width", 200.f, 0.3f)` 这类调用可用。
- [ ] **IME / 输入法**
      桌面候选框定位、Switch 软键盘。`EditText` 的前置条件。
- [ ] **在库内补 `getClassString()` 的默认实现**
      见技术债第 1 条。

---

## 与 borealis 的功能对等（定位：bklv 是升级版）

清单与原因见 [`notes/borealis-parity.md`](notes/borealis-parity.md)（对照方式：逐个头文件对读）。
分批：

- [x] 6 个列表 Cell（`widgets/cell.*`：detail / bool / radio / selector / slider / input）
      + demo 设置页（对标 borealis Settings tab）
- [x] Battery / Wireless 图标化（矢量自绘 + 数据源可插拔；Switch 走 psm/nifm，
      桌面注入演示数据）+ demo 底栏可见
- [x] Notification / Toast（`widgets/notification.*` + `App::notify`；外观状态驱动出现/消失，
      没有根视图时先排队）+ demo 设置页有"弹一条通知"入口
- [ ] **P0 控件补齐**：
      Battery/Wireless 做成图标化状态区、Notification、手势（tap/pan/scroll）、
      View 变换（scale/rotate/skew/fontScale）
- [ ] **P1 核心能力**：task/thread + 异步图片加载、audio（SDL3 + Switch）、
      debug_layer（wireframe）、event 补齐
- [ ] **P2 平台扩展**：D3D11（待定）、Android/iOS 骨架（无法本机验证）、
      video（需要决定是否引解码依赖）
- [ ] **P3 demo 对标**：把 borealis demo 的 7 个启用页 + 3 个注释页全部做出来

## 页头与底部栏全局化（2025-10 需求确认后实施）

- [x] 页头/底栏提到外壳级：`AppletFrame[Header / content(grow) / BottomBar]`，
      内容区吃剩余高度；隐藏时用 GONE，栏里不留固定外边距
- [x] 底栏**合并成一个**：左侧提示（HintsBar）+ 右侧状态区（StatusArea：桌面时钟、Switch 电量）
- [x] 页头按 borealis 改成 `[图标][标题][右侧区]`（header_icon.json 管图标尺寸与左边距）
- [x] 显隐用实例方法 `showHeader/showBottomBar`，`setBarsAnimated` 控制是否走补间折叠
- [x] 页面不再自己画标题行（标题归全局页头）
- [x] 修 `AppearanceView::update()` 不递归子树（底栏里的提示栏因此从不刷新）

## 对齐 borealis 的视觉与 demo 重做

- [x] **UI 按 720p 比例缩放**（`core/ui_scale.*`）：主题尺寸按 1280x720 写，
      运行时 `uiScale = 宽/1280`；metric 与外观里的像素量都跟着缩放
- [x] **还原 borealis 的焦点高亮**：每个可聚焦控件三层高亮
      （高亮底 / 辉光 / 描边），参数取 `brls/highlight/*`（描边 5、圆角 6、外扩 2.5）
- [x] 默认主题 = borealis 暗色 / Switch 风格（色值 + 尺寸 + 各控件语义）
- [x] 提示栏按 borealis 的 hint.xml：内边距 4/16、图标 25.5、文字 21.5、间距 8；
      底栏高度取 `brls/applet_frame/footer_height`（73）——不再被压扁
- [x] 侧栏是**菜单语义**：上下只移动焦点，A 才切页，切页后焦点自动进子页第一个控件
- [x] **demo 推倒重做**：`demo/app/demo_app.*`（AppletFrame + 侧栏 + 子页面）+
      `demo/app/ui.*`；页面全部基于库控件重写，删掉手搓控件与旧外壳

## 交互手感与默认主题对齐 borealis

- [x] 按键重复改成 borealis 的节奏（首次立刻响应，250ms 后每 100ms 一次），
      键盘/手柄/多平台统一；**修掉"两个键共用一个边沿状态"导致按住时按帧率触发**
- [x] 默认主题改成 borealis 暗色 / Switch 风格（色值与尺寸都在
      `resources/theme/default/` 的 JSON 里），控件外观也按 borealis 的语义接上新键
      （按钮高亮字色、滑块三段、遮罩、页头色条、侧栏选中色条）
- [x] 图标字体 `resources/font/switch_icons.ttf` + `getControllerButtonIcon()`
      （码位同 borealis）+ 文字层字体钩子 `AppearanceView::getLayerFont()`
- [x] 提示栏 `widgets/hints_bar.*`（AppletFrame 底栏），撤掉旧的顶部覆盖层
- [x] demo 重构：AppletFrame（页头 + 提示栏）+ 左侧栏导航 + 右侧子页面

## 主题与外观系统（数据驱动换肤）

需求：`resources/theme/default/` 分类存 JSON，default 为默认主题，复制改名改 JSON 即换肤；
控件外观（含绘制结构）由 JSON 描述，状态动效用 tweeny 插值；主题只覆盖未显式设置的值。

- [x] 主题数据层：`themes.json` 清单 + `<name>/{theme.json,colors.json,metrics.json,widgets/*.json}`，
      key 拍平成 `color.*` / `metric.*`，缺项回退 default
- [x] 外观引擎：`layers`（rect/text/image/custom）+ `states`（`<层>.<属性>` → 目标值/时长/缓动），
      13 个可插值属性，颜色 4 通道 + 标量 1 通道各自一个 tweeny tween，状态有优先级
- [x] `View::applyTheme()` / `applyThemeRecursive()`；`App::setTheme()` 运行时换肤
- [x] `bklv/widgets/`：`AppearanceView` 基类 + `Rectangle` + `Button`（含 ghost 变体、禁用态、按下反馈）
- [x] 第二套皮肤 `ocean`（复制 default 改出来的）用于验证换肤
- [x] 测试：`tests/theme_test.cpp`（44）+ `tests/widget_test.cpp`（24）
- [x] `Header` / `BottomBar`（多文字层 + 显式宽高的装饰件）
- [x] 外观层支持显式 `width`/`height` + `align`/`valign`（色条、下边线这类装饰件要用）
- [x] 外观驱动控件的**固有尺寸**：测量函数按"文字层宽度 + inset"上报，
      所以 TabCell/Button 这类不用给死宽高
- [x] `Slider` / `TabFrame` / `Sidebar`
- [x] **`@theme/` 引用写错会硬失败**（不再静默回退成洋红），
      并由 theme_test 第 8 节逐个编译两套主题的全部外观兜底
- [x] **弹层 + 焦点栈**（`InputManager` 模态栈 + `App::pushOverlay`/`popOverlay`）：
      模态内命中与导航限制、焦点保存/恢复、App 销毁时清态
- [x] `Dialog`（遮罩 + 面板 + 按钮行，开关走外观状态动效）
- [x] `Dropdown`（触发器 + 弹层列表；选完/点外面自动关）
- [x] `AppletFrame`（宿主框架：页头 + 内容栈 + 底栏；页头 Regular/Popup 走 variant）
- [x] **动作沿父链冒泡**（`View::triggerAction`）：容器级 B 动作在焦点位于子节点时才真的生效
- [x] **IME**（`core/ime.*`：接口 + 平台注入）+ Switch 的 swkbd 实现 + 桌面内联键盘输入
- [x] `EditTextDialog`（文本模型 + 输入框外观 + 光标闪烁）
- [x] **控件移植清单全部完成**（对照 borealis 20 个控件）
- [x] 主题引用容错：`@theme/color/a.b` 自动按点分解析并警告（不存在的 key 仍硬失败）

### 控件移植清单（对照 borealis 的 20 个）

| borealis | bklv | 状态 |
|---|---|---|
| label / image | `views/` | 已有（语义绘制 + 跟随主题） |
| scrolling_frame / h_scrolling_frame | `views/` | 已有 |
| recycler | `views/recycler` | 已有 |
| progress_spinner | `views/progress_spinner` | 已有 |
| hint | `views/hints` | 已有（等价） |
| debug_layer | `views/debug_window` | 已有（等价，用 ImGui 窗口） |
| rectangle | `widgets/rectangle` | 新增（外观驱动，零绘制代码） |
| button | `widgets/button` | 新增（外观驱动 + ghost 变体） |
| header | `widgets/header` | 新增 |
| bottom_bar | `widgets/bottom_bar` | 新增 |
| tab_frame | `widgets/tab_frame` | 新增（tab 栏 + 内容区，切页只脱离不销毁） |
| sidebar | `widgets/sidebar` | 新增（条目复用 tab_cell 外观） |
| slider | `widgets/slider` | 新增（外观分段 track/fill/knob，进度几何留 C++） |
| applet_frame | `widgets/applet_frame` | 新增（页头 + 内容栈 + 底栏，B 逐层返回） |
| dialog | `widgets/dialog` | 新增（含弹层 + 焦点栈：模态输入、焦点保存/恢复） |
| dropdown | `widgets/dropdown` | 新增（触发器 + 弹层列表，锚在触发器下方） |
| edit_text_dialog | `widgets/edit_text_dialog` | 新增（TextEditModel + 桌面内联输入 / Switch swkbd） |

## JSON 视图系统（移植 borealis 的 XML 视图）

对应用户确认的方案：bklv 不用 XML，用 JSON 描述视图树；i18n 同样用 JSON。
依赖 `bklv/3rd/json`（nlohmann/json，与 borealis 同版本）。

- [x] **Template / instantiate 两阶段**
      模板缓存只存**未经语言/主题解析**的 JSON，`@i18n/` `@style/` `@theme/` 在实例化时才解析。
      否则缓存会把当前语言烤死（列表 cell 复用同一模板时必现）。
- [x] **属性值带类型**：`24` 与 `"24px"` 等价，`"50%"` 走百分比；类型不符报错而非静默忽略。
- [x] **错误可定位**：语法错误用 `json::parse_error.byte` 折算行号，属性错误用 `json_pointer` 路径。
      未知属性=警告，未知 type / 结构错误=返回 nullptr（borealis 是 `fatal()` 崩进程）。
- [x] **cell 数据绑定**：JSON 节点 `"bind": "title"` + app 侧回调，复用现有 `RecyclerDataSource`。
      默认目标属性按类型定（Label→text / Image→src），其他类型要求显式 `bindTo`。
- [x] **include + 循环检测**、**深度/节点上限**、**异常安全（RAII）**。

已完成（P0–P3 + demo 接入）：
- 依赖：`bklv/3rd/json`（nlohmann/json v3.11.3，单头模式）。实测 NRO +65536 字节
- 代码：`core/attributes.*`、`core/json_view.*`、`views/json_cell.*`、
  `resources/views/{page_json,list_item}.json`、`demo/app/pages/page_json.cpp`（第 5 个 tab）
- 测试：`tests/json_test.cpp`（80 项断言）+ `tests/i18n_test.cpp`（30 项），
  CMake 开关 `-DBKLV_BUILD_TESTS=ON`，产物在 `dist/` 用 ctest 或直接执行
- 文档：`notes/json-view-design.md`、`docs/bklv-api.md`、README、AGENTS.md

移植期间测试抓到的真 bug（都已修）：`"12px"` 声明支持但没实现、语法错误没进诊断通道、
include 循环检测失效导致爆栈、单行 Label 缺容差把中文截成省略号。

---

## P4 —— 布局与属性表优化（**已完成**）

已完成（`App::getLayoutCount()` 是累计布局次数，调试窗口状态行可见）：

实测结果（同一台机器，控件页静止 / JSON 页带回收列表）：

| 场景 | 改前 | 改后 |
|---|---|---|
| 控件页静止 | 60 次/秒（每帧 1 次） | **0 次/秒** |
| JSON 页（回收列表） | 10 次/帧 | **1 次/帧** |
| 一次 `invalidate()` | 1 次 | 1 次（下一帧） |
| `BKLV_FORCE_RELAYOUT=1` | — | 每帧 1 次（逃生开关有效） |

顺带修掉一个潜在问题：`refreshFontDependentLayout()` 原来**只向上冒泡**，
调在根节点上等于什么都没标（根通常没有测量函数），子节点 Label 的测量缓存不会失效。
当前流程因为"字体就绪早于首次布局"没暴露出来，改成向下递归了。

- [ ] **脏标记布局**
      现在 `App::frame()` 每帧无条件 `YGNodeCalculateLayout` 全树重算
      （`bklv/core/app.cpp`，注释自认"比 borealis 保守"）。
      borealis 的做法是 `invalidate()` 冒泡到根时才重算。
      改法：`View` 加脏标志 + 冒泡；`App::frame()` 条件重算；
      文本变更 / 子节点增删 / Style 主题切换 / 窗口 resize / 字体换代 全部置脏；
      加 `App::requestLayout()` 兜底与 `BKLV_FORCE_RELAYOUT` 环境变量排查漏置脏。
      *验收*：加 `getLayoutCount()` 计数器，控件页静止 1 秒布局次数 60 -> 0；
      滚动帧为 0（滚动只改 translate）；切 tab / 回收 cell 时才 +1。GL 与 VK 都要跑回归。
- [ ] **属性表类级静态化**
      borealis 每个 View 实例挂 7 个 `unordered_map` + 1 个 `set<string>`。
      改为虚函数 `attributes()` 返回类级 static 表，handler 带 `this`。
      *验收*：`sizeof(View)` 下降，列表页内存下降（估算每实例省约 500B，以实测为准）。

---

## P3 —— 平台与工程

- [ ] **CI**
      GitHub Actions 矩阵：{macOS, Ubuntu, Windows} × {GL, Vulkan}，
      加上 `-DBKLV_TARGETS=main` 的快速通道。目标是零警告 + 全部目标能链接。
      *验收*：PR 上能看到矩阵全绿。
- [ ] **单元测试**
      现在没有测试，验证全靠手动跑 demo。优先给纯逻辑的部分加：
      `RecyclerFrame` 的可见区间与回收、`Label` 的断行、焦点导航的几何打分、
      `ScrollingFrame` 的偏移与惯性。
      *验收*：`ctest` 能跑；纯逻辑用例不依赖图形 API 与 ImGui 上下文，
      因此不需要窗口就能执行（`RecyclerFrame` / `Label` / 焦点导航都不碰绘制）。
- [ ] **Android**
      SDL3 支持 Android，需要工程骨架（Gradle + `SDLActivity`）与
      `PlatformAndroid`。Vulkan 在 Android 上是主力后端。
- [ ] **iOS**
      同上，需要 Xcode 工程骨架。
- [ ] **`applet_frame`**
      Switch 首页 / 小程序模式的覆盖层。只有在做 Switch 应用时才需要。
- [ ] **光标 API**
      borealis 的 `application` 有光标状态（隐藏 / 自动隐藏 / 忙碌）。
      桌面端迟早要，Switch 无鼠标可跳过。

---

## 技术债

按「会不会再坑人」排序。

| # | 问题 | 现状与影响 | 建议 |
|---|---|---|---|
| 1 | `getClassString()` 要每个子类手动 override | 基类返回 `"bklv::View"`，`Box` 返回 `"bklv::Box"`；漏了就显示基类名。调试日志与 dump 会误导（排查时被坑过一次） | 用「基类构造时登记的类型标记」或在 `View` 上加一个 `virtual const char* typeName()`，配合宏保证子类自动实现 |
| 2 | `Style` / `Theme` 用 `std::string` 作 key 查 `unordered_map` | 每次 `draw()` 都做字符串哈希查找，绘制热点上不划算 | 改成枚举或编译期常量 id；字符串 key 只保留给「从外部配置读主题」的入口 |
| 3 | 每帧两次全树遍历：`root->update()` + `root->frame()` | 无脏标记、无可见性剔除（只跳过 `GONE` 和裁剪区外）。`RecyclerFrame` 已经兜住了最大的那种列表，但深树会线性变贵 | 先量后改：加一个节点数与耗时的统计，确认是真瓶颈再引入脏标记 |
| 4 | `RecyclerCell` 默认绝对定位 | 它的定位是给 `RecyclerFrame` 手动摆格子用的；当普通流式控件用时必须传 `false`，否则所有格子叠在原点（踩过：tab 栏全叠在一起） | 构造函数参数名与注释已写明；考虑把「回收接口」与「绝对定位」拆开，或让 `RecyclerFrame` 自己设定位 |
| 5 | `ScrollingFrame` 不拥有内容视图 | 换内容必须用 `evictContentView()`，直接 `setContentView()` 覆盖会销毁旧内容而调用方可能仍持有指针（踩过：use-after-free 崩溃） | 语义已写进注释；更好的做法是让内容视图的所有权显式化（`std::unique_ptr` 或 `shared_ptr` 参数） |
| 6 | `Logger` 的格式化缓冲 1024 / 512 字节 | 超长日志被静默截断 | 截断时补一个提示，或按需扩容 |
| 7 | 无测试、无 CI | 回归靠人跑 demo；本会话里三个 bug 都是靠临时写的无窗口测试才定位到的 | 见 P3；把这次写的那些 headless 验证脚本整理成 `tests/` |
| 8 | ~~`src/app` 是 demo 代码却在库源码树里~~ | 已解决：库在 `bklv/`，demo 在 `demo/` | — |
| 9 | 库里的 `bklv_app` 层已经搬走，但 `demo/app` 仍然耦合在根 `CMakeLists.txt` 里 | 加第二个示例程序时要复制一遍源文件列表 | 等真有第二个示例时再抽成 `demo/CMakeLists.txt` |
| 10 | 多后端产物目录靠不同构建目录区分 | 两个构建目录若指向同一输出目录，会互相覆盖同名文件，而 CMake 认为目标最新不会重链，得到「另一个后端编出来的程序」且无提示 | 现在是 `dist/`、`dist/vk/`、`dist/switch/` 三份；加新构建配置时注意别和已有的合并 |

---

## 与 borealis 的取舍

**技术选型差异（不是差距，是有意为之）**

| 维度 | borealis | bklv |
|---|---|---|
| 绘制 | NanoVG | ImGui `ImDrawList` |
| 布局 | Yoga，但界面用 XML 描述 | Yoga，界面用 C++ 直接写 |
| 主题 / 样式 | `Style` + 主题文件 | 同样是字符串 key 的 `Style`/`Theme`（同名设计） |
| 日志界面 | `DebugLayer` 是 View，每条日志建 3 个 Label | `DebugWindow` 是 ImGui 窗口，读环形缓冲（不为每条日志建视图） |
| 页面 | `Activity` 栈 | 暂无（P1） |

**明确不做**

| 能力 | 原因 |
|---|---|
| XML 布局 / 绑定（`bind`、`tab_frame` 的 XML 装载） | 项目约定 C++ only |
| 音频（borealis 用 miniaudio） | 与界面库职责无关，交给使用者的应用层 |
| 视频（borealis 用 mpv） | 同上，且依赖太重 |
| PS4 / PSV 平台 | 无设备、无需求 |
| GLFW 平台 | 桌面统一用 SDL3 |
| `singleton` / `main` / `util` 这类工具头 | C++17 有更直接的做法（函数局部静态、标准库） |

**待定**

| 能力 | 说明 |
|---|---|
| `cache_helper` | borealis 用它缓存 XML / 纹理；等 P2 的纹理缓存落地后再看要不要 |
| `gesture` | Switch 触摸是确定要的，桌面鼠标手势优先级低 |
| `applet_frame` | 取决于是否做 Switch 应用 |

---

## 怎么用这份文档

1. 开工前把要做的条目在 issue 里开一条，注明本文件的对应小节。
2. 做完了删掉条目，不要留「已完成」清单——历史归 `git log`。
3. 技术债随改随手清；新增债在表里加一行，写清现象和触发条件。
4. 优先级变了就调整分组，别另开一份文档。

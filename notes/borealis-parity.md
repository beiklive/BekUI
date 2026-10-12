# 与 borealis 的功能对等清单

> 项目定位：**bklv 是 borealis 的升级版**——borealis 有的演示功能与控件我们都要有；
> 无法实现的写明原因；borealis 设计得不好的地方按更好的做法实现。
>
> 核对方式：逐个头文件/控件对读 `borealis/library/include/borealis/**` 与 `bklv/**`，
> 不是凭印象。日期：见 git 历史。

## 一、控件与视图

| borealis | bklv | 状态 | 备注 |
|---|---|---|---|
| label / image / rectangle / slider / button / header | 同名 | ✅ | 我们是外观驱动（JSON），他们是 XML + 主题表 |
| scrolling_frame / h_scrolling_frame / recycler | 同名 | ✅ | 我们的滚动容器修了 `flex-basis: 0` + `shrink: 1`（Yoga 默认 shrink 0 会压兄弟节点） |
| progress_spinner | 同名 | ✅ | 颜色跟随主题 |
| dialog / dropdown / edit_text_dialog | 同名 | ✅ | 我们的模态栈由 `InputManager` 管，`App::pushOverlay` 挂载 |
| applet_frame / bottom_bar / tab_frame / sidebar | 同名 | ✅ | 页头/底栏全局化 + 内容区吃剩余高度 + 补间折叠 |
| `views/hint`（提示图标） | `widgets/hints_bar` + `hint_item` | ✅ | 改良：提示是底栏左侧内容，按焦点动作重建（他们是一个独立 Hint 单例） |
| `views/cells/cell_bool` | `widgets/cell.*`（`bklv/widgets/cell.hpp`） | ✅ | BooleanCell：一条列表项 + 开关 |
| `views/cells/cell_detail` | `widgets/cell.*`（`bklv/widgets/cell.hpp`） | ✅ | DetailCell：标题 + 右侧详情文字（可点击） |
| `views/cells/cell_input` | `widgets/cell.*`（`bklv/widgets/cell.hpp`） | ✅ | InputCell / InputNumericCell：列表项里调起输入 |
| `views/cells/cell_radio` | `widgets/cell.*`（`bklv/widgets/cell.hpp`） | ✅ | RadioCell：单选圆点 |
| `views/cells/cell_selector` | `widgets/cell.*`（`bklv/widgets/cell.hpp`） | ✅ | SelectorCell：左右切换的选项 |
| `views/cells/cell_slider` | `widgets/cell.*`（`bklv/widgets/cell.hpp`） | ✅ | SliderCell：列表项里的滑块 |
| `views/widgets/battery` | `widgets/status_area.*` 的 `BatteryIcon` | ✅ 改良 | 矢量自绘（外壳 + 电量条 + 充电闪电），不用 `img/sys/*` 那套疑似系统图形；数据源可插拔（`setProvider`） |
| `views/widgets/wireless` | `widgets/status_area.*` 的 `WirelessIcon` | ✅ 改良 | 矢量自绘（三段弧按等级点亮 / 以太网方块）；Switch 走 `nifm` 的类型+强度，桌面可注入 |
| `views/widgets/account` | `widgets/account.*` | ✅ 改良 | borealis 是 **Switch 专用**（直接调 account 服务取头像图、没有文字、桌面是空控件）；我们做成可插拔数据源 + **无图也能看**（圆形色块 + 昵称首字）+ 昵称/副标题 |
| `views/debug_layer`（画视图边界/wireframe） | `views/debug_layer.*` + `App::setDebugLayerEnabled` | ✅ 改良 | 收集边界是**纯函数**（能无窗口测）；颜色在主题里；根视图就绪前的请求会排队补上 |
| — | `views/json_cell` | ➕ 多出 | JSON 模板驱动的列表项 |
| — | `widgets/appearance_view` / `status_area` / `hints_bar` | ➕ 多出 | 外观驱动基类与底栏两块内容 |

## 二、View 能力

| 能力 | borealis | bklv | 状态 |
|---|---|---|---|
| translate | `setTranslationX/Y`（参与布局） | 同名 + `setTranslateX/Y`（绘制期） | ✅ |
| scale | 无（demo 在自定义视图里手写） | `setScaleX/Y` | ✅ 比 borealis 更进一步 |
| rotate | 无（同上） | `setRotate`（弧度，绕控件中心） | ✅ |
| skew | 无（同上） | `setSkewX/Y`（弧度） | ✅ |
| fontScale | 无（同上） | `Label::setFontScaleX/Y` | ✅ 只缩放字形 |
| 变换中心 | nvgTranslate/Scale 自己算 | `setTransformOrigin`，缺省控件中心 | ✅ |

> 说明：`setScaleX` 这些在 borealis 里**不是 View 的 API**，而是它 demo 的 `TransformBox`
> 自定义视图里手写的 NanoVG 变换。ImDrawList 没有变换栈，我们在 `View::frame` 一层按
> 顶点段实现，所以**任何控件**都能用（这是升级点）。实现细节与踩坑见下。
| 可见性 VISIBLE/INVISIBLE/GONE | 同名 | 同名 | ✅ |
| 动作系统 registerAction / 冒泡 | ✅ | ✅（沿父链冒泡） | ✅ |
| 手势（tap / pan / scroll） | `core/gesture.hpp` + Tap/Pan/Scroll 识别器 | ✅ 改良 | 状态机简化成 START/STAY/END/CANCEL/SCROLL + "吃掉与否"；**识别器生命周期归 View**（`unique_ptr`，borealis 存裸指针）；纯逻辑可无窗口测 |
| 数据绑定 / cache_helper | `bind` / `cache_helper` | `json_cell` 绑定回调 + 纹理路径缓存；`Image::setImageFromFileAsync` 异步加载 | ⚠️ 部分 |

## 三、core 层

| borealis | bklv | 状态 |
|---|---|---|
| application（全局单例） | `core/app.hpp`（实例） | ✅ 改良：无全局静态、模态栈、`resetFocusState()` |
| style / theme（字符串 key 的全局表） | `core/theme_data.hpp` + `resources/theme/*.json` | ✅ 改良：JSON 主题、严格 key（写错报错不回退）、720p 缩放、可换肤 |
| animation / time / timer | `core/time.hpp`（`Animatable` + `Ticking` + 缓动） | ✅ |
| i18n | `core/i18n.hpp` | ✅ |
| ime | `core/ime.hpp` | ✅（Switch swkbd / 控件自绘） |
| input | `core/input.hpp` + `core/gamepad.hpp` | ✅ |
| logger | `core/logger.hpp` + `views/debug_window` | ✅ |
| platform / assets | `core/platform.hpp` / `core/resource.hpp` | ✅ |
| view / box / frame_context | `core/view.hpp` / `core/box.hpp` | ✅ |
| activity（Activity 栈 + 转场动画） | `AppletFrame` 的内容栈 | ⚠️ 无独立 Activity 与转场动画（待补） |
| audio（8 种系统音效） | `core/audio.hpp`（8 种音效，**代码合成**） | ✅ 改良 | borealis **只有 Switch 出声**（用 Nintendo 内置音效）、桌面什么都不做；我们代码合成，桌面 + Switch 同一套，且不随包带任何音频素材。Switch 的 audren 输出还没接（需要真机） |
| **video** | — | ❌ 见"无法实现"一节 |
| notification_manager（Toast） | `widgets/notification.*` + `App::notify` | ✅ 改良（实例而非全局单例；没根视图时排队） |
| task / thread（异步任务、线程池） | `core/task.hpp`（固定 2 worker + 主线程回调） | ✅ 改良 | borealis 用 `brls::async` + `ASYNC_RETAIN` 宏；这里是普通线程池，回调**保证在主线程**跑（由 `App::frame` 每帧 pump），没有宏 |
| **event**（事件总线） | 动作系统 | ⚠️ 部分 |
| gesture / touch/ | `core/gesture.hpp`（Tap / Pan / Scroll） | ✅ 改良（识别器归 View 所有，`unique_ptr`） |
| singleton / util / main | — | ➖ 工具类，不逐个对等 |

## 四、平台与渲染

| 平台/后端 | borealis | bklv | 状态 |
|---|---|---|---|
| macOS / Linux / Windows | ✅ | ✅（SDL3 + GL/VK） | ✅ |
| Switch | ✅（GLES3 + libnx） | ✅（构建通过，待真机验证） | ✅ |
| Android / iOS | ✅ | — | ❌ 见"无法实现" |
| PS4 / PSV | ✅ | — | ❌ 见"无法实现" |
| GLFW | ✅（桌面另一套窗口） | — | ➖ 我们用 SDL3，重复 |
| D3D11 | ✅ | — | ❌ 待定（Windows 上有实际价值） |
| Metal | ✅ | — | ❌ 待定（macOS 已有 GL，收益低） |
| 渲染方式 | NanoVG（矢量） | ImDrawList（多后端 GL/VK/GLES3） | ✅ 不同路线，见"改良" |

## 五、demo 页面（对标 borealis demo 的展示内容）

| borealis demo 页 | bklv demo | 状态 |
|---|---|---|
| Basic components（HScroll / 5 种按钮 / Slider / Label 对齐 / 图片 5 种缩放） | demo「控件」页 | ✅ | HScroll + primary/bordered/borderless/ghost/disabled 五种按钮 + 滑块带实时百分比 + 左中右标签对齐 + 图片缩小/原尺寸/放大/拉伸/居中裁切 5 例 |
| Layout and alignment（flex 四组 + grow + wireframe） | 布局页 + 设置页的 wireframe 开关 | ✅ |
| Transform test（translate/scale/skew/rotate/fontScale + 多段缓动） | demo「变换」页 | ✅ |
| Text test（宽高/对齐/singleLine 实时调） | demo「文本」页 | ✅ | 用 SliderCell/SelectorCell/BooleanCell 实时改宽高、水平/垂直对齐、单行截断（对标 borealis 的 TextTestTab） |
| Scroll test | demo「滚动」页 | ✅ | 独立 ScrollingFrame + 20 行按钮验证"焦点走到哪外层滚到哪" |
| Pokedex（RecyclerFrame 两段 + 详情页） | demo「图鉴」页 | ✅ 改良 | 回收器补上分段：`getSectionCount` / `getItemCountInSection` / `getSectionTitle` / `getItemIdentifierInSection` / 表头行。**接口用 `*InSection` 后缀**——borealis 是重载 `getItemCount()`/`getItemCount(section)`，派生类只重写一个会把另一个名字隐藏掉（C++ 名字隐藏），我们避开这个坑。精灵图是任天堂 IP，用类型色圆 + 名称首字自绘占位，数据自造 |
| Settings（6 种 Cell 全家桶 + 运行时开关） | demo「设置」页 | ✅ |
| About | 不适用 | ➖ | **borealis demo 没有 About tab**：`resources/xml/activity/main.xml` 的 tab 只有 components / layout / transform / text / scroll / pokedex / settings。之前把它列成待补是我记错了，这里更正 |
| （被注释的）Popups / notifications / dialogs | demo「弹层」页 + 「设置」页的 Toast | ✅ | borealis 里这个 tab 是注释掉的（`<!-- <brls:Tab label="@i18n/demo/tabs/popups" /> -->`）；我们做了：Dialog、EditTextDialog（弹层页）与 Notification/Toast（设置页） |
| （被注释的）Horizon / Misc layouts | 不适用 | ➖ | 同样是 borealis 里注释掉的空 tab（没有对应实现可对标）。要做得先定义"Horizon 布局"具体指什么，属于新增需求而不是补齐 |

## 六、无法实现 / 必须说明的

| 项 | 结论 | 原因 |
|---|---|---|
| PS4 / PSV 平台 | **不做** | 需要 Sony 专有 SDK（不公开、需授权），也无法在本机编译与验证 |
| Android / iOS 平台 | **暂不做** | 技术上可行（SDL3 支持），但需要 gradle/Xcode 工程、移动端生命周期与触摸、真机验证；没有设备就只能给"看起来支持"的假象 |
| Metal 后端 | **暂不做** | macOS 已有 GL 后端可用，收益低；等真有需要再补（ImGui 有官方后端，属于可做项） |
| D3D11 后端 | **可做，待定优先级** | Windows 上 D3D11 有实际价值（省 GL 驱动问题），需要 Windows 环境验证 |
| video（视频播放） | **先不做** | borealis 的视频是各平台硬件解码（Switch 用 libnx），桌面没有通用实现；自己做要么引 ffmpeg（体积 + 许可），要么各平台各写一套。**需要你决定**是否引入解码依赖 |
| account 的真实数据 | **平台相关** | Switch 走 `account` 服务；桌面没有统一账号 → 做成"数据源接口 + Switch 实现"，桌面留占位 |
| Switch 真机行为 | **待验证** | swkbd、共享字体、触摸、电池/无线都只能在真机确认；仓库里只能保证编译通过 |

## 七、borealis 设计得不好、我们要改良的点

| borealis 的做法 | 问题 | 我们的做法 |
|---|---|---|
| `Application` / `Style` / `Theme` 全是全局单例 + 字符串 key | 不可测（无窗口测试拿不到干净状态）、拼错 key 只在运行时才炸、无法多实例 | 实例化 `App`、JSON 主题、**key 不存在直接报错**、8 个无窗口测试 |
| 布局全靠 XML + 反射注册 + 每个控件一套 `draw(NVGcontext*)` | 绑死 NanoVG；换渲染后端要改控件；XML 解析期就得知道所有类型 | ImDrawList + `RenderBackend` 抽象（GL/VK/GLES3）；控件外观用 JSON `layers` 描述，换皮肤不改 C++ |
| 主题/样式表没有 UI 缩放 | 逻辑分辨率写死，换窗口大小就得改主题 | `core/ui_scale`：主题按 720p 写，运行时按画布宽等比缩放 |
| 每帧全量重排 | 大列表/动画时浪费 | 延迟布局：`invalidate()` 冒泡标记，一帧最多算一次 |
| 提示是一个全局 `Hint` 单例 + 独立组件 | 与底栏耦合松散、状态在全局 | `HintsBar` 作为底栏左侧内容，按当前焦点的动作重建（签名不变就不重建） |
| 底栏拆成 Hints + BottomBar 两个控件互相切换 | 显隐逻辑互相打架 | 合并为一个 `BottomBar`（左提示 / 右状态） |
| Cell 家族每个都硬编码 XML + 绘制 | 换皮肤要改 C++ | 我们做 Cell 时走 `AppearanceView`（`box` + `layers` + `states`），与其它控件一套机制 |
| `NotificationManager` 静态单例 | 不可多实例、测试困难 | 做成 `App` 上的通知队列 + 外观驱动 |
| 手势用 `new TapGestureRecognizer` 手动挂 | 容易漏 delete | 识别器归 `View` 拥有（`addGesture` 接管生命周期） |
| Battery / Wireless 直接读平台 | 桌面没法显示、无法测试 | 数据源接口 + 默认实现（Switch 真数据、桌面可注入），控件本身可无窗口测试 |
| Yoga 默认 `flexShrink: 0` 未处理 | 内容变高会挤压同列兄弟（我们踩过：按钮行 48 → 28） | `ScrollingFrame` 用 `flex-basis: 0` + `shrink: 1`；并写进 AGENTS 约定 |

### status_area 的两次踩坑（已修，测试覆盖）

| 现象 | 根因 | 修法 |
|---|---|---|
| 桌面上"无数据"时无线图标不隐藏 | `setSnapshot` 在"值没变"时提前 return，跳过了显隐更新 | 显隐先定，再看值有没有变 |
| 开关类图标在某些外观下不可见 | 自绘层没给尺寸时几何会用到整块矩形 | 几何取主题 metric + 右对齐（同 Cell 的开关） |
| 变换里的 NaN 会让控件**静默消失** | demo 在布局算完前用 `getWidth()` 换算位移，NaN 传进了变换，整段顶点变 NaN | 库侧：`View::frame` 检测到 NaN/Inf 就报警并跳过变换（宁可"没生效"，也不要看不见）；应用侧：换算前对尺寸做 isfinite 兜底 |
| **关窗口时崩溃（SIGSEGV）** | 音频设备归 SDL 管，但 `App::~App` 的 `audio::shutdown()` 跑在 `PlatformSDL3` 析构（内部 `SDL_Quit()`）**之后**，`SDL_DestroyAudioStream` 拿到已拆的 SDL | 平台析构里在 `SDL_Quit()` **之前**调 `audio::shutdown()`（App 里那次成空操作）；实测退出码 0 |
| **代码里 `setCornerRadius()` 不生效** | 绘制只读外观层的 `radius`，外观 JSON 写了 `radius: metric.radius.md` 就把代码值压住 → 想画圆形头像画成圆角方块 | 按项目既有约定"显式设置优先于主题"：`setCornerRadius(r, explicit=true)`，外观套用走 `explicit=false`；显式值优先于外观层半径 |
| **`ScrollingFrame::update()` 无头必崩** | 直接读 `ImGui::GetIO().DeltaTime`，无窗口（测试）时没有上下文 → 空指针 | 时间改用框架的 `getFrameDeltaSeconds()`（与 `Animatable`/`KeyRepeater` 同一份时间源）；鼠标输入在无上下文时整体跳过。顺带让回收器的回收行为可以被测试覆盖 |
| 主题缺键静默画成品红 | `ThemeData::color()/number()` 缺键只记 **DEBUG** 日志，默认看不见 | 改成 **WARNING 且同 key 只报一次**（外观 JSON 的引用仍然是编译失败）；加上后立刻抓出本轮 3 个缺键（`color.surface.hover` 等） |
| 改完代码好像没生效 | 21 个目标各挂一条 POST_BUILD `copy_directory` 往同一个 `dist/resources` 写，`-j` 并行时互相踩 → 间歇性 `Error 2`，失败后二进制还是旧的 | 改成**按输出目录只复制一次**（独立目标 + 各目标依赖它）；实测连续 3 次 `-j 8` 无失败 |
| 通知浮层看不见 | 构造时把 `hidden` 与 `shown` 两个外观状态**同时**激活，`hidden` 的透明度 0 把 `shown` 盖掉 | 状态要互斥地切：先撤旧的再开新的（和 Dialog 的 open/closed 一个套路） |
| 窗口建好前 `App::notify` 的通知丢了 | 那时还没有根视图，只能警告丢弃 | 排进 `pendingNotifications`，`setRootView` 之后补发 |

### 手势相关（本轮）

| 现象/设计 | 处理 |
|---|---|
| 拖动一个按钮会顺手把它按下去 | 手势先派发：识别器吃掉 START 就不再当成"点击"触发动作 |
| 滚轮会误触发拖动/轻触 | 单独的 `SCROLL` 状态，只有滚动识别器响应 |
| 换根/销毁后手势目标成野指针 | `resetFocusState()` 里一并清 `gestureTarget`（和模态栈同一个坑） |
| demo 里滑块/手势的 lambda 捕获了构建函数的局部变量（悬空） | 值放到 `shared_ptr` 上，捕获共享指针 |

### debug layer（本轮）

| 现象 | 根因 | 修法 |
|---|---|---|
| 开了调试层却什么都看不到 | `App::setDebugLayerEnabled` 在根视图就绪前被静默忽略（窗口创建前调用） | 记下 `debugLayerRequested`，`setRootView` 之后补开（和通知排队同一套做法） |

### task / 异步图片（本轮）

| 项 | 说明 |
|---|---|
| 线程池 | 默认 2 worker，`std::function` 任务；`shutdown()` 显式关停，`App::~App` 里调用，worker 持有共享状态，join 后才退出 |
| 主线程回调 | `postToMain` + `App::frame` 每帧 `pumpMainThread()`；回调里可以安全碰视图/纹理/ImGui |
| 异步图片 | 后台 `decodeImageFile`（文件 IO + 解码），主线程 `loadTextureFromPixels` 上传；**按代数丢弃过期请求**；纹理仓库没就绪时保留 `pendingPath` 交给 `update()` 重试 |
| 新增公开 API | `loadTextureFromPixels(rgba, w, h)`（只上传不解码） |

### demo 页面覆盖（本轮）

对标 `resources/xml/activity/main.xml` 的 tab 列表：components / layout / transform / text /
scroll / pokedex / settings（popups、hos_layout、misc_layouts、misc_components 在 borealis
里是注释掉的）。已覆盖：控件(=components)、布局、变换、文本(=text)、滚动(=scroll)、
设置(=settings)，另外多了主题/JSON/导航/弹层四个页面（弹层即 borealis 注释掉的 popups 一类）。

### 回收器分段（本轮）

| 项 | 说明 |
|---|---|
| 行模型 | 表头行 + 条目行展开成一张平表（`Row{kind, section, itemIndex, top, height}`），可视区取数、回收、定位全部按行号走 |
| 段头 | `getSectionTitle(section)` **返回空串 = 不占表头行**；段头 cell 用 `RecyclerSectionHeader`，标题经**虚函数** `onSectionTitle` 下发（不用 RTTI，Switch 构建带 `-fno-rtti`） |
| 兼容 | 单段老写法（只重写 `getItemCount()`）一行都不用改 |
| 单元 | `RecyclerCell::setSectionIndex(section, index)`，`getSection()/getIndex()`；`getIndex()` 在单段时仍是行号 |

## 七点五、实现过程中顺带修掉的真 bug

| 现象 | 根因 | 修法 |
|---|---|---|
| 列表里从"内嵌滑块的单元格"往下走，焦点卡住不动 | `Box::getNextFocus` 把"当前焦点自己"（经 `getDefaultFocus()` 解析回来）当成候选，用最低分短路了整个搜索，于是永远不往父容器冒泡 | 解析出的候选等于 `currentView` 时跳过；`widget_test` 加了 4 项跨容器导航回归 |
| 设置页的开关/单选圆点画不出来 | 基类 `Cell` 构造期调 `applyTheme()`，此时**虚函数不会派发到子类**，开关/单选的配色一直是 0 | 派生类构造末尾自己再调一次 `applyTheme()`；测试断言"构造后配色已就绪" |
| 开关的自定义层铺满整行 | 自定义层没在 JSON 里给尺寸，几何用了整块矩形 | 自绘时按主题 `metric.switch.*` / `metric.radio.*` 取几何并右对齐；配色在 `applyTheme` 时取好（draw 里不查主题） |

## 八、执行批次与验收

| 批次 | 内容 | 验收 |
|---|---|---|
| **P0 控件补齐** | 6 个 Cell、Battery/Wireless 图标化、Notification、手势（tap/pan/scroll）、View 变换（scale/rotate/skew/fontScale） | 每个控件进无窗口测试；GL/VK 零警告；Switch 构建通过；demo 有对应演示页 |
| **P1 核心能力** | task/thread + 异步图片加载、audio（SDL3 + Switch）、debug_layer（wireframe）、event 补齐 | 同上；异步加载有压力测试 |
| **P2 平台扩展** | D3D11（待定）、Android/iOS 骨架（无法验证，需你确认）、video（需你决定是否引解码依赖） | 平台相关验收只能到"编译通过 + 文档写明未验证" |
| **P3 demo 对标** | borealis demo 的 7 个启用页 + 3 个注释页全部做出来（用我们的 JSON 主题 + 外观驱动） | 每页可演示；双后端跑得起来 |

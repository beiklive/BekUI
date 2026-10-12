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
| **`views/widgets/account`** | — | ❌ 待补 | 头像 + 昵称；数据源要可插拔（Switch 用 account 服务，桌面无） |
| `views/debug_layer`（画视图边界/wireframe） | `views/debug_window`（日志窗口） | ⚠️ 部分 | 日志有了，缺"框出每个 View + 显示尺寸"的调试层 |
| — | `views/json_cell` | ➕ 多出 | JSON 模板驱动的列表项 |
| — | `widgets/appearance_view` / `status_area` / `hints_bar` | ➕ 多出 | 外观驱动基类与底栏两块内容 |

## 二、View 能力

| 能力 | borealis | bklv | 状态 |
|---|---|---|---|
| translate | `setTranslationX/Y` | 同名 | ✅ |
| **scale** | `setScaleX/Y` | — | ❌ 待补 |
| **rotate** | `setRotate` | — | ❌ 待补 |
| **skew** | `setSkewX/Y` | — | ❌ 待补 |
| **fontScale** | `setFontScaleX/Y` | — | ❌ 待补 |
| 可见性 VISIBLE/INVISIBLE/GONE | 同名 | 同名 | ✅ |
| 动作系统 registerAction / 冒泡 | ✅ | ✅（沿父链冒泡） | ✅ |
| 手势（tap / pan / scroll） | `GestureRecognizer` + 3 个实现 | — | ❌ 待补 |
| 数据绑定 / cache_helper | `bind` / `cache_helper` | `json_cell` 的绑定回调 | ⚠️ 部分 |

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
| **activity**（Activity 栈 + 转场动画） | `AppletFrame` 的内容栈 | ⚠️ 无独立 Activity 与转场动画 |
| **audio**（8 种系统音效） | — | ❌ 待补（SDL3 audio + Switch audren） |
| **video** | — | ❌ 见"无法实现"一节 |
| **notification_manager**（Toast） | — | ❌ 待补 |
| **task / thread**（异步任务、线程池） | — | ❌ 待补（异步加载图片是真需求） |
| **event**（事件总线） | 动作系统 | ⚠️ 部分 |
| **gesture / touch/** | — | ❌ 待补 |
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
| Basic components（HScroll / 5 种按钮 / Slider / Label 对齐 / 图片 5 种缩放） | 控件页（部分） | ⚠️ 待补：图片缩放 5 例、按钮样式齐全 |
| Layout and alignment（flex 四组 + grow + wireframe） | 布局页（部分） | ⚠️ 待补 wireframe（调试层） |
| **Transform test**（translate/scale/skew/rotate/fontScale + 多段缓动） | — | ❌ 依赖 View 变换 |
| Text test（宽高/对齐/singleLine 实时调） | 文本页（部分） | ⚠️ 待补交互式调节 |
| Scroll test | 列表/滚动页 | ⚠️ 待补独立页 |
| Pokedex（RecyclerFrame 两段 + 详情页） | 列表页（Recycler） | ⚠️ 待补分段与详情页导航 |
| Settings（6 种 Cell 全家桶 + 运行时开关） | 主题/控件页（部分） | ❌ 依赖 cells |
| About | — | ❌ 待补 |
| （被注释的）Popups / notifications / dialogs | 弹层页（部分） | ⚠️ 待补 Notification |
| （被注释的）Horizon / Misc layouts | — | ❌ 待补 |

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
| 通知浮层看不见 | 构造时把 `hidden` 与 `shown` 两个外观状态**同时**激活，`hidden` 的透明度 0 把 `shown` 盖掉 | 状态要互斥地切：先撤旧的再开新的（和 Dialog 的 open/closed 一个套路） |
| 窗口建好前 `App::notify` 的通知丢了 | 那时还没有根视图，只能警告丢弃 | 排进 `pendingNotifications`，`setRootView` 之后补发 |

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

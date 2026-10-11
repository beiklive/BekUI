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
| 图形后端 | OpenGL 3.2 Core、Vulkan、OpenGL ES 3、空后端；编译期选择 |
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

现在 bklv 是「跟着这个仓库一起编译的源码」，不是库：没有 `install()`，
没有导出的 CMake target，`src/app/`（总览程序的应用层）混在库源码树里。

- [ ] **库与 demo 分层**
      `src/bklv` 是库；`src/app`（页面 / tab / 列表项）是应用层 demo 代码。
      把 `src/app` 移到 `demo/app/`，让库源码树只含库。
      *验收*：`src/` 下只有 bklv；`demo/` 能独立编译。
- [ ] **导出 CMake target**
      提供 `bklv::bklv`（STATIC 库 + 公共头 + 传递依赖），并支持
      `add_subdirectory` 与 `find_package(bklv)` 两种用法。
      *验收*：另建一个空工程，`find_package(bklv REQUIRED)` +
      `target_link_libraries(app PRIVATE bklv::bklv)` 后能编出可运行程序。
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
- [ ] **umbrella 头**
      `bklv/bklv.hpp` 一次引入常用类型，降低使用者的 include 负担。
      *验收*：示例工程只 include 它就能写出一个界面。
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
- [ ] **i18n**
      `Platform::getLocale()` 已经有了，缺文案表与切换机制。
      borealis 用 XML，bklv 按约定用 C++ 表（`bklv::i18n::get("key")`）。
      *验收*：切换语言后界面文案即时更新。
- [ ] **手势层**
      Switch 触摸屏的原始状态已经在 `imgui_impl_libnx.cpp` 里取到了，
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

## P3 —— 平台与工程

- [ ] **CI**
      GitHub Actions 矩阵：{macOS, Ubuntu, Windows} × {GL, Vulkan}，
      加上 `-DBKLV_TARGETS=main` 的快速通道。目标是零警告 + 全部目标能链接。
      *验收*：PR 上能看到矩阵全绿。
- [ ] **单元测试**
      现在没有测试，验证全靠手动跑 demo。优先给纯逻辑的部分加：
      `RecyclerFrame` 的可见区间与回收、`Label` 的断行、焦点导航的几何打分、
      `ScrollingFrame` 的偏移与惯性。
      *验收*：`ctest` 能跑，且这些用例能在无图形环境下执行（用空后端）。
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
| 8 | `src/app` 是 demo 代码却在 `src/` 下 | 使用者容易误以为它是库的一部分 | P0 第 1 条 |
| 9 | `demo_01` ~ `demo_05` 与 bklv 无关 | 它们是纯 ImGui / Yoga / Tweeny 的学习示例，走的是 `demo_common.h` 那条路 | 库化后移到 `demo/learn/` 并在 README 里标明定位 |
| 10 | 多后端产物目录靠不同 build 目录区分 | 早先空后端与 GL 共用 `dist/` 会互相覆盖，而 CMake 认为目标最新不会重链，得到「空的 GL 版程序」且无提示 | 已改为 `dist/`、`dist/vk/`、`dist/null/`、`dist/switch/`；改构建时留意别再合并 |

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

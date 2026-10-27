# demo

演示程序：一个窗口 + 顶栏 + **标签页**（`bklv::TabView`）+ 底栏。
它只依赖 bklv 的公开接口（`bklv/bklv.hpp` 里的那套），GL 与 Vulkan 两种后端都能编。

```
demo/
  main.cpp            入口：解析参数、构造 DemoApp、run
  demo_app.hpp/.cpp   外壳：顶栏、标签条、TabView 装配、主题轮换
  tabs/
    buttons_tab.*     第 1 页「按钮」：Button 家族 + 点击反馈 + 切换标签条排列
    layout_tab.*      第 2 页「布局」：横排 / 竖排 / grow / 内边距居中 / 主轴对齐
    animation_tab.*   第 6 页「动画」：Transform 变换（Play/Reset + 滑条）+ JSON transitions.show
  layout_kit.hpp      "一节"与"彩色矩形"两个排版小工具（只在 demo 里用）
  style.hpp           从主题取页面底色与文字色的小工具
```

六个标签页**常驻**：切走只是 `GONE`，对象和状态都在，切回来滚动位置和焦点也还是原样
（`TabView` 负责记忆）。这跟内容层下钻（`Shell` 的 `ContentStack`：新层压上来、旧的出场即销毁）不是一回事。

## 跑起来

```bash
cmake -S . -B build/gl -G "Visual Studio 17 2022"
cmake --build build/gl --config Release
./dist/demo.exe
```

Vulkan 版把配置换成 `-DBKLV_GRAPHICS=vk`，产物落在 `dist/vk/demo.exe`。

命令行参数：

| 参数 | 作用 |
|---|---|
| `--theme <name>` | 启动主题（`default` / `glass`） |
| `--tab <name>` | 启动标签页：`buttons`（默认）/ `layout` / `list` / `container` / `debug` / `animation` |
| `--tabbar <top\|left>` | 标签条排列：`top`（默认，顶部水平）/ `left`（左侧竖直） |
| `--screenshot <png>` | 渲染若干帧后截图存 PNG 并退出（无人值守验证） |
| `--frames <n>` | 截图前等多少帧，默认 3 |
| `--size <宽x高>` | 窗口尺寸，默认 `1280x720` |

```bash
./dist/demo.exe --screenshot shot.png --frames 10
./dist/demo.exe --tab layout --screenshot layout.png --frames 14
./dist/demo.exe --tab animation --screenshot animation.png --frames 20
./dist/demo.exe --tab layout --tabbar left
```

## 加一个标签页

1. 在 `demo/tabs/` 下建 `xxx_tab.hpp` / `.cpp`，导出一个工厂：

   ```cpp
   bklv::View* buildXxxTab(demo::DemoApp& app);
   ```

2. 实现里返回一棵子树即可，通常用 `ScrollBox(Axis::COLUMN)` 当根，里面按"节"摆东西
   （`layout_kit.hpp` 的 `makeSection()` / `makeColorRect()` 就是干这个的）。

3. 在 `demo_app.cpp` 的 `buildShell()` 里加两项：
   `titleLabel` 的候选标题数组 `kTabTitles[]` 和 `tabBar->addItem(标题, 图标码位, "id")`、
   `tabs->addTab("id", buildXxxTab(*this))`。

4. 把新的 `.cpp` 加进根 `CMakeLists.txt` 的 `add_executable(demo ...)`。

## 几个约定

- **标签条与标签页双向同步**：`tabBar->setOnTabSelected` 切 `tabs->setActiveTab`；
  `tabs->setOnTabChanged` 反过去刷 `tabBar->setActiveIndex(index, false)` 和顶栏标题。
  代码里主动切页也走这一对回调，别只改一边。
- **标签条排列**：`TabBar::Orientation` 换外观变体（`base/tab/tabbar.vertical`、
  `tabitem.vertical`），同时要把外层 `Box` 的轴从 `COLUMN` 换成 `ROW`
  —— 见 `DemoApp::setTabOrientation()`。第 1 页里那个「切换标签条排列」按钮就是调它。
- **换肤后写死的文字不会自己变**：第 1 页的「当前主题」这类文字要在重建时读
  `app.getThemeName()`；`TabView` 常驻，所以换肤不会重建页面（这也是常驻的代价）。
- 新加资源要登记进主题的 `theme.json` 的 `files[]`，否则 `scripts/check_theme.py` 会报清单不一致。

## 外壳：Shell 的五层

根视图是 `bklv::Shell`（`bklv/ui/shell/shell.hpp`），它在构造时按顺序建好五层，
层次就是绘制顺序：

| 层 | 放什么 | demo 里怎么用 |
|---|---|---|
| `getBackdropLayer()` | 背景（底色 / 底图），绝对定位铺满、不参与布局 | 铺页面底色 |
| `getContentLayer()` | 正文内容，正常参与 Yoga 布局（它本身是竖直 Box） | 顶栏 + 标签区（下边留 100px 给底栏） |
| `getOverlayLayer()` | 覆盖层：页头 / 底栏 / 对话框 | 底栏 `BottomBar`（贴底、靠右显示提示） |
| `getHudLayer()` | 浮层，不吃输入：toast、常驻提示、焦点框 | 焦点框与通知由 Shell 自己挂在这里 |
| `getDebugLayer()` | 调试覆盖层，永远最后画 | 可放 `DebugLogView` |

```cpp
this->root = new bklv::Shell();
this->root->getContentLayer()->addView(header);
this->root->getContentLayer()->addView(this->tabLayout);
this->root->getBackdropLayer()->setBackgroundColor(pageBackgroundColor());
```

另外 Shell 还提供 `setSafeAreaInsets()`（五层统一内缩，Switch 的屏幕边距用得上）、
`fillLayer(view)`（把一个视图铺满某一层）、`pushContentView()` / `popContentView()`
（内容层层级栈，带入场出场转场与焦点、滚动记忆）和过渡宿主（`getTransitionHost()`）。

## 对话框与列表容器

第 1 页底部的「对话框」「列表容器」两个按钮演示 `ui/widgets/container/` 的两个派生容器：

- `bklv::Dialog`：全屏容器 = 玻璃遮罩 + 居中面板。面板里是「图标 + 标题」表头、
  可滚动内容区、按钮行；`addButton("取消", ...)` 的按钮外观是 `base/button/button.dialog`。
  打开 / 关闭走 `open()` / `close()`（内部是 `App::pushOverlay` / `popOverlay`），
  打开时焦点自动落到第一个按钮上、关闭后回到原来那个按钮；
  点遮罩关闭，B 键也能关（`pushOverlay` 的 `closeOnBack`）。
  底部按钮**均匀分配**（每个 `grow = 1`），只有一个按钮时铺满整行。
- `bklv::ListBox`：只是一个纵向滚动的容器，`addItemText()` / `addItem(View*)` 往里放东西，
  焦点移动时会把那一项滚进可视区。**高度要有界**（示例里 `setHeight(420)`），
  否则它算不出视口高度。

遮罩的高斯模糊来自玻璃层，而玻璃默认是关的（库不改变宿主观感），
`demo_app.cpp` 启动时 `bklv::glass::getGlassService().setEnabled(true)` 打开了它
（也可以 `root->setGlassEnabled(true)` —— Shell 有转发）。

## 底栏与提示条

底栏是 `bklv::BottomBar`（`bklv/ui/widgets/controls/hint_bar.hpp`），挂在覆盖层贴底，
内部自己套一个 `HintBar`，内容来自 `InputManager::getActiveHints()` ——
沿当前焦点的父链收集按键提示（按按键去重、`START` 排最前、`B`/`A` 排最后、
没有 A 时补一条「Ⓐ 确定」）。

```cpp
this->bottomBar = new bklv::BottomBar();
this->bottomBar->setPositionType(bklv::PositionType::ABSOLUTE);
this->bottomBar->setPositionBottom(0);
this->root->getOverlayLayer()->addView(this->bottomBar);
this->root->getContentLayer()->setPadding(0, 0, 100, 0);   // 给底栏让位
```

三个类都**不吃焦点**：`Hint` 覆写 `isPointerTarget()` 并挂一个 `TapGestureRecognizer`，
所以提示条能点（点了执行对应动作、焦点不动），方向键和焦点框却当它不存在。
底栏高度/分隔线/内边距全在 `resources/theme/default/base/hint/{bottombar,hintbar,hintitem}.json`，
`Hint` 的图标层必须用开关图标字体（`setLayerIconFontKind("icon", IconFontKind::SWITCH)`），
否则同一个码位会画成 Material 的灯泡/书本。

## 焦点框

焦点框归 Shell 所有：`Shell` 构造时把 `FocusRingView` 挂在 HUD 层上，
demo 只需要 `root->setFocusRingEnabled(true)`（默认就是开的）。

它自己是绝对定位铺满父容器、不吃输入，每帧问输入管理器"当前焦点是谁"、
再问那个控件"你是什么形状"（`View::getFocusOutline()`）—— 控件完全不知道它的存在。
样式写死在代码里（`root->getFocusRingStyle()` 可调），不跟主题 JSON 走。

## 换肤与资源

主题来自 `<exe 目录>/resources/theme/`（构建时从仓库 `resources/` 先删后拷）。
`themes.json` 的 `available` 决定能切哪些主题，缺的键回退 `default`。
玻璃主题只覆盖少量 token 和外观，看 `resources/theme/glass/`。

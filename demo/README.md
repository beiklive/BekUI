# demo

演示程序的最小框架：一个窗口 + 顶栏 + 内容区 + 底栏，中间是**页面栈**。
它只依赖 bklv 的公开接口（`bklv/bklv.hpp` 里的那套），GL 与 Vulkan 两种后端都能编。

```
demo/
  main.cpp            入口：解析参数、构造 DemoApp、run
  demo_app.hpp/.cpp   外壳与页面栈（Shell 五层容器、顶栏、内容区、底栏、主题轮换）
  page.hpp            页面接口（getTitle / build）
  style.hpp           从主题取页面底色与文字色的小工具
  pages/
    home_page.*       首页：导航、换肤，两种通知（图标 / 图片）+ 连发 5 条（看堆叠与逐条收回）
    settings_page.*   设置页：演示压栈进入、出栈返回
```

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
| `--page <name>` | 启动页面：`home`（默认）/ `settings` |
| `--screenshot <png>` | 渲染若干帧后截图存 PNG 并退出（无人值守验证） |
| `--frames <n>` | 截图前等多少帧，默认 3 |
| `--size <宽x高>` | 窗口尺寸，默认 `1280x720` |

```bash
./dist/demo.exe --screenshot shot.png --frames 10
./dist/demo.exe --theme glass
./dist/demo.exe --page settings --screenshot settings.png --frames 12
```

## 加一个页面

1. 在 `demo/pages/` 下建 `xxx_page.hpp` / `.cpp`，继承 `demo::Page`：

   ```cpp
   class XxxPage : public demo::Page
   {
     public:
       const char* getTitle() const override { return "标题"; }
       bklv::View* build(demo::DemoApp& app) override;
   };
   ```

2. `build()` 里用 `bklv::Box` / `bklv::Label` / `bklv::TextButton`（或 `IconButton` / `IconTextButton`）拼一棵子树返回。
   每次进入页面都会重新 build，旧子树随出栈销毁 —— 要跨次保留的数据放页面成员里。

3. 从某个按钮的回调里压栈：

   ```cpp
   button->setOnClick([&app]() { app.pushPage(new XxxPage()); });  // button 是 bklv::TextButton*
   ```

4. 把新的 `.cpp` 加进根 `CMakeLists.txt` 的 `add_executable(demo ...)`。

## 三个约定

- **导航必须走 `pushPage()` / `goBack()`**：它们把动作推迟到下一帧开头执行。
  按钮回调里直接改页面栈会把正在执行的子树删掉（call-after-free）。
- 页面里写死的状态文字（比如「当前主题」）在换肤后不会自己变，
  `cycleTheme()` 会重建当前页，所以拼文字时读 `app.getThemeName()` 即可。
- **装完页要自己补一次焦点**：`App::setRootView()` 只在换根那一刻找默认焦点，
  而这里的页面是在第一帧（或每次换页）才进树，那次查找会拿到空。
  `showCurrentPage()` 末尾因此有一句 `giveFocus(root->getDefaultFocus())`
  （`giveFocus` 默认静音，不会出声）；不补的话既没有焦点框，方向键也无从下手。

## 外壳：Shell 的五层

根视图是 `bklv::Shell`（`bklv/ui/shell/shell.hpp`），它在构造时按顺序建好五层，
层次就是绘制顺序：

| 层 | 放什么 | demo 里怎么用 |
|---|---|---|
| `getBackdropLayer()` | 背景（底色 / 底图），绝对定位铺满、不参与布局 | 铺页面底色 |
| `getContentLayer()` | 正文内容，正常参与 Yoga 布局（它本身是竖直 Box） | 顶栏 + 内容区（下边留 100px 给底栏） |
| `getOverlayLayer()` | 覆盖层：页头 / 底栏 / 对话框 | 底栏 `BottomBar`（贴底、靠右显示提示） |
| `getHudLayer()` | 浮层，不吃输入：toast、常驻提示、焦点框 | 焦点框由 Shell 自己挂在这里 |
| `getDebugLayer()` | 调试覆盖层，永远最后画 | 可放 `DebugLogView` |

```cpp
this->root = new bklv::Shell();
this->root->getContentLayer()->addView(header);
this->root->getContentLayer()->addView(this->content);
this->root->getBackdropLayer()->setBackgroundColor(pageBackgroundColor());
```

另外 Shell 还提供 `setSafeAreaInsets()`（五层统一内缩，Switch 的屏幕边距用得上）、
`fillLayer(view)`（把一个视图铺满某一层）和过渡宿主（`getTransitionHost()`，
换页动画期间存在、空了自动摘）。

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
新加资源文件记得同时登记进主题的 `theme.json` 的 `files[]`，否则 `scripts/check_theme.py` 会报清单不一致。

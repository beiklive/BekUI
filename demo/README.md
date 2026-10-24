# demo

演示程序的最小框架：一个窗口 + 顶栏 + 内容区 + 底栏，中间是**页面栈**。
它只依赖 bklv 的公开接口（`bklv/bklv.hpp` 里的那套），GL 与 Vulkan 两种后端都能编。

```
demo/
  main.cpp            入口：解析参数、构造 DemoApp、run
  demo_app.hpp/.cpp   外壳与页面栈（窗口、顶栏、内容区、底栏、主题轮换）
  page.hpp            页面接口（getTitle / build）
  style.hpp           从主题取页面底色与文字色的小工具
  pages/
    home_page.*       首页：导航 / 换肤 / 通知三个入口
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

## 焦点框

`demo_app.cpp` 的 `buildShell()` 在根视图最后加了一个 `bklv::FocusRingView`：

```cpp
this->focusRing = new bklv::FocusRingView();
this->root->addView(this->focusRing);   // 加在最后 → 画在所有内容之上
```

它自己是绝对定位铺满父容器、不吃输入，每帧问输入管理器"当前焦点是谁"、
再问那个控件"你是什么形状"（`View::getFocusOutline()`）—— 控件完全不知道它的存在。
样式写死在代码里（`getRingStyle()` 可调），不跟主题 JSON 走。

## 换肤与资源

主题来自 `<exe 目录>/resources/theme/`（构建时从仓库 `resources/` 先删后拷）。
`themes.json` 的 `available` 决定能切哪些主题，缺的键回退 `default`。
新加资源文件记得同时登记进主题的 `theme.json` 的 `files[]`，否则 `scripts/check_theme.py` 会报清单不一致。

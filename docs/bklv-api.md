# bklv 速查

`bklv/` 是从 [borealis](https://github.com/xfangfang/borealis)（Apache-2.0，wiliwili 使用的框架）
移植过来的视图层。这份文档是写代码时的速查表：文件职责、常用调用、以及踩过的坑。

设计上跟 borealis 保持一致：**没有独立的渲染树，一个 View 持有一个 `YGNode`**，
布局由 Yoga 递归计算，绘制由 `View::draw` 递归完成。

绘制后端从 NanoVG 换成了 ImGui 的 `ImDrawList`，并按需砍掉了这次用不到的
阴影、渐变、焦点高亮、点击动画、XML 布局和平台抽象层。

## 文件职责

下表路径都相对 `bklv/`。

| 文件 | 职责 |
|---|---|
| `core/geometry.hpp` | Point / Size / Rect 与颜色辅助 |
| `core/theme.hpp/.cpp` | 样式数值表与颜色主题 |
| `core/time.hpp/.cpp` | `Animatable` 补间（自带实现，不依赖 tweeny） |
| `core/font.hpp/.cpp` | 字体加载：桌面读系统字体文件；Switch 用 `pl` 服务取内置共享字体并合并 fallback 链；都没有则退回内置字体 |
| `core/platform.hpp/.cpp` | **平台接口**：窗口、事件循环、子系统工厂、系统能力 |
| `platforms/sdl3_platform.*` | macOS/Linux/Windows 的 SDL3 平台实现 |
| `platforms/switch_platform.*` | Switch 的 libnx 平台实现（EGL/GLES3 + pad + 电源/网络/语言） |
| `core/gamepad.hpp/.cpp` | 手柄输入抽象（桌面 SDL3 / Switch libnx） |
| `core/json_view.hpp/.cpp` | JSON 视图描述：类型注册、模板缓存、实例化、include、错误 |
| `core/attributes.hpp/.cpp` | 属性值与属性表：类型检查、`@i18n/` `@style/` `@theme/` `@res/` 前缀 |
| `views/json_cell.*` | JSON 模板驱动的列表项（数据绑定 + 焦点视觉） |
| `widgets/appearance_view.*` | 外观驱动控件基类：解释 layers、状态动效、固有尺寸测量 |
| `widgets/rectangle.*` `widgets/button.*` `widgets/header.*` `widgets/bottom_bar.*` `widgets/slider.*` `widgets/tab_frame.*` `widgets/sidebar.*` | 外观驱动的控件（绘制结构在主题 JSON 里） |
| `core/i18n.hpp/.cpp` | 文案系统：JSON 资源、三级回退、查找缓存、运行中切语言 |
| `core/resource.hpp/.cpp` | 资源路径解析：桌面用可执行文件目录，Switch 用 `romfs:/` |
| `core/graphics.hpp/.cpp` | **渲染后端接口** + 纹理管理，上层与图形 API 的唯一分界 |
| `backends/opengl_backend.*` | 桌面 OpenGL 后端 |
| `backends/vulkan_backend.*` | Vulkan 后端（动态渲染优先，退到 render pass；交换链重建、纹理上传） |
| `backends/gles3_backend.*` | Switch 的 EGL + OpenGL ES 3 后端 |
| `backends/imgui_impl_libnx.*` | ImGui 的 libnx 平台后端（把按键/触摸喂进 ImGuiIO） |
| `backends/gl_loader.hpp/.cpp` | 跨平台 GL 入口：macOS framework / Linux libGL 直接用原型，Windows 用运行时加载（复用 ImGui 自带的 imgl3w，不引入 glad/gl3w） |
| `backends/gles3_loader.hpp` | Switch 的 GLES3 头 |
| `core/image_decoder.hpp/.cpp` | 图片解码抽象：macOS 走 ImageIO，其余平台走 stb_image |
| `core/resource.hpp/.cpp` | 资源路径解析：桌面用可执行文件目录，Switch 用 `romfs:/` |
| `core/view.hpp/.cpp` | 视图基类：Yoga 样式透传、布局失效、绘制模板、测量回调 |
| `core/box.hpp/.cpp` | 容器：增删子节点时同步 Yoga 树、Flexbox 属性、递归绘制 |
| `core/app.hpp/.cpp` | 帧循环：ImGui 上下文、布局、绘制调度。窗口由 `Platform` 负责 |
| `core/logger.hpp/.cpp` | 日志：级别过滤、时间戳、文件行号、环形缓冲（日志窗口读它） |
| `core/input.hpp/.cpp` | 输入与焦点：hitTest、方向导航、动作注册、手柄 |
| `views/label.hpp/.cpp` | 文本：自动换行、单行省略号、对齐、排版缓存 |
| `views/scrolling_frame.hpp/.cpp` | 纵向滚动：惯性、拖拽、滚动条、焦点自动滚动 |
| `views/h_scrolling_frame.hpp/.cpp` | 横向滚动 |
| `views/recycler.hpp/.cpp` | 虚拟列表：对象池、复用、视口预取与回收 |
| `views/image.hpp/.cpp` | 图片：四种缩放模式、圆角 |
| `views/progress_spinner.hpp/.cpp` | 加载转圈 |
| `views/hints.hpp/.cpp` | 底部提示栏 |
| `views/debug_window.hpp/.cpp` | 调试窗口：ImGui 窗口显示日志流与运行状态 |

## 布局（Box）

以下属性全部直接透传给 Yoga：

```cpp
Box* box = new Box(Axis::ROW);          // ROW 横向 / COLUMN 纵向
box->setPadding(16.0f);                  // 内边距，也支持 4 边分别设置
box->setGap(14.0f);                      // 相邻子节点间距
box->setJustifyContent(JustifyContent::SPACE_BETWEEN);  // 主轴分布
box->setAlignItems(AlignItems::CENTER);  // 交叉轴对齐
Box* child = new Box(Axis::COLUMN);
child->setDimensions(90.0f, 90.0f);      // 固定尺寸
child->setGrow(1.0f);                    // 或按比例分配剩余空间
child->setShrink(0.0f);                  // 固定宽度不被压缩
child->setMargin(8.0f);                  // 外边距
box->addView(child);
```

视图树管理：`addView(view[, position])`、`removeView(view, free)`、`clearViews()`、
`getView(id)`。**增删子节点时 `children` 与 Yoga 节点树必须同步更新**，
否则布局结果会和 C++ 对象树不一致；子节点析构时会自动从父节点摘除。

## 交互与内容控件

```cpp
// 输入与焦点：View 自己注册动作，提示栏会自动显示
cell->registerAction(ControllerButton::A, "播放", [](View* v) { /* ... */ });
bool focusable = cell->isFocusable();          // 想被方向键选中就重写它

// 纵向滚动
ScrollingFrame* frame = new ScrollingFrame();
frame->setHeight(300.0f);
Box* content = new Box(Axis::COLUMN);
content->setHeight(40.0f * 100);               // 内容必须显式给高度
frame->setContentView(content);

// 虚拟列表：1000 条数据只创建约 8 个 cell
RecyclerFrame* list = new RecyclerFrame();
list->setItemHeight(80.0f);
list->setPreFetchLine(1);
list->registerCell("default", [] { return new MyCell(); });
list->setDataSource(&mySource);                // 实现 getItemCount / getItemIdentifier

// 图片
Image* image = new Image();
image->setDimensions(96.0f, 60.0f);
image->setCornerRadius(6.0f);
image->setScalingType(ImageScalingType::FILL); // FIT / FILL / STRETCH / CENTER
// 路径相对"资源目录"：桌面 = 可执行文件目录（构建会复制 resources/ 过去），
// Switch = romfs:/（资源打进 nro）。setImageFromFile 内部会解析，不用自己拼。
image->setImageFromFile("resources/testcard.png");
```

键盘：方向键或 WASD 导航，回车/空格确认，Esc 返回。
鼠标点击默认**只聚焦不执行**动作；想点一下就执行，在控件构造函数里设
`onPointerAction = true`（按钮、tab 都这么做）。列表项保持默认，避免误触发。

## 文案（i18n）

资源放 `resources/i18n/<locale>.json`，key 用 `/` 分层：

```json
{ "demo": { "welcome": "欢迎使用 bklv", "list_count": "共 {} 项" } }
```

```cpp
bklv::getStr("demo/welcome");              // "欢迎使用 bklv"
bklv::getStr("demo/list_count", 12);       // "共 12 项"（{} 按顺序替换）
bklv::getStr("demo/nope");                 // 未命中返回 key 本身
bklv::setLocale("en-US");                  // 运行中切换
bklv::getLocale();                         // 规范化后的当前语言
```

三条要记住的：

1. **文案在控件构造时就解析成字符串**，改语言不会自动重译已有视图——
   要重新 `setText()` 或重建视图树（`demo/app/pages/page_text.cpp` 里有一个
   「切换语言」按钮演示了显式重译）。
2. **语言名是规范化的**（`zh-Hans` / `zh-Hant` / `en-US` / `ja-JP` / `ko-KR`）。
   桌面 SDL 给的是 `zh-CN`、Switch 给的是 `zh-Hans`，都经 `canonicalLocale()` 归一，
   所以资源文件只需要一套命名。
3. **不依赖目录枚举**：按名字读 `<locale>.json`；要拆文件就在主文件里写
   `"$files": ["hints.json"]` 清单（romfs 上的目录迭代真机未验证，所以不扫描目录）。

`initI18n()` 在 `App` 构造时调用，早于视图树构建；也可以在测试里单独调
`setLocale()`，默认语言表是懒加载的。

## JSON 视图描述

用 JSON 描述界面，运行时装成视图树。属性直接平铺在节点上，特殊键只有四个
（`type` / `children` / `include` / `bind`）：

```json
{
  "type": "Box", "axis": "row", "gap": 12, "padding": 12,
  "backgroundColor": "#252a33", "cornerRadius": 10,
  "children": [
    { "type": "Image", "src": "@res/testcard.png", "width": 96, "height": 54,
      "cornerRadius": 8, "scalingType": "fill" },
    { "type": "Label", "text": "@i18n/json/card_title", "fontSize": 16, "singleLine": true }
  ]
}
```

```cpp
bklv::View* view = bklv::createFromJSONFile("resources/views/page_json.json");
if (view != nullptr)
    box->addView(view);           // 失败时返回 nullptr，原因已在日志里
```

值可以写原生 JSON 类型，也可以写带单位的字符串：
`24` 与 `"24px"` 等价，`"50%"` 走百分比，`"auto"` 由内容决定，`true`/`false` 是布尔。

四条要记住的：

1. **两段式**。`ViewTemplate` 是可缓存的"解析结果"，`instantiate()` 才把 `@i18n/`
   这类上下文相关的前缀解析掉。同一模板实例化多次只解析一次
   （`resources/views/list_item.json` 被 8 个 cell 共用）。
2. **路径写全**：`createFromJSONFile("resources/views/x.json")`——和图片一样带 `resources/`
   这一层（桌面解析到可执行文件目录，Switch 在 romfs）；属性里的 `@res/x` 则相对
   `resources/` 目录。
3. **出错不会崩**：语法错误带行号、结构错误带 `json_pointer` 路径，都返回 `nullptr`；
   未知属性只是警告并提示最接近的属性名。多打一个键不会让整个界面建不出来。
4. **新控件要能被 JSON 用**，重写 `attributes()` 返回一张静态表即可；公共属性
   （尺寸/间距/定位/外观/可见性）在 `View` 层已经都有。

列表项模板配合数据绑定（JSON 只管字段名，取值在 C++ 侧）：

```json
{ "type": "Label", "bind": "title" }
```

```cpp
auto tmpl = bklv::ViewTemplate::load("resources/views/list_item.json");
list->registerCell(bklv::RecyclerDataSource::DEFAULT_CELL_ID, [tmpl] {
    return bklv::JSONRecyclerCell::create(tmpl, [](const std::string& field, size_t index) {
        if (field == "title") return titles[index];
        return std::string();
    });
});
```

`bind` 的默认目标属性：`Label` → `text`，`Image` → `src`；其他类型要显式写 `bindTo`。

## 布局与失效

布局是**延迟**的：`View::invalidate()` 冒泡到根只是标记"要重算"，真正的一次
`YGNodeCalculateLayout` 发生在 `App::frame` 里，一帧最多一次。

| 触发条件 | 谁来触发 |
|---|---|
| 视图改了（setText / setWidth / addView / …） | 各 setter 内部 `invalidate()` |
| 画布尺寸变了 | `App::frame` 自己比较上一帧尺寸 |
| 首帧 | `App::frame` 初始化时就是脏的 |
| 影响布局的全局状态变了（如切换 Style） | 你自己调 `App::requestLayout()` |

```cpp
bklv::App::requestLayout();        // 请求下一帧重算
bklv::App::getLayoutCount();       // 累计布局次数（调试窗口状态行里也有）
```

**不要在改了布局相关属性之后、同一帧内立刻读 `getWidth()/getHeight()`** ——
新值要下一帧才生效。回收器不受这条影响：cell 的位置是用 `setTranslation` 摆的，
与 Yoga 无关。

界面不刷新时先怀疑"漏置脏"：用 `BKLV_FORCE_RELAYOUT=1` 跑一次，退回每帧全量重算，
现象消失就说明有地方改了布局状态却没 `invalidate()`（或该用 `requestLayout()`）。

## 主题与外观

换肤不需要改代码：复制 `resources/theme/default/` 改名，改里面的 JSON，再在
`themes.json` 里登记，就多了一套皮肤。桌面用 `BKLV_THEME=ocean ./dist/demo` 直接看效果。

```json
// resources/theme/ocean/widgets/button.json
{ "layers": [
    { "op": "rect", "id": "bg", "fill": "@theme/color.surface.raised",
      "radius": "@theme/metric/radius.md" },
    { "op": "text", "id": "title", "source": "text", "color": "@theme/color.text.primary",
      "fontSize": "@theme/metric/font.size.md", "align": "center", "valign": "center" } ],
  "states": {
    "focused": { "bg.fill": { "to": "@theme/color.accent.base", "duration": 120,
                              "easing": "quadraticOut" } } } }
```

```cpp
// 控件：外观全在 JSON 里，C++ 只写语义
bklv::Button* button = new bklv::Button("确定");
button->setOnClick([] { /* ... */ });

bklv::Rectangle* panel = new bklv::Rectangle();          // 纯装饰
bklv::Button* ghost = new bklv::Button("取消", "ghost");  // variant -> button.ghost.json

app.setTheme("ocean");   // 运行时换肤：重新取外观 + 重排，已建视图立即更新
```

要记住的四条：

1. **装饰走 JSON，语义留 C++**：底/边框/文字/图片/状态动效由 `layers` 描述；换行截断、
   图片缩放、进度弧这类走 `custom` 层，由控件自己画。
2. **外观值在 `applyTheme()` 时取定**，`draw()` 里不查主题（否则每帧几千次字符串查表）。
3. **主题只覆盖没被显式设置过的值**：`label->setFontSize(18)` 之后换肤不会改它；
   要让它跟随主题就 `label->setFontSizeExplicit(false)`。
4. **加控件外观要在 `theme.json` 的 `widgets` 数组里登记**（不扫目录，romfs 限制）。

写成 `@theme/color.x.y` / `@theme/metric.a.b` 的引用在**装载时**解析：
`colors.json` 的 key 拍平成 `color.*`，`metrics.json` 拍平成 `metric.*`。
皮肤缺的 key 回退 `default`。

## 不生成 imgui.ini

bklv 的绘制全部落在 viewport 的绘制列表上，不依赖 ImGui 窗口的位置和尺寸，
所以默认把 `io.IniFilename` 置空，ImGui 既不会读取也不会写出 `imgui.ini`。

```cpp
// 默认就是关闭的，无需任何调用
bklv::App app;

// 想恢复 ImGui 默认行为（读写 imgui.ini）时：
app.setIniPersistenceEnabled(true);
// 或者用环境变量：BKLV_USE_INI=1 ./dist/demo
```

实现位置：`bklv::App::run()` 里 `ImGui::CreateContext()` 之后、
第一次 `NewFrame()` 之前设置 `ImGui::GetIO().IniFilename = nullptr`。
这个赋值必须在那之前完成，之后改无效。

## 日志与调试窗口

日志用 `BKLV_LOG_INFO(...)` 等宏，输出带 `文件:行号`。日志同时进控制台与
`Logger` 的环形缓冲，界面上的调试窗口（`views/debug_window.hpp`）读缓冲渲染。

**不要为每条日志创建 View** —— borealis 那样做会不断触发 Yoga 重排。
`demo` 界面上右上角的「显示日志」按钮开关日志窗口。

## 已知的坑（都已处理）

1. **字体必须惰性解析。** 视图树是在 `RunDemo` 的参数里构建的，而字体在 `App::run()`
   里才加载；如果 `Label` 在构造时绑定字体，会绑到 `nullptr`，文本永远测不出宽度。
2. **字体加载后必须重排一次。** 树构建时字体还没就绪，文本会测出 0 宽并被 Yoga 缓存。
   `App::frame` 用 `getFontGeneration()` 检测字体变化并触发一次全树重算。
3. **`invalidate()` 不能用 `YGUndefined` 重算。** 在 `update()` 里新建视图（虚拟列表
   每帧都在建 cell）会触发 `invalidate`，如果传 `YGUndefined`，百分比宽高全部失去参照，
   整棵树会塌成内容尺寸。现在用 `App::getLayoutWidth/Height()` 记住的画布尺寸重算。
4. **滚动偏移不能重复叠加。** `getLocalX/Y()` 已含 `translation`，`getX/Y()` 只累加父节点，
   否则滚动距离会被算两次（之前 500 的偏移变成了 1000）。
5. **回收的 cell 可能正是当前焦点。** 被回收时必须清掉"已自动滚动"标记，否则焦点转回来时不会滚动。
6. **图片路径按进程工作目录解析**，且加载失败会被 `g_pathCache` 永久缓存
   （`TEXTURE_CACHE_FAILED`），只会打一条日志。所以要在项目根目录下运行；
   出现"图片位置空着"时先确认工作目录，别急着怀疑渲染后端。

## 分层约定

```
bklv/views/*               Image / Label / ScrollingFrame / RecyclerFrame
                            只认 ImDrawList 和 ImTextureID
        ↓
bklv/core/graphics.hpp     RenderBackend 接口 + 纹理管理
bklv/core/platform.hpp     Platform 接口（窗口 / 事件循环 / 系统能力）
        ↓
bklv/backends/*            OpenGLBackend / VulkanBackend（桌面）/ GLES3Backend（Switch）
bklv/platforms/*           PlatformSDL3（桌面）/ PlatformSwitch（libnx）
```

**后端选择是编译期的**，没有运行时开关——后端代码、链接库、shader 版本都不同。
选择点在 `bklv/platforms/*.cpp` 的 `createBackend()`，是几行 `#if`，
没有注册表、没有优先级列表。加后端的步骤见 [`notes/adding-render-backend.md`](../notes/adding-render-backend.md)。

## 学习提示

- **Yoga**：先配置父节点的 `FlexDirection`、`JustifyContent`、`AlignItems`、`Gap` 和
  `Padding`；再设置子节点尺寸约束或 `FlexGrow`；调用 `YGNodeCalculateLayout()` 后读取
  `YGNodeLayoutGetLeft/Top/Width/Height()`。Yoga 只算几何布局，不负责绘制。
- **Tweeny**：用 `tweeny::from(start).to(end).via(easing).during(frames).build()` 创建动画；
  每帧按经过时间调用 `step(frames)`，通过 `peek()` 读取插值。继续调用 `to()` 可添加关键帧，
  用 `on(tweeny::event::complete, callback)` 监听完成事件。
- **Dear ImGui**：负责接收输入、生成控件和绘制命令；窗口由平台层（SDL3 或 libnx）支持，
  绘制由图形后端（OpenGL / GLES3 / Vulkan）完成。
- **平台层**：上层只跟 `Platform` 与 `RenderBackend` 两个接口打交道。
  加平台写一个 `Platform` 子类，加图形 API 写一个 `RenderBackend` 子类，view 层不用动。
- **Switch 特有**：构建带 `-fno-rtti`，类型判断用 `View::asBox()` 虚函数而不是 `dynamic_cast`；
  Yoga 需要单独用 C++20 编译。

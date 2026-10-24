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
| `geom/geometry.hpp` | Point / Size / Rect 与颜色辅助 |
| `geom/transform.hpp` | 绘制期变换（平移 / 缩放 / 旋转 / 斜切） |
| `paint/gradient.hpp/.cpp` | 线性/径向渐变的顶点色网格（ImDrawList 网格，三后端零改动） |
| `paint/shadow.hpp/.cpp` | 柔化阴影（外投影 / 内阴影 / cutout 壳层网格） |
| `paint/focus_ring.hpp/.cpp` | 焦点框形状与 LUT 采样 |
| `anim/time.hpp/.cpp` | `Animatable` 补间与缓动（自带实现，不依赖 tweeny） |
| `anim/transition.hpp` | 视图级转场（`ViewTransition`：入场 / 出场，绘制期变换零重排） |
| `anim/transition_player.hpp/.cpp` | 外观 `transitions` 段的通道播放器 |
| `json/json_util.hpp/.cpp` | JSON 读取工具：取值 / 报错 / 诊断收集 |
| `json/json_vars.hpp/.cpp` | JSON 变量块：`@` 跨文件、`&` 同文件 |
| `json/json_policy.hpp/.cpp` | JSON 错误策略（`STRICT` / `SKIP_FIELD` / `FALLBACK_DEFAULT`） |
| `core/globals.hpp/.cpp` | 进程级状态登记（`getGlobalStore()`：对象槽 / 数值槽 / 重置钩子） |
| `core/resource.hpp/.cpp` | 资源路径解析：桌面用可执行文件目录，Switch 用 `romfs:/` |
| `core/file_system.hpp/.cpp` | 文件读取与目录操作（平台差异集中在这里） |
| `core/logger.hpp/.cpp` | 日志：级别过滤、时间戳、文件行号、环形缓冲（日志窗口读它） |
| `core/task.hpp/.cpp` | 线程池与主线程延迟任务（回调带生命周期护栏约定） |
| `core/event.hpp/.cpp` | 事件总线（RAII 订阅句柄，不用 RTTI） |
| `core/services.hpp/.cpp` | 进程级服务登记（纹理仓库等） |
| `core/alive.hpp` | `AliveFlag`：异步回调的生命周期护栏 |
| `core/cache.hpp` | 通用缓存容器 |
| `core/ui_scale.hpp/.cpp` | UI 缩放：主题尺寸按 1280x720 写，运行时按画布宽等比放大 |
| `core/frame_budget.hpp/.cpp` | 帧预算：把可推迟的工作摊到后续帧 |
| `core/frame_stats.hpp/.cpp` | 帧耗时统计（p50 / p99） |
| `core/decode_queue.hpp/.cpp` | 图片解码队列（离主线程解码、按帧上限回传） |
| `core/i18n.hpp/.cpp` | 文案系统：JSON 资源、三级回退、查找缓存、运行中切语言 |
| `core/platform.hpp/.cpp` | **平台接口**：窗口、事件循环、子系统工厂、系统能力 |
| `core/frame_stats` / `core/frame_budget` | 见上（帧统计与帧预算） |
| `theme/theme_data.hpp` + `theme/theme_parse.cpp` | JSON 主题：颜色表 / 度量表 / 外观清单 |
| `theme/appearance*.{hpp,cpp}` | 外观编译与运行：`box` 段、`layers`、`states`、`import`、缓存 |
| `theme/attributes.hpp/.cpp` | 属性值与属性表：类型检查、`@i18n/` `@style/`（主题数值）`@theme/`（主题颜色）`@res/` 前缀 |
| `theme/layer_registry.hpp/.cpp` | 图层类型表：能写哪些层、每层有哪些参数（`LayerType` / `LayerParam`） |
| `theme/appearance_layer_fields.*` / `appearance_layer_meta.*` | 层的通用字段表与 op 元信息（拼写、默认 id、对齐枚举） |
| `layer/layer_draw_*.cpp` | 六个内置层的绘制实现（rect / text / image / shadow / glass / custom） |
| `layer/layer_register.hpp` | 第三方层的注册模板（`registerLayer<T>()`） |
| `view/view.hpp/.cpp` | 视图基类：Yoga 样式透传、布局失效、绘制模板、测量回调 |
| `view/box.hpp/.cpp` | 容器：增删子节点时同步 Yoga 树、Flexbox 属性、递归绘制 |
| `view/json_view.hpp` + `view/view_parse.cpp` | JSON 视图描述：类型注册、模板缓存、实例化、include、错误 |
| `view/json_data.hpp/.cpp` / `view/json_actions.hpp/.cpp` | JSON 里的数据源与动作绑定 |
| `app/app.hpp/.cpp` | 帧循环：ImGui 上下文、布局、绘制调度。窗口由 `Platform` 负责 |
| `app/context.hpp/.cpp` | 每上下文状态（ImGui Context / 后端 / 平台 / 输入） |
| `media/font.hpp/.cpp` | 字体加载：桌面读系统字体文件；Switch 用 `pl` 服务取内置共享字体并合并 fallback 链 |
| `media/icon_font.hpp/.cpp` | 图标字体：私有区码位扫描与字体选择 |
| `media/image_decoder*.{hpp,cpp}` | 图片解码抽象：macOS 走 ImageIO，其余平台走 stb_image；GIF / WebP / SVG 各一份 |
| `media/image_writer.hpp/.cpp` | 截图 / 图片写出 |
| `media/audio.hpp/.cpp` | 音频设备与混音（音效，不做流式播放） |
| `media/sound_bank.hpp/.cpp` | 音效表：JSON `sound.json` + WAV 解码与缓存 |
| `input/input.hpp/.cpp` | 输入与焦点：hitTest、方向导航、动作注册、模态栈 |
| `input/gamepad.hpp/.cpp` | 手柄输入抽象（桌面 SDL3 / Switch libnx） |
| `input/gesture.hpp/.cpp` | 手势识别（点击 / 拖拽 / 长按） |
| `input/ime.hpp/.cpp` | 文本输入：平台接管（Switch swkbd）或控件自己读键盘 |
| `render/graphics.hpp/.cpp` | **渲染后端接口** + 纹理管理，上层与图形 API 的唯一分界 |
| `render/gl/opengl_backend.*` + `render/gl/gl_loader.*` | 桌面 OpenGL 后端与跨平台 GL 入口（Windows 运行时加载，复用 ImGui 的 imgl3w） |
| `render/vk/vulkan_backend.*` | Vulkan 后端（动态渲染优先，退到 render pass；交换链重建、纹理上传） |
| `render/gles3/gles3_backend.*` + `gles3_loader.hpp` | Switch 的 EGL + OpenGL ES 3 后端 |
| `render/libnx/imgui_impl_libnx.*` | ImGui 的 libnx 平台后端（把按键/触摸喂进 ImGuiIO） |
| `render/glass/` + `render/glass/gl|vk/` | 玻璃（背景模糊）子系统：服务 / 队列 / 模糊计划 + 两套后端实现 |
| `platform/sdl3_*` | macOS/Linux/Windows 的 SDL3 平台实现（窗口 / 手柄） |
| `platform/switch_*` | Switch 的 libnx 平台实现（EGL/GLES3 + pad + 电源/网络/语言 + swkbd） |
| `widgets/appearance/` | 外观驱动控件基类：解释 box / layers、状态动效、固有尺寸测量；`widget_types.cpp` 注册 `Button` |
| `widgets/controls/` | `Button`（唯一的 JSON 视图类型）与 `Notification`（`App::notify()` 的落点） |
| `widgets/basic/` | `Label` / `Image` / `Padding` / `ProgressSpinner` —— 语义控件，只认 ImDrawList 与 core 接口 |
| `widgets/container/` | `ScrollingFrame` / `HScrollingFrame` / `ScrollBox` |
| `widgets/list/` | `RecyclerFrame` 与数据源 / JSON 列表项桥接 |
| `widgets/debug/` | 日志层 `DebugLogView`、FPS 覆盖层、`PresetBackdrop` |
| `widgets/view_types.cpp` | 内置视图类型的注册入口（`registerBuiltinWidgetViewTypes()`） |

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
// 输入与焦点：View 自己注册动作（宿主可读它们自己渲染按键提示）
button->registerAction(ControllerButton::A, "播放", [](View* v) { /* ... */ });
bool focusable = button->isFocusable();          // 想被方向键选中就重写它

// 纵向滚动
ScrollingFrame* frame = new ScrollingFrame();
frame->setHeight(300.0f);
Box* content = new Box(Axis::COLUMN);
content->setHeight(40.0f * 100);               // 内容必须显式给高度
frame->setContentView(content);

// 虚拟列表：1000 条数据只创建约 8 个 cell（MyCell 派生自 RecyclerCell）
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
`onPointerAction = true`（`Button` 就是这么做的）。列表项保持默认，避免误触发。

## 文案（i18n）

资源放 `resources/i18n/<locale>.json`，key 用 `/` 分层：

```json
{ "demo": { "welcome": "欢迎使用 bklv", "list_count": "共 {} 项" } }
```

```cpp
bklv::translate("demo/welcome");              // "欢迎使用 bklv"
bklv::translate("demo/list_count", 12);       // "共 12 项"（{} 按顺序替换）
bklv::translate("demo/nope");                 // 未命中返回 key 本身
bklv::translate("demo/list_count", 12);       // 带占位符必须用函数形式
"demo/welcome"_i18n;                          // 无参数 key 的字面量写法（需 using namespace bklv::i18n）
"demo/list_count"_i18n(12);                   // 带参数：占位符 {} 按顺序替换
"demo/welcome"_i18n.str();                    // 显式取字符串（auto 存下来就是 key 包装，不是 string）
bklv::setLocale("en-US");                  // 运行中切换
bklv::getLocale();                         // 规范化后的当前语言
```

三条要记住的：

1. **文案在控件构造时就解析成字符串**，改语言不会自动重译已有视图——
   要重新 `setText()` 或重建视图树（宿主里那个
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

> 四类 JSON（视图描述 / 控件外观 / 主题 / 文案）的完整键表、取值形式与枚举拼写，
> 见 [`json-syntax.md`](json-syntax.md)。

## 主题与外观

换肤不需要改代码：复制 `resources/theme/default/` 改名，改里面的 JSON，再在
`themes.json` 里登记，就多了一套皮肤。桌面用 `BKLV_THEME=glass ./dist/embed_imgui_gl` 直接看效果。

```json
// resources/theme/glass/base/button.json
// box 段管盒模型（尺寸/内边距/圆角/底色…），layers 管绘制结构，states 管状态动效
{ "box": { "height": "@theme/metric.control.height", "padding": [15, 25, 15, 25] },
  "layers": [
    { "op": "rect", "id": "bg", "fill": "@theme/color.surface.raised",
      "radius": "@theme/metric/radius.md" },
    { "op": "text", "id": "title", "data_id": "text", "color": "@theme/color.text.primary",
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

app.setTheme("glass");  // 运行时换肤：重新取外观 + 重排，已建视图立即更新
```

### 常用色（`color.common.*`）

每套主题的 `colors.json` 第一组是**通用色板 + 原始色值**，语义组（`surface` / `text` / `button` …）跟在后面：

| 组 | 键 | 说明 |
|---|---|---|
| 基础色板 | `red` `orange` `yellow` `green` `cyan` `blue` `purple` `pink` `brown` `grey` | 每个还有 `XxxLight` / `XxxDark` 两档（Material 三档） |
| 原色 | `black` `white` `transparent` | 纯色与全透明 |
| 常用半透明 | `overlay`（模态遮罩）`divider`（1px 分割线）`shadow`（阴影基准） | |
| 其它 | `focusRing` `textMuted` | |

```json
"common": { "red": "#f44336", "redLight": "#e57373", "redDark": "#d32f2f",
            "blue": "#2196f3", "…": "…",
            "transparent": "#00000000", "white": "#ffffff", "black": "#000000",
            "overlay": "#00000080", "divider": "#ffffff1f",
            "shadow": "#00000073", "focusRing": "#00ffcc", "textMuted": "#8b929b" }
```

外观里引它：`"fill": "@theme/color.common.transparent"`（60 处重复的透明字面量已统一成这个）。
`colors.json` 内部**不支持互相引用** —— 语义键还是字面量，换配色时一起改。

### 阴影（`"op": "shadow"`）

阴影是**一种层**，写在自己的底色之前就先画阴影（`theme_test` 会断言这个顺序）：

```json
"layers": [
  { "op": "shadow", "id": "shadow", "fill": "@theme/color.shadow.base",
    "feather": "@theme/metric.shadow.feather",   // 羽化半径
    "radius": "@theme/metric.shadow.radius",     // 圆角（会跟着外扩一起长）
    "offset": [0, "@theme/metric.shadow.offsetY"], // 位移：正 y = 往下方投影
    "spread": 0 },                               // 额外外扩（负数可收紧）
  { "op": "rect", "id": "bg", "fill": "@theme/color.surface.card", "radius": 12 }
]
```

- **`"cutout": true`**（可选）：把控件自身圆角形状挖掉，只画外圈（沿中线描一圈实现）。不开时阴影是垫在控件下面
  的一整块——底色若是半透明就会透出阴影；开了之后内圈不画，后面加的层哪怕是透明色也干净
  （borealis 的 `nvgBoxGradient` + `NVG_HOLE` 就是永远这么做）
- `fill` 的 alpha 就是阴影浓度；`opacity`、`feather`、`spread`、`offsetX/offsetY` 都能进
  `states` 做动效（例如聚焦时 `"shadow.opacity": { "to": 0.7 }`）
- 绘制档位是**全局开关**，两种都是多层近似：`fast`（8 层，默认）/ `smooth`（24 层高斯权重）
  ```cpp
  bklv::shadow::setQuality(bklv::ShadowQuality::SMOOTH);   // 或 setQualityByName("smooth")
  ```
  ```bash
  BKLV_SHADOW_QUALITY=smooth ./dist/embed_imgui_gl    # 启动前用环境变量指定
  ```
- 控件自己在 `custom` 层画的东西（滑块把手）不走外观层，直接调
  `bklv::shadow::drawSoft(...)`
- 真离屏高斯模糊没做：那要给三个后端各加一套离屏渲染 + 模糊 shader，
  而 UI 阴影的羽化只有 10px 上下。档位开关留在这里，以后换实现不影响 JSON 与调用方

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
// 或者用环境变量：BKLV_USE_INI=1 ./dist/embed_imgui_gl
```

实现位置：`bklv::App::run()` 里 `ImGui::CreateContext()` 之后、
第一次 `NewFrame()` 之前设置 `ImGui::GetIO().IniFilename = nullptr`。
这个赋值必须在那之前完成，之后改无效。

## 日志与调试窗口

日志用 `BKLV_LOG_INFO(...)` 等宏，输出带 `文件:行号`。日志同时进控制台与
`Logger` 的环形缓冲，界面上的调试日志视图（`widgets/debug/debug_log_view.hpp` 的 `DebugLogView`）读缓冲渲染；
它挂在宿主指定的调试层上（`Shell` 已移除，层级由宿主自己管），不吃输入。

**不要为每条日志创建 View** —— borealis 那样做会不断触发 Yoga 重排。
`demo` 界面上右上角的「显示日志」按钮（或 F1）开关日志窗口。

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

## 近期新增（2026-10）

### 滚动盒：`ScrollBox`

不用手工建 `ScrollingFrame` + `setContentView`：

```cpp
ScrollBox* list = new ScrollBox(Axis::COLUMN);   // 纵向；Axis::ROW = 横向
list->setHeight(240);                            // 视口高
list->addView(item);                             // 直接加，自动进内容盒
list->getContentBox()->setGap(8);                // 间距写在内容盒上
list->getScrollRange();                          // 透传：范围/偏移/滚动条
list->setScrollOffset(120, true);
```

### 段落标题工厂

```cpp
Box* section = bklv::makeSectionHeader("图标", "共 340 个字形");
// 等价于 new AppearanceView("base/section") + 写 text/info 槽
```

### 顶层帧率

```cpp
root->addView(new FpsOverlay());       // 右上角：60.0 FPS / 目标 60 + p99 / 最坏帧（Shell 已移除，宿主自己挂）
```

### 外观 children 的 id 与查找

```json
"children": [
  { "id": "icon",  "box": { "width": 24, "height": 24 }, "icon": "@icon/star" },
  { "id": "label", "box": { "grow": 1 }, "text": "确认" }
]
```

```cpp
view->findViewById("icon")->setVisibility(Visibility::GONE); // 不占位，文字自动重排
```

### 状态驱动整块视图（`states.<状态>.self`）

```json
"states": {
  "focused": {
    "self": { "translateX": { "to": 100, "duration": 160, "easing": "cubicOut" } },
    "bg":   { "fill": { "to": "@theme/color.surface.hover", "duration": 120 } }
  }
}
```

- `self` 支持：`translateX/Y`、`scaleX/Y`、`alpha`、`rotate`、`skewX/Y`；
- 层目标支持分组写法（`"bg": { "fill": … }`），旧的 `"bg.fill"` 兼容；
- 位移/缩放/旋转都是**绘制期**变换：不重排、不占位，命中区域不跟随移动；
- 多状态同时激活时，后激活的赢；状态关闭时平滑回默认值（120ms）。

### 预设背景

```cpp
root->addView(new PresetBackdrop(BackdropPreset::GRID));   // 放在内容之下（Shell 已移除）
// GRID / FLOATING / GRADIENT；颜色全部来自主题（surface.card / accent.base / common.grey）
```

## 分层约定

```
bklv/widgets/*             Button / Label / Image / RecyclerFrame / 调试层
                            只认 ImDrawList 与 view/ core/ 的接口
        ↓
bklv/view/*                View / Box（Yoga 布局）/ ViewTemplate
bklv/theme/* + layer/*     外观数据模型 + 图层绘制
        ↓
bklv/render/graphics.hpp   RenderBackend 接口 + 纹理管理
bklv/core/platform.hpp     Platform 接口（窗口 / 事件循环 / 系统能力）
        ↓
bklv/render/gl|vk|gles3|libnx   具体后端实现（核心玻璃在 render/glass/）
bklv/platform/*            PlatformSDL3（桌面）/ PlatformSwitch（libnx）
```

**后端选择是编译期的**，没有运行时开关——后端代码、链接库、shader 版本都不同。
选择点在 `bklv/platform/*.cpp` 的 `createBackend()`，是几行 `#if`，
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

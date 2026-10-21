# JSON 审查报告（语法完备性 / 代码解析 / 渲染）

> 时间：2026-10　范围：`bklv/compile/*`（解析）、`bklv/core/{theme_data,appearance,view,attributes}.*`、
> `bklv/widgets/*`（绘制）、`resources/**`（数据）。设计器已移除，本文最后附带"重做设计器前
> 需要补的接口"，但不以设计器为前提。
>
> 规模参考：`resources/theme/default/` 共 **99** 个 JSON（113 色 / 164 数值 / 90 份外观），
> 注册的视图类型 **27** 种。

---

## 0. 结论摘要

1. **语法是闭合的**：五类 JSON 各有唯一写法，取值域、枚举拼写、报错口径都有单一实现点
   （`compile/json_util.*`、`compile/appearance_layer_fields.cpp`、`core/attributes.cpp`），
   没有"两种写法等价但只认一种"的暗坑（`@theme/color.a.b` 写斜杠会被容错成点分并警告）。
2. **有三处天然"开放"，决定了任何静态工具都不可能穷举**：`op: EXTENSION`（第三方在运行期注册）、
   `op: custom`（像素由 C++ 画）、`import`/`include`（一份文件 ≠ 一个完整对象）。
3. **一个真实的不完备**：视图描述的**未知属性只警告不报错**（外观侧则报错）。所以"合法 JSON"在
   视图侧不可判定——拼错的属性名能存能跑，只是被静默忽略。

---

## 1. 语法完备性

### 1.1 五类文件

| # | 文件 | 位置 | 作用 | 校验强度 |
|---|---|---|---|---|
| 1 | **视图描述** | `<主题>/view/*.json`、`resources/views/*.json` | 节点树（类型 + 属性 + children + include） | 结构错误 → 不建节点 + 报错；**未知属性 → 警告** |
| 2 | **外观** | `<主题>/base/*.json`、`<主题>/base/widget/*.json` | `vars` / `import` / `box` / `layers` / `states` / `transitions` | 基本都报错（缺 `layers`、key 不存在、枚举拼错、vars 写错） |
| 3 | **主题令牌** | `<主题>/tokens/colors.json`、`metrics.json` | `a.b` → 颜色 / 数值 | 语法错误报错；**key 名自由**（语义由引用方决定） |
| 4 | **主题元信息** | `<主题>/theme.json`、`themes.json` | `name`/`author`/`description`/`files[]` | `files[]` 条目必须带 `.json`（少了会静默回退） |
| 5 | **文案** | `resources/i18n/<locale>.json` | `key` → 字符串、`{}` 占位、`$files` 清单 | 语法错误报错；`$` 开头的键跳过 |

### 1.2 引用与变量（值语法）

| 前缀 | 作用域 | 例子 | 解析点 |
|---|---|---|---|
| `@theme/` | 跨文件（主题色/数值） | `@theme/color.accent.base`、`@theme/metric.control.height` | `compile/json_util.cpp`、`core/attributes.cpp` |
| `@style/` | 跨文件（视图侧的数值） | `@style/metric.radius.md` | `core/attributes.cpp` |
| `@i18n/` | 跨文件（文案） | `@i18n/page/shadow/title` | 实例化期（模板缓存里保留原文） |
| `@res/` | 跨文件（路径，相对 `resources/`） | `@res/img/a.png` | 使用处自己解析 |
| `&color.` / `&number.` | **同文件** `vars` | `&color.none`、`&number.radius` | `compile/json_vars.cpp`（合并/实例化前展开） |
| `$...` | **只用于键**（元数据） | `$comment`、`$files` | 不参与取值；i18n 装载器跳过 `$` 开头的键 |

长度 / 数值写法：`12`、`"12"`、`"auto"`、`"50%"`、`@theme/metric.a.b`；`padding`/`margin` 接受
**一个数**或**四个数**（`[上,右,下,左]`）。颜色只认 `#RRGGBB` / `#RRGGBBAA` / `@theme/color.x.y`。

### 1.3 闭合 / 半开 / 开放

| 元素 | 性质 | 说明 |
|---|---|---|
| `box` 字段、`layers` 的 `op`（6 种内置）、`states` 的通道名、`transitions` 的四个事件 | **闭合** | 枚举拼错有提示，未知键有警告/报错 |
| 视图属性 | **半开** | 27 种类型各自的 `attributes()` 静态表 + 公共属性；**未知属性只警告** |
| `op: EXTENSION` | **开放** | 第三方在运行期 `registerLayer<T>()` 注册类型 + 参数模式（`compile/layer_register.hpp`） |
| `op: custom` | **不可枚举** | JSON 只声明"这层由控件自己画"（`AppearanceView::drawCustomLayer`） |
| `import` / `include` | **组合** | 一份文件的有效内容 = 合并/包含结果，文件本身不是完整对象 |
| `vars` | **间接** | 值是引用还是字面量，取决于同文件 `vars` |
| `gradient`（rect 内嵌） | **闭合** | `type/angle/center/stops[2..4]`，五类写错都报错 |

### 1.4 已知的不完备（按影响排序）

| # | 问题 | 后果 | 位置 |
|---|---|---|---|
| 1 | 视图未知属性只警告 | 拼错的属性名能存能跑、静默忽略；"合法 JSON"不可判定 | `core/attributes.cpp`（既有约定） |
| 2 | 外观对 `layers` 以外的顶层键不敏感 | `$comment` 之类可随便放；但拼错的顶层键（如 `state`）不会报错 | `Appearance::compile` 只查 `layers` 必需 |
| 3 | `theme.json` 的 `files[]` 少了 `.json` | 该外观静默回退到空外观（有警告）；theme_test 才兜住 | `compile/theme_parse.cpp` |
| 4 | `colors.json` 不支持 key 间引用 | 语义色只能写字面量；改整体配色要一起改 | 文档约定 |
| 5 | 数值多形态 | `12` 与 `"12"` 都合法；做"原样回写"的工具必须保住原文，不能统一成 float | 解析器宽容 |

---

## 2. 代码解析

### 2.1 主题装载链（`compile/theme_parse.cpp`）

```
themes.json            → 主题清单（名字/目录）
<主题>/theme.json      → 元信息 + files[]（99 条）
  tokens/colors.json   → 展平成 color.a.b   → ThemeData::color(key, fallback)
  tokens/metrics.json  → 展平成 metric.a.b  → ThemeData::number(key, fallback)   ← 这里乘 uiScale
  base/*.json   → **原文缓存**（widgetAppearanceJson），编译在用到时做
```

- 键展平后**不存在就报错**（不静默回退洋红）；`metric.*` 在 `number()` 里乘 `uiScale`，
  但 `metric.anim.*`（毫秒）与 `metric.font.lineHeight`（系数）不乘。
- 缺项回退链：当前主题 → `default` → 空外观（警告）。

### 2.2 外观编译链（`Appearance::compile`，`core/appearance.cpp:825`）

```
① Json::parse（语法错误带行号）
② expandJsonVars（本文件的 vars / & 引用；失败 → "外观 JSON 的变量块有问题"）
③ resolveImports（不是 resolve 合并链：逐字段 merge；被导入文件**先按它自己的 vars 展开**）
   合并规则：对象递归、layers 按 id 合并、其它数组整体替换、标量覆盖、当前文件优先
   限制：深度 8、循环报错、父级不存在报错
④ 必需 `layers` 数组（缺了直接失败）
⑤ 逐层解析（compile/appearance_layer_fields.cpp）：
     args 先按注册表的参数模式解析（NUMBER / PIXELS / COLOR / BOOL / ENUMERATION）
     再按 op 的字段表解析（fill/stroke/radius/inset/offset/gradient/...）
⑥ box 解析（appearance_box.cpp）：只覆盖 JSON 里出现的字段（没写的保留旧值）
⑦ states 解析：通道 = <层id>.<属性名>，值 = { to, duration, easing? }
⑧ transitions 解析：enter/exit/show/hide × 视图级通道 + 层内通道（when: with|after）
⑨ sortLayers（layerSortRank）：SHADOW(0) → GLASS(1) → 内置(2, 书写顺序) → DECORATION(3) → OVERLAY(4)
     ⚠️ shadow 层 "inner": true 时按内置层处理（画内缘，必须压在底色上）
⑩ 返回 shared_ptr<const Appearance>
```

缓存：`getWidgetAppearance()` 按 **`主题名/外观名`** 缓存编译结果（`appearanceCache()`），
换主题或热重载时 `clear()`。所以同一份外观只编译一次。

### 2.3 视图解析链（`compile/view_parse.cpp`）

**两段式**（关键设计，决定 `@i18n` 的行为）：

```
① ViewTemplate::load(path)  → JSON::parse → expandJsonVars（模板缓存前）→ 缓存（与语言无关）
② tmpl->instantiate(bindings?) → 递归建 View 树：
     · 按 type 从注册表取工厂（27 种；RecyclerFrame 故意不注册，要数据源）
     · 属性 → 各类型 attributes() 表里的 handler（公共属性在 core/view.cpp）
     · 字符串值里的 @i18n / @theme / @style 在**这一步**解析（切语言不会烤进缓存）
     · children 递归；include 整棵接入（栈深 32、循环报错）
```

限制：深度 32 / 节点 4096 / include 栈 32 / 模板缓存按路径。结构错误返回 `nullptr` + 日志。

### 2.4 缩放与单位（唯一的三个乘法点）

| 位置 | 乘 `uiScale` | 不乘 |
|---|---|---|
| `ThemeData::number("metric.*")` | ✅（`metric.anim.*`、`metric.font.lineHeight` 除外） | — |
| 外观 JSON 的 radius / strokeWidth / width / height / fontSize / inset / offset / feather / spread | ✅ | `opacity`（系数）、`duration`（毫秒） |
| 代码里写死的像素 | ❌ | — |

画布尺寸变化 → `App::frame` 调 `updateUiScaleFromCanvas()` → 重新套用外观（外观值在 `applyTheme()`
时就取定了）+ 重排。

### 2.5 错误与限制清单

| 项 | 值 | 位置 |
|---|---|---|
| 视图结构错误 | 返回 `nullptr` + 日志（记录到 `lastError`） | `view_parse.cpp` |
| 视图未知属性 | **警告**（带拼错提示） | `attributes.cpp` |
| 外观缺 `layers` | 报错 | `appearance.cpp` |
| 外观 key 不存在 | 报错 | `json_util.cpp` |
| 外观枚举拼错 | 警告 + 保持原值 | `appearance_box.cpp` 等 |
| `import` 深度 / 循环 | 8 / 报错 | `appearance_imports.cpp` |
| `vars` 链深 / 循环 | 4 / 报错 | `json_vars.cpp` |
| 视图深度 / 节点 / include 栈 | 32 / 4096 / 32 | `view_parse.cpp` |
| 图片尺寸上限 | 解码前按头部卡（`decodeImageFile`） | `image_decoder.cpp` |

---

## 3. 渲染

### 3.1 一帧的顺序（`core/app.cpp`）

| # | 步骤 | 关键点 |
|---|---|---|
| 0 | 字体（窗口创建后才可用） | 视图树通常更早建好 → 首次构建拿不到字形 |
| 1 | **布局（只在脏时算，一帧最多一次）** | `invalidate()` 只置脏；`YGNodeCalculateLayout` |
| 1b | `updateUiScaleFromCanvas()` | 画布变宽 → 重新取外观 + 重排 |
| 2 | 动画与视图逐帧逻辑（滚动、动图、状态补间） | `AppearanceInstance` 的 tweeny 更新 |
| 3 | 输入与焦点 | 用上一帧命中结果 |
| 4 | 业务帧回调（`onUpdate`） | 可能新建/改动视图树 |
| **4b** | **补一次布局** | 上面任何一步改了树，这里补算（切页时文字不再堆顶） |
| 5 | 绘制视图树 | `root->frame(drawList)` → 递归 |
| 6 | 玻璃 `beginFrame` / 采集 | GL：首个 glass 命令时抓帧一次 |
| 7 | ImGui 窗口（宿主自己的面板） | 视图树之后、`ImGui::Render()` 之前 |

### 3.2 控件 → 层

```
AppearanceView::frame → draw(drawList, x, y, w, h)
  ├─ 取当前 AppearanceInstance（applyTheme 时从 Appearance 拷一份，状态补间作用在它身上）
  ├─ 对 current.getLayers() 逐层：
  │    compile 期的 layerSortRank 已排好序 → 这里只按顺序画
  │    op → 注册表里的 LayerDrawContext 分派 → widgets/layer_draw_*.cpp
  │      rect   → fill / stroke / gradient（顶点色网格）
  │      text   → AddText，字体来自 getLayerFont()（图标层用图标字体）
  │      image  → 纹理 + scaling
  │      shadow → 外投影（drawSoft，可 cutout）/ 内阴影（drawInnerSoft，inner:true）
  │      glass  → GlassService（GL：半分辨率模糊 + 折射；VK：mip 链，无折射）
  │      custom → 控件自己的 drawCustomLayer（进度弧、滑块轨道、开关图案…）
  │      EXTENSION → 第三方注册的 draw（如 stripe / quad）
  └─ 子节点由容器控件自己递归（AppearanceView 是容器：先 Box::update 再画）
```

层默认铺满"控件范围按 inset 收缩后"的区域；写了 `width`/`height` 就按 `align`/`valign` 摆
（装饰件：色条 4px、下边线 1px）。

### 3.3 状态动效

`states.<状态>.<层id>.<属性名> = { to, duration, easing }`。运行时是
`AppearanceInstance` 上按通道的补间（tweeny）；**取消要先撤旧补间**再建新的
（否则"刚开又关"会把旧补间跑到新目标上）。可补间属性在 `ALL_PROPS`（颜色/数值；
**渐变不补间**、`inset`/`offset` 作为整体通道）。

### 3.4 文本与图片（语义绘制在 C++）

- **Label**：宽度由测量函数算、可用宽度由 Yoga 算，比较留 **0.5px 容差**（否则会把够短的中文
  截成一个省略号、或把两字标题拆两行）。换行/截断/对齐都在 `views/label.cpp`。
- **Image**：缩放模式（fit/fill/stretch）、九宫？不，按 `scaling`；动图在 `update()` 里按帧时钟
  推进、**就地 `updateTexture()`**（不重建纹理、不参与布局）；离屏不推进。
- **图标**：两套字体（Material = 控件图标，Switch = 按键/系统图标），层可声明用哪套。

### 3.5 与 JSON 的对应表

| JSON | 编译产物 | 绘制位置 |
|---|---|---|
| `box.*` | `AppearanceBox`（数值已乘缩放） | `applyAppearanceBox` 写进 Yoga 节点 |
| `layers[i].fill/stroke/radius/inset/offset/opacity` | `AppearanceLayer` 的定长字段 | `layer_draw_*` |
| `layers[i].gradient` | `AppearanceLayer::Gradient`（不做补间） | `core/gradient.cpp` 顶点网格 |
| `layers[i].inner/cutout` | 布尔 | `layer_draw_shadow.cpp` 选 drawSoft / drawInnerSoft |
| `states.*` | 通道表（目标值 + 时长 + 曲线） | `AppearanceInstance` 补间 → 绘制期读值 |
| `transitions.*` | 视图级通道 + 层内通道（delay 已折算） | `TransitionPlayer`（绘制期变换 = 零重排） |
| `vars` / `&` / `@theme` | **解析期就消失**，剩下的是最终值 | 无（运行时看不到引用） |
| `import` | 合并后的 `Appearance` | 无（运行时看不到来源） |

---

## 4. 重做设计器前的接口缺口

设计器已移除，但若要让"能完整显示所有合法 JSON"成立，模型层需要这三样（都属于**库**，不是 UI）：

1. **源文档保留 + 有效值并列**：`import` / `include` / `vars` 让"文件"≠"对象"。需要
   `Appearance::compile` 之外的"结构模型"（保留键序、未知键、原始写法如 `"50%"`/`&color.x`），
   以及"合并后有效值 + 每个字段的来源文件"。
2. **引用可解**：`&color.x` / `@theme/...` 要能给出"它指向哪里 + 解析后的值"，同时**不丢引用**。
   目前 `json_vars` 是就地展开、展开即消失。
3. **视图侧的严格模式**：未知属性现在只警告 → 工具无法判定"合法"。需要一个
   `strictAttributes` 开关（报错而不是警告）。

---

## 5. 建议（按优先级）

| # | 建议 | 收益 | 成本 |
|---|---|---|---|
| 1 | 视图侧加 **strict 模式**（未知属性报错，默认关） | 让"合法 JSON"可判定；拼写错误立刻暴露 | 小（一个开关 + 日志级别） |
| 1b | ~~错误恢复策略~~ **已实现**：`JsonErrorPolicy`（STRICT / SKIP_FIELD / FALLBACK_DEFAULT）+ 诊断表；见 `docs/json-syntax.md`「错误策略与诊断」 | 容错但不静默：恢复过的错误都留诊断 | 已完成 |
| 2 | `theme.json` 的 `files[]` 少后缀从警告升为**报错**（或至少汇总一条"本主题有 N 个文件没装载"） | 避免"外观静默变空" | 小 |
| 3 | 外观顶层键白名单（`vars/import/box/layers/states/transitions/$comment`），其余报错 | 拼错 `state`/`layer` 能被发现 | 小 |
| 4 | 把"结构模型"抽成库（保留键序/原文/来源），本轮不动 UI | 重做设计器的基础，也方便写校验工具 | 中 |
| 5 | `docs/json-syntax.md` 补两处：`$` 键=元数据、`EXTENSION` 的参数表从哪查 | 减少误用 | 很小 |

前三条都是"让错误更早暴露"，与现有"不静默降级"的取向一致；第四条是为下一步（重做设计器）
准备的地基。

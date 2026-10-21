# JSON 语法速查

bklv 里一共有四类 JSON，各管一件事。这份文档是它们的**完整语法**（键名、取值形式、
枚举拼写、默认值、报错行为），全部对照代码整理，不是示意。

| 类别 | 位置 | 用途 | 解析处 |
|---|---|---|---|
| 视图描述 | `resources/views/*.json`，或 `createFromJSONString()` | 用 JSON 搭视图树（类型 + 属性 + 子节点） | `core/json_view.cpp` `core/attributes.cpp` |
| 控件外观 | `resources/theme/<主题>/base|widget/<控件>[.<变体>].json` | 一个控件的盒模型 / 绘制层 / 状态动效 | `core/appearance.cpp` |
| 页面视图 | `resources/theme/<主题>/view/<名字>.json` | 页面/列表项的结构与外观（`createViewFromTheme`） | `core/json_view.cpp` |
| 主题 | `resources/theme/themes.json` + `<主题>/{theme,colors,metrics}.json` | 主题清单、颜色表、数值表 | `core/theme_data.cpp` |
| 文案 | `resources/i18n/<locale>.json` | i18n 文本 | `core/i18n.cpp` |

---

## 一、通用取值语法

### 长度 / 数值

| 写法 | 含义 |
|---|---|
| `12` 或 `"12"` | 像素，按 **1280x720 设计稿**写，运行时乘 `uiScale`（见 `core/ui_scale.*`） |
| `"auto"` | 由内容决定（只有 `width` / `height` 支持） |
| `"50%"` | 相对父容器的百分比（`width` / `height` 与绝对定位的四边偏移支持） |
| `"@theme/metric.a.b"` | 引用主题数值。**取值时已经乘过缩放**，不要再自己乘 |

### 颜色

**只支持两种写法**（其它字符串直接报错）：

```json
"#RRGGBB"        // 不透明
"#RRGGBBAA"      // 带 alpha，00 = 全透明
"@theme/color.a.b"
```

### 引用前缀（视图描述 JSON 的字符串属性）

| 前缀 | 含义 | 解析时机 |
|---|---|---|
| `@i18n/key` | 文案，取 `resources/i18n/<locale>.json` 的 `key` | **实例化期**（模板缓存里保留原文） |
| `@theme/color.a.b` | 主题颜色 | 实例化期 |
| `@style/metric.a.b` | 主题数值 | 实例化期 |
| `@res/x` | 文件路径，**相对 `resources/` 目录** | 使用处自己解析 |
| `@icon/star` | **内置图标字形**（按名字，名字表见 `core/icon_font.cpp`） | 实例化期（TEXT 解析里换成 UTF-8 字形） |
| `@icon/U+E000` | 同上，**按码位**（名字表是子集，码位能取到字体里的全部字形） | 同上 |

> **长度/坐标/偏移都按 1280x720 写**，和主题 metric、外观 JSON 用同一个基准：
> 解析时统一乘 `uiScale`（1080p 窗口里 100 -> 150），换分辨率不会错位。
> **不乘缩放**的只有：百分比（`"50%"`）、`auto`、系数类（`grow`/`shrink`/`alpha`/图片
> 的 `scale`）、以及毫秒（动画时长）。写 `@style/metric.x` 时值已经乘过，不会重复乘。

### 引用前缀：`@` 跨文件、`&` 同文件

| 前缀 | 作用域 | 例子 |
|---|---|---|
| `@` | **别的文件**（主题 / 文案 / 资源 / 内置图标） | `@theme/color.accent.base`、`@theme/metric.control.height`、`@i18n/xxx`、`@res/xxx`、`@icon/star` |
| `&` | **同一份文件**的 `vars` 块 | `&color.accent`、`&number.row` |

`$` 前缀只出现在**键**上，表示"这不是数据"（`$comment` 写注释、`$files` 是 i18n 的文件清单）；
出现在**值**位置上会被报错并提示改用 `&`。

### 变量块（`vars`）

一份文件在**顶层**写一个 `vars`，把本文档反复用到的颜色/数值起个名字，下面用 `&` 引用：

```json
{
  "vars": {
    "colors":  { "clear": "@theme/color.common.transparent",
                 "ring":  "#FF8800CC" },
    "numbers": { "radius": "@theme/metric.component.radius",
                 "row":    44,
                 "inset":  -2.5 }
  },
  "layers": [
    { "op": "rect", "id": "bg",   "fill": "&color.clear", "radius": "&number.radius" },
    { "op": "rect", "id": "line", "fill": "&color.ring",  "radius": "&number.radius",
      "inset": ["&number.inset", "&number.inset", "&number.inset", "&number.inset"] }
  ]
}
```

视图描述 JSON 里也一样，**`children` 里可以直接用**（`&` 绑的是文件级作用域，不需要在子节点里再写一遍 `vars`）：

```json
{
  "vars": {
    "colors":  { "accent": "@theme/color.accent.base" },
    "numbers": { "row": 44, "gap": 8 }
  },
  "type": "Box", "axis": "column", "gap": "&number.gap",
  "children": [
    { "type": "Box", "height": "&number.row", "backgroundColor": "&color.accent" },
    { "type": "Box", "height": "&number.row", "backgroundColor": "&color.accent" }
  ]
}
```

五条规则（**写错就报错，不静默降级**）：

1. **一个文件只有一个 `vars`，必须在顶层**；嵌套出现 → 报错
2. 只有 `colors` / `numbers` 两个子块；别的子块 → 报错
3. 值可以是字面量（`"#RRGGBB[AA]"` / `12`），也可以是 `@theme/...` 引用
4. `vars` 之间可以用 `$` 互相引用（链式，最多 4 层）；**循环 → 报错**
5. `&` 的展开与"直接写那个值"**完全等价** —— 缩放（`uiScale`）、`opacity`/`duration` 不缩放、
   报错口径都由 `resolveColor` / `resolveNumber` 决定，`vars` 这一层不加工数值

`vars` 怎么和已有机制配合：

| 机制 | 解决什么 |
|---|---|
| `@theme/...` | 同一份**跨文件**的全局 token（主题） |
| `vars` + `$` | 同一份**文件内**的重复值（一眼看出这句话在说什么） |
| `import` | 整份外观的复用（父外观的字段被当前文件覆盖） |

`&` 绑的是"写它的那份文件"：A import B 时，B 里的 `$` 按 B 自己的 `vars` 展开，再合并。
想让 A 调 B 的值，直接在 A 里覆盖那个字段（`import` 的"当前文件优先"）。

**明确不做**：算式（`"&number.gap * 2"`、`"100% - &number.x"`）、结构复用（整组 `inset` / `states` /
`layers`，那是 `import` 的活）、字符串/枚举变量、跨文件 `&`、子节点内的 `vars`。

### 缩放规则（记住这一条就不会算错）

| 会乘 `uiScale` | 不乘 |
|---|---|
| `metric.*`（主题数值）、外观里的 `radius` / `strokeWidth` / `width` / `height` / `fontSize` / `inset` / `offset` / `feather` / `spread` | `opacity`（系数）、`duration`（毫秒）、`metric.anim.*`（毫秒）、`metric.font.lineHeight`（系数） |

代码里写死的像素值**不缩放**。想跟随比例就写进主题 metric 或外观 JSON。

---

## 一·补、错误策略与诊断（`json_policy`）

解析失败之后，除了报错，还可以按**策略**选一种恢复动作。三种策略（默认 `STRICT`）：

| 策略 | 字段级错误（颜色/数值非法、`@theme` key 不存在、属性类型不对） | 文件级错误（JSON 语法坏、缺 `layers`） |
|---|---|---|
| `STRICT`（默认） | 整份编译失败 | 整份编译失败 |
| `SKIP_FIELD` | **跳过该字段**（保留默认值），其余照常编译 | 仍然失败（没法跳） |
| `FALLBACK_DEFAULT` | 仍然失败 | **整份退到 default 主题的同名外观**（`@theme` 引用仍按当前主题解析） |

```cpp
#include "bklv/compile/json_policy.hpp"

compile::setJsonErrorPolicy(compile::JsonErrorPolicy::SKIP_FIELD);   // App 启动前设
for (const compile::JsonDiagnostic& d : compile::getJsonDiagnostics())
    // d.file（外观名/视图路径）、d.path（layers[2].fill）、d.message、d.recovered（是否已恢复）
```

- 环境变量 `BKLV_JSON_POLICY=strict|skip|fallback` 也能设（排查用）；**显式 API 优先**。
- **不论哪种策略都会留诊断**：`recovered=false` 表示致命，`true` 表示已按策略恢复。
  `skip` 不是"没发生"，只是"不致命"——调用方/工具应该把 `recovered=true` 的那些显示出来。
- 能跳过的只有**字段级**错误。文件级（JSON 语法、缺 `layers`、`import`/`vars` 循环、
  渐变那五类结构校验）一律失败，`FALLBACK_DEFAULT` 只能"整份退 default"。
- **枚举拼错不受策略影响**：一直是"警告 + 保持原值"（它已等价于跳过；改成致命会破坏第三方主题）。
- 两处以前**静默忽略**、现在会报出来的地方：
  ① **外观顶层键白名单**：`vars` / `import` / `box` / `layers` / `states` / `transitions` /
     `children`（`$` 开头的键当注释放过）。拼错成 `state` / `layer` 会在 STRICT 下失败并提示
     "是否想写 states"；SKIP 下忽略该键 + 留诊断。
  ② **`theme.json` 的 `files[]` 条目少了 `.json` 后缀**：这是**清单写错**（不是缺项），
     打 `error` 并留诊断，该项按缺项回退到 default。后缀齐全但文件真的不在，仍是 warning + 回退。

## 二、视图描述 JSON

```json
{
  "type": "Box",
  "axis": "row",
  "gap": 12,
  "padding": [16, 16, 16, 16],
  "children": [
    { "type": "Label", "text": "@i18n/page/title", "fontSize": 22 },
    { "type": "Padding" },
    { "type": "Image", "src": "@res/img/logo.png", "width": 48, "height": 48 }
  ]
}
```

### 保留键

| 键 | 说明 |
|---|---|
| `type` | **必需**。可写 `Box` 或 `bklv:Box` |
| `children` | 子节点数组（按顺序 `addView`） |
| `include` | 字符串路径：把另一个描述文件整棵接进来（栈深上限 32） |
| `bind` | 数据字段名 → 写到该类型的**主属性**上（Label → `text`，Image → `src`） |
| `bindTo` | 显式指定属性名（Box 这类没有主属性的必须写它） |

### 已注册类型

`Box`（默认 `column`）、`Row`、`Label`、`Image`、`ScrollingFrame`、`HScrollingFrame`、
`ProgressSpinner`、`Padding`。

> `RecyclerFrame` **故意不注册**：它必须先有数据源才能用，光有类型没有意义。

### 容器额外属性（`Box` / `Row` / 各类面板都继承，定义在 `Box::attributes()`）

| 键 | 类型 | 说明 |
|---|---|---|
| `axis` | ENUM | `row` / `column` —— 主轴方向 |
| `direction` | ENUM | `inherit`(默认) / `ltr` / `rtl` |
| `alignItems` | ENUM | `auto` / `flexStart` / `center` / `flexEnd` / `stretch` / `baseline`（默认 `stretch`） |
| `justifyContent` | ENUM | `flexStart` / `center` / `flexEnd` / `spaceBetween` / `spaceAround` / `spaceEvenly` |
| `alignContent` | ENUM | 换行后行与行的对齐：`flexStart` / `center` / `flexEnd` / `stretch` / `spaceBetween` / `spaceAround` |
| `flexWrap` | BOOL | 子节点换行（默认 false） |
| `flexBasis` | SIZE | 主轴初始尺寸（像素 / `"auto"`）；**百分比暂不支持**（警告并按 auto） |
| `gap` `gapRow` `gapColumn` | NUMBER | 子节点间距（像素量）；`gap` 跟 `axis` 走同一根 gutter |
| `padding` `paddingTop` `paddingRight` `paddingBottom` `paddingLeft` | NUMBER | 内边距（像素量）；单边写在后会覆盖 `padding` |

> **`order` 不支持**：这套 Yoga 没有对应接口，JSON 里写它只会得到"未知属性"警告。
> 要在视觉上换顺序就改 `children` 里的书写顺序。

### 公共属性（所有控件，定义在 `View::attributes()`）

| 键 | 类型 | 说明 |
|---|---|---|
| `width` `height` `minWidth` `minHeight` `maxWidth` `maxHeight` | SIZE | 长度语法（含 `"auto"` / `"50%"`） |
| `margin` `marginTop` `marginRight` `marginBottom` `marginLeft` | NUMBER | 外边距（像素量，随 uiScale） |
| `grow` `shrink` | NUMBER | flex 伸缩 |
| `alignSelf` | ENUM | `auto`(默认) / `flexStart` / `center` / `flexEnd` / `stretch` / `baseline` |
| `position` | ENUM | `relative`(默认) / `absolute` |
| `top` `left` `right` `bottom` | SIZE | 绝对定位的四边偏移；数字 / `"50%"` / `"@style/metric.a.b"`，可只写需要的边 |
| `visibility` | ENUM | `visible`(默认) / `invisible`（不画但占位） / `gone`（不占位） |
| `backgroundColor` `borderColor` | COLOR | |
| `borderThickness` `cornerRadius` `alpha` | NUMBER | `alpha` 是 0~1 系数 |
| `clipsToBounds` | BOOL | 裁掉自己和子节点超出本控件矩形的部分 |

> **定位原语**：`absolute` 的子节点**不参与父的 flex 排布**——不占主轴空间，兄弟节点的位置
> 不受它影响（但它仍在 Yoga 树里，父容器尺寸变了照样重算）。四边偏移相对**父容器的外框**算
> （父的 `padding` **不**参与基准，百分比也按父的外框尺寸算）：
> 数字是像素、`"50%"` 是对应边长的百分比（父宽 400 时 `left: "50%"` → x = 200；
> `right: "50%"` → x = 400 - 200 - 自身宽）、`"@style/metric.a.b"` 取主题数值
> （视图 JSON 的 SIZE **只认 `@style/`**，`@theme/` 是颜色用的）。
> 四边可以只写需要的那几条。值不是长度（`"abc"`）、百分比越界（`"120%"`）、
> 枚举值不认识（`"position": "relative-ish"`）都会**建不出控件**并报错（见第 6 节）。

### `Box` / `Row` 追加

| 键 | 取值 |
|---|---|
| `axis` | `row` / `column`（默认） |
| `direction` | `inherit`(默认) / `ltr` / `rtl` |
| `alignItems` | `flexStart`(默认) / `center` / `flexEnd` / `stretch` / `baseline` / `auto` |
| `justifyContent` | `flexStart`(默认) / `center` / `flexEnd` / `spaceBetween` / `spaceAround` / `spaceEvenly` |
| `flexWrap` | BOOL |
| `gap` `gapRow` `gapColumn` | NUMBER |
| `padding` `paddingTop` `paddingRight` `paddingBottom` `paddingLeft` | NUMBER |

### `AppearanceView` / `Button` 追加（内容槽）

| 键 | 说明 |
|---|---|
| `text` | TEXT：文字（Button 用这个改按钮文字） |
| `icon` | TEXT：**图标字形**，写 `@icon/star`（名字）或 `@icon/U+E000`（码位） |
| `info` | TEXT：右侧信息文字 |
| `iconFont` | ENUM：`material` / `switch` —— 图标字形跟哪套字体配套（同一码位在两套字体里是不同图标） |

### `Label` 追加

| 键 | 说明 |
|---|---|
| `text` | TEXT（也可写 `@i18n/x`） |
| `fontSize` | NUMBER |
| `textColor` | COLOR |
| `singleLine` | BOOL（false 时按可用宽度换行） |
| `lineHeight` | NUMBER（行高系数） |
| `horizontalAlign` `verticalAlign` | ENUM：`left` / `center` / `right`，`top` / `center` / `bottom` |

### `Image` 追加

| 键 | 说明 |
|---|---|
| `src` | RESOURCE：图片路径（也可写 `@res/x`） |
| `scalingType` | ENUM：`fit` / `fill` / `stretch` / `center`，**默认 `fit` = 原比例** |
| `scale` | NUMBER：**显示比例**，默认 `1`（原比例）；`fit`/`stretch` 在适配尺寸上再乘，`center` 乘在原尺寸上 |
| `offsetX` `offsetY` | NUMBER：像素平移 |
| `cornerRadius` | NUMBER |
| `placeholderColor` | COLOR：加载中/失败时的底色 |

`scale` / `offsetX` / `offsetY` 在 `scalingType: "fill"` 下**无效**（fill 靠自己铺满 + uv 裁切，
没有可解释的缩放/平移语义），写了会打一条警告；要缩放/平移就改用 `fit` / `stretch` / `center`。
平移可以把自己盒外的内容画出来，需要裁剪就套一层裁剪容器（`ScrollingFrame` / 带裁剪的容器）。

> ✅ **枚举拼写已统一**：两套 JSON 都接受 camelCase 与 snake_case，**大小写、`_`、`-` 都不敏感**
> （`flexEnd` == `flex_end` == `FLEX_END` == `Flex-End`）。下表列的是标准写法，随便挑一种写都行。

### 行为与限制

- **两段式**：`ViewTemplate`（可缓存、与语言无关）→ `instantiate()`（解析 `@i18n/` 等）。
  所以**切语言不会自动重译已有视图**；解析期就替换 `@i18n/` 会把缓存里的语言烤死，别再那么做。
- 路径要写出 `resources/` 这一层：`createFromJSONFile("resources/views/x.json")`。
- 上限：**深度 32 / 节点 4096 / include 栈 32**。
- 出错行为：结构错 → 返回 `nullptr` + 日志；**未知属性只警告**（带拼写建议），不中断。
- 跨「创建视图树」的操作必须 RAII（`std::unique_ptr<View>`），否则中途失败会泄漏半棵树。

---

## 三、控件外观 JSON

文件放在 `resources/theme/<主题>/base/`（基础控件：单个容器/元素）或 `widget/`
（二级控件：引用 base 组合出来的），**必须**在该主题 `theme.json` 的 `files[]` 里登记
（**不扫目录**，没登记 = 不生效 + 一条 warn）。

**外观名 = 相对主题根的路径去掉扩展名**：`base/button.json` → `"base/button"`；
变体是后缀：`base/button.json` + 变体 `primary` → 文件 `base/button.primary.json`。

一份外观 = 三段，`layers` 必填（纯布局控件写 `"layers": []`），另有一个可选的 `import`：

```json
{
  "import":  "base/box.component",
  "box":     { "…": "盒模型与布局" },
  "layers":  [ { "op": "rect", "…": "绘制结构，按数组顺序画" } ],
  "states":  { "focused": { "bg.fill": { "to": "#ffffff", "duration": 120 } } }
}
```

### `import`：外观引用外观

- 值可以是字符串或字符串数组；**逐字段合并，当前文件优先**。
- 对象（`box` / `states` / 单个 state）递归合并；`layers` 按 `id` 合并
  （同 id 的层合并字段、子赢；新 id 追加在后面）；其它数组整体替换；标量覆盖。
- 循环引用 / 链深 > 8 / 父级不存在 → **编译失败**（错误信息指出是哪个 import）。
- 一行里放多个内容层时：层不是流式布局（用 `inset` 排队），并给 box 写
  `"measureContent": "sum"` 让固有宽度按行求和（默认取"最宽的那一层"）。

### `box`：盒模型 / 布局

| 键 | 取值 |
|---|---|
| `width` `height` `minWidth` `minHeight` | 数字 / `"auto"` / `"50%"` / 主题引用 |
| `padding` `margin` | 单值 或 `[上, 右, 下, 左]` |
| `gap` | 数字 |
| `grow` `shrink` | 数字（`AppearanceView` 默认 column） |
| `axis` | `row` / `column` |
| `justifyContent` | `flexStart`(默认) / `center` / `flex_end` / `space_between` / `space_around` / `space_evenly`（`end` = `flex_end`） |
| `alignItems` | `flexStart`(默认) / `center` / `flex_end` / `stretch` / `baseline`（`end` = `flex_end`） |
| `backgroundColor` `borderColor` | 颜色语法 |
| `cornerRadius` `borderThickness` | 数字 |

**只套用写了的字段** —— 空 `box` 不会冲掉控件在代码里的默认值。
反过来，代码里**显式**设过的值（`setCornerRadius(r, explicit=true)`）外观覆盖不了。

### `layers`：绘制结构

通用字段：

| 键 | 适用 op | 说明 |
|---|---|---|
| `op` | 全部 | `rect`(默认) / `text` / `image` / `shadow` / **`glass`** / `custom` |
| `id` | 全部 | 状态里引用它；不写 = `op+序号` |
| `fill` | rect, shadow | 填充色（单色）。同层写了 `gradient` 时以渐变为准 |
| `gradient` | rect | 渐变：`{ type(linear/radial), angle, center, stops[2..4]{at, color} }` |
| `stroke` `strokeWidth` | rect | 描边色 / 宽 |
| `radius` | rect, shadow | 圆角 |
| `color` / `textColor` | text | 文字色 |
| `fontSize` | text | 字号 |
| `data_id` | text, image | 取哪份数据（如 `"text"` = 控件 `setText()` 的值） |
| `scaling` | image | `fit`(默认) / 由控件解释 |
| `opacity` | 全部 | 透明度系数（**不**乘缩放） |
| `width` `height` | 全部 | **给了就按 align/valign 摆**（色条 4、下边线 1）；不给就铺满 inset 收缩后的区域 |
| `inset` | 全部 | `[上,右,下,左]`，负数向外扩（焦点描边就是 -2.5） |
| `offset` | 全部 | `[x, y]` 位移 |
| `align` | 全部 | `left`(默认) / `center` / `right` |
| `valign` | 全部 | `center`(默认) / `top` / `bottom` |
| `feather` `spread` | shadow | 羽化半径 / 额外外扩 |
| `cutout` | shadow | BOOL：挖空控件自身圆角形状，只画外圈（这样后面加的层即使半透明也不会透出阴影） |
| `inner` | shadow | BOOL：改成**内阴影** —— 贴内缘最暗、往内渐透的一圈带子，**没有方向**（`offset` 对它无效）。它画在底色**之后**（外投影在底色之前），且永远不会超出形状。与 `cutout` 互斥（内阴影本来就是空心的） |
| `fill` | glass | 染色（带 alpha）。写了就覆盖 `Shell` 的 `GlassTuning` 染色；不写就用 Shell 的 |
| `opacity` | glass | 这块玻璃的整体不透明度 |
| `radius` | glass | 圆角（半径等于短边一半时就是胶囊/正圆） |

**层序有三条硬约定**（`theme_test` 会遍历两套主题的每一份外观断言）：

0. **`glass` 层必须排在自己的内容层之前**：玻璃画在这一层的位置上，后面的内容层压在它上面；
   排反了内容会被玻璃盖住。详见 [`glass-layer.md`](glass-layer.md)。
1. `hlBg`（焦点高亮底）必须在 `text` / `image` / `custom` **之前** —— 排反了聚焦时那块
   不透明高亮底会把内容盖住（开关/单选图形消失过）。
2. `shadow` 层必须在本控件的底色**之前**。
3. `hlGlow` / `hlBorder` 排在最后（画在内容之上）。

`custom` 层的**内容留给 C++**（`AppearanceView::drawCustomLayer`）：进度弧、滑块轨道、
开关图形这类语义绘制不该塞进 JSON，否则等于写解释器。

#### 层 import：`{ "import": "base/layer/xxx" }`

`layers` 数组里可以放一条**只有 import** 的条目（没有 `op`），它会**就地展开**成那个外观的
`layers` 数组（它的 `box` / `states` / `children` 不会带进来），后面的层照常排在它之后：

```json
"layers": [
  { "import": "base/button/frame" },      // 展开成 shadow / bg / hlBg
  { "op": "text", "id": "title", "data_id": "text" }
]
```

- 适合把「阴影」「玻璃」「背景框」这类基础层拆成独立文件反复叠加；
- 可以写数组一次导入多份：`{ "import": ["base/layer/shadow", "base/layer/border"] }`；
- 被导入文件自己的顶层 `import` 也会先解析（例如 `base/button/frame` 本身）；
- 循环引用、深度 > 8、文件不存在、没有 `layers` 都会**报错**；
- **层 id 不能重复**（导入层与本地层撞名会报错）—— states 与字体覆盖都按 id 找层。
- **同一个 bundle 的 import 指令会合并**：顶层 `import` 的父级写了 `{ "import": "base/layer/shadow" }`，
  子级再写 `{ "import": "base/layer/shadow", "cutout": true }` 时**合成一条**（子级参数覆盖父级），
  不会展开两次变成重复 id。所以“继承基外观的 states + 覆盖某个 bundle 的参数”可以直接写。
- **单层 bundle 可以就地覆盖**：import 旁边的字段会合并到那一层，例如
  `{ "import": "base/layer/shadow", "cutout": false, "offset": [0, 8] }`；
  也可以改 `id` 在同一外观里叠两层阴影。**多层 bundle 带覆盖字段会报错**（语义不明确）。

### `states`：状态动效

key 是 `"<层 id>.<属性名>"`，值是一个补间目标：

```json
"states": {
  "focused": {
    "hlBg.fill":         { "to": "@theme/color.highlight.background", "duration": 120, "easing": "quadraticOut" },
    "hlGlow.strokeWidth": { "to": 11, "duration": 120 }
  }
}
```

| 字段 | 说明 |
|---|---|
| `to` | **必需**。颜色或数值；数值按长度语法（会乘缩放），颜色与 `opacity` 不乘 |
| `duration` | 毫秒，省略 = 0（瞬时） |
| `easing` | 省略 = `linear` |

可用属性名：`fill` `stroke` `color`/`textColor` `strokeWidth` `radius` `opacity`
`offsetX` `offsetY` `insetTop` `insetRight` `insetBottom` `insetLeft` `fontSize`
`feather` `spread`。

可用 easing：`linear`、`quadraticIn/Out/InOut`、`cubicIn/Out/InOut`、`quarticOut`、
`quinticOut`、`sinusoidalIn/Out/InOut`、`exponentialOut`、`circularOut`、`backOut`、
`backInOut`、`bounceOut`、`elasticOut`、`stepped`。

#### 分组目标与 `self`（视图级通道）

除了上面的扁平写法，每个状态还可以按**目标**分组；`self` 表示控件自身，
走**视图级通道**（整棵子树一起动的绘制期变换）：

```json
"states": {
  "focused": {
    "self": { "translateX": { "to": 100, "duration": 160, "easing": "cubicOut" } },
    "bg":   { "fill": { "to": "@theme/color.surface.hover", "duration": 120 },
              "radius": { "to": 10, "duration": 120 } }
  }
}
```

- `self` 的可用通道：`translateX` / `translateY` / `scaleX` / `scaleY` / `alpha` /
  `rotate` / `skewX` / `skewY`（位移乘缩放，其余是系数/弧度不缩放）；
- 层目标里的属性名与扁平写法完全一致；
- 旧的 `"bg.fill": {...}` 继续支持（静默兼容，不告警）；
- 多个状态同时激活时**后激活的赢**；状态关闭时平滑回默认值（120ms）；
- 视图通道是绘制期变换：**不重排、不占位**，命中区域不跟随移动。

状态名不限于内置的 `focused` / `pressed` / `disabled`；控件自定义的（如 Cell 的 `selected`）
由控件自己触发。重算目标时会先撤同一通道正在跑的补间。

### 变体与回退

- **变体**：`setAppearanceVariant("sidebar")` → 读 `<控件名>.sidebar.json`
  （`tab_cell.sidebar.json`、`rectangle.card.json`、`button.primary.json` 都是这么来的）。
  同一控件要多套尺寸就加变体文件，别在代码里改高度。
- **回退链**：当前主题 → `default` 主题 → 找不到文件时给空外观 + 一条 warn（不崩）。
- **编译失败**：`op` 不认识、`states` 引用了不存在的层、`@theme/` key 不存在、
  缺 `layers` → 打 `error` 日志并返回空外观。**不会静默回退成洋红/0**。

---

## 四、主题 JSON

### `resources/theme/themes.json`

```json
{ "default": "default", "available": ["default", "ios"] }
```

### `<主题>/theme.json`

```json
{ "name": "default", "version": 1, "colorScheme": "dark",
  "author": "beiklive", "description": "默认深色主题",
  "files": ["tokens/colors.json", "tokens/metrics.json",
            "base/button.json", "widget/sidebar.json", "view/page_json.json"] }
```

`files[]` 就是这个主题的全部文件清单（**不扫目录**：romfs 上目录迭代真机未验证，
所以加/删文件一定要登记）。`author` / `description` 给设计器"打开项目"用；
老主题的 `widgets[]` 仍然能读，但会打一条"请迁移"的警告。

**`behavior`：主题级的行为默认值**（不是颜色也不是尺寸，但属于主题的一部分）：

```json
"behavior": { "dropdown.placement": "trigger" }
```

| key | 取值 | 说明 |
|---|---|---|
| `dropdown.placement` | `trigger`（默认）/ `center` / `bottom` | 下拉弹层的默认摆放；`center` = 画布居中，`bottom` = 同宽卡片贴底 |

- 查找顺序：当前主题 → default 主题 → 代码兜底（`trigger`）；
- 单个 Dropdown 仍可用代码覆盖：`dd->setPlacement(DropdownPlacement::CENTER)`，
  或视图 JSON 里写 `"placement": "center"`；
- 想让"居中主题"所有下拉都居中，只改这一行即可，不用改任何 C++。

### `<主题>/colors.json`

点是分层分隔符：`"button": { "primaryBg": "#..." }` → `@theme/color.button.primaryBg`。
值只能是 `#RRGGBB` / `#RRGGBBAA`。**不支持 key 之间互相引用**，语义键仍是字面量。

第一组 `common` 是通用调色板 + 原始色值：

| 组 | 键 |
|---|---|
| 基础色板 | `red` `orange` `yellow` `green` `cyan` `blue` `purple` `pink` `brown` `grey`，每个还有 `XxxLight` / `XxxDark` |
| 原色 | `black` `white` `transparent` |
| 半透明 | `overlay`（模态遮罩）`divider`（1px 分割线）`shadow`（阴影基准） |
| 其它 | `focusRing` `textMuted` |

### `<主题>/metrics.json`

同构的数值表：`"cell": { "height": 70 }` → `@theme/metric.cell.height`。
`metric.*` 取值时乘 `uiScale`；`metric.anim.*`（毫秒）与 `metric.font.lineHeight`（系数）
是例外，不乘。

---

## 五、文案 JSON

`resources/i18n/<locale>.json`，嵌套对象，key 用 `/` 取：

```json
{
  "$files": ["pages"],
  "app": { "title": "bklv 演示" },
  "shadow": { "quality_state": "当前档位：{}" }
}
```

| 规则 | 说明 |
|---|---|
| 取值 | `translate("app/title")`；带占位符 `{}` 时 `translate("shadow/quality_state", "smooth")` |
| 拆文件 | 主文件用 `"$files": ["pages"]` 显式列（**不扫目录**；写文件名不带 `.json`，它自己补） |
| locale 名 | 只能用规范名：`zh-Hans`（桌面 `zh-CN`、Switch `zh-Hans` 都归到这里）、`en-US` |
| 装载时机 | `App::App()`，早于视图树构建；**切语言不会自动重译已有视图** |

---

## 六、枚举与字段的容错规则

| 规则 | 说明 |
|---|---|
| 拼写宽容 | 枚举值忽略大小写与 `_` / `-`：`flexEnd` / `flex_end` / `FLEX_END` / `Flex-End` 等价（视图描述 JSON 与外观 JSON 一样） |
| 视图描述 JSON 里拼错 | **不建这个控件** + 可读报错：`JSON 视图 <string>/justifyContent: 期望 枚举，可选值：flexStart|center|flexEnd|…，得到 "spacebetwen"` |
| 外观 JSON 的 box 里拼错 | 打一条警告（带字段名与可用值）并**保持控件当前值不动**（不再静默改成默认值） |
| 外观 JSON 的层字段（`align` / `valign` / `scaling`）拼错 | 编译期警告一次（绘制期不重复报，避免刷屏） |
| `op` / `states` 里引用不存在的层 / `@theme` key 不存在 | **编译失败**（error 日志 + 空外观），不静默兜底 |
| 其余数值/颜色写坏 | 编译失败，报错里有位置（`box.width` / `layers[2].fill` 这种路径） |

各字段的标准写法：

| 概念 | 取值（两种拼写等价） |
|---|---|
| `justifyContent` | `flexStart` / `center` / `flexEnd` / `spaceBetween` / `spaceAround` / `spaceEvenly` |
| `alignItems` | `flexStart` / `center` / `flexEnd` / `stretch` / `baseline`（视图侧还多一个 `auto`） |
| `alignSelf` | `auto` / `flexStart` / `center` / `flexEnd` / `stretch` / `baseline` |
| `axis` | `row` / `column` |
| `direction` | `inherit` / `ltr` / `rtl`（只有视图描述 JSON 有） |
| `visibility` | `visible` / `invisible` / `gone` |
| `position` | `relative` / `absolute` |
| `horizontalAlign` | `left` / `center` / `right` |
| `verticalAlign` | `top` / `center` / `bottom` |
| 图片 `scalingType`（视图）/ 层 `scaling`（外观） | `fit` / `fill` / `stretch` / `center`（层的 `scaling` 目前只由控件自己解释，写别的会警告） |
| 层 `align` / `valign` | `left`/`center`/`right`，`center`/`top`/`bottom` |
| 层 `op` | `rect` / `text` / `image` / `shadow` / `custom` |
| `easing` | 见第 3 节清单；拼错时警告并回退 `linear` |

## 七、常见错误与排查

| 现象 | 原因 |
|---|---|
| 外观改了没反应 | 文件没登记进 `theme.json` 的 `files[]`（日志有 `主题里没有控件外观 "x"`） |
| 日志 `控件外观编译失败：… 缺少 layers 数组` | `layers` 是必填，纯 box 的外观写 `"layers": []` |
| 日志 `引用了主题里不存在的颜色/数值` | key 拼错；写法是 `@theme/color.a.b`，不是 `@theme/color/a.b`（后者会被容错成点分并警告） |
| 聚焦后控件里的图形消失 | 外观层序反了：`hlBg` 排到了内容层之后 |
| 阴影被底色盖住 / 透出来 | **外投影**要写在底色之前（不透明底用不上 `cutout`，半透明底要 `"cutout": true`）；**内阴影相反**，要写在底色之后（`"inner": true`，解析期不会把它提前） |
| 圆角变成直角 | 自己拼矩形带子会被 `AddRectFilled` 把圆角钳到边长一半；用现成的 `radius` / shadow 实现 |
| `"50%"` 没效果 | 百分比只对 `width` / `height` 与绝对定位的四边偏移（`top` / `left` / `right` / `bottom`）有效；其它 NUMBER 属性要绝对像素 |
| 枚举写错 | 视图描述 JSON 会**建不出控件**并在日志里列可选值；外观 JSON 只警告并保持原值（见第 6 节） |
| 切语言界面没变 | 文案在控件构造时已解析成字符串，需要重建视图（见 AGENTS） |

### 渐变（rect 的 `gradient`）

```json
{ "id": "bg", "op": "rect", "radius": 10,
  "gradient": {
    "type": "linear",
    "angle": 90,
    "stops": [ { "at": 0.0, "color": "#2E4A9EFF" },
               { "at": 1.0, "color": "@theme/color.accent.base" } ] } }
```

| 键 | 取值 | 说明 |
|---|---|---|
| `type` | `linear`(默认) / `radial` | 枚举比较大小写与 `_`/`-` 不敏感；拼错**报错**（列出可选值） |
| `angle` | 数字（度） | 0 = 从左到右、90 = 从上到下（顺时针）；只有 `linear` 用。**无量纲，不按 720p 缩放** |
| `center` | `[x, y]`，各 0..1 | 相对层矩形；只有 `radial` 用（默认 `[0.5, 0.5]`） |
| `stops` | 2~4 个 `{ at, color }` | `at` 是 0..1 且必须**严格递增**；`color` 支持 `@theme/...` |

报错规则（都不静默降级）：`at` 越界、`at` 不递增、停靠点数量不在 2~4、停靠点缺 `color`、
`type` 不认识 —— 都会带上「是哪个 stop / 当前值」的提示。

与 `import` 合并时：`gradient` 是对象按字段合并，`stops` 是数组**整体替换**。
与 `fill` 的关系：写了 `gradient` 以它为准；`fill` 继续服务状态覆盖（状态里写 `fill` 的层不带渐变）。
**渐变不做补间**（不是标量通道），也不支持给 `stroke` 加渐变 —— 见 [`rect-gradient.md`](rect-gradient.md)。

### 侧栏面板（`widget/sidebar.json`）

侧栏的面板观感**只由 `box` 段决定**（`Sidebar::applyPanelLook()` 走 `applyWidgetBox`）：

```json
"box": {
  "width": "@theme/metric.sidebar.width",
  "margin": "@theme/metric.sidebar.margin",
  "padding": [ "@theme/metric.sidebar.paddingTop", "@theme/metric.sidebar.paddingRight",
               "@theme/metric.sidebar.paddingBottom", "@theme/metric.sidebar.paddingLeft" ],
  "cornerRadius": "@theme/metric.sidebar.radius",
  "borderThickness": 1,
  "borderColor": "@theme/color.sidebar.border",
  "backgroundColor": "@theme/color.sidebar.background"
}
```

- `layers` 段在这里**不参与绘制**，但**必须存在**（少了会报「外观 JSON 缺少 layers 数组」）
- 面板投影没有对应的 box 字段，由 `Sidebar::draw()` 用 `shadow::drawSoft(..., cutout=true)` 补
- 条目外观是 `base/tab_cell.sidebar.json`：`icon` 层（`"data_id": "icon"`，自动用图标字体）+
  `title` 层（`inset` 让位）+ 选中态的 `bg.fill` / `accent.fill` / `title.color` / `icon.color`
- 分组用 `Sidebar::addSeparator()`（不可聚焦、不进条目索引）

### 视图过渡（`transitions`）

外观 JSON 的顶层 `transitions` 段描述**视图生命周期**的动画：`enter` / `exit` / `show` / `hide`。
和 `states` 的分工：`states` 管控件内部状态（选中/聚焦/按下…），`transitions` 管出现与消失。

```json
"transitions": {
  "enter": {
    "duration": 200, "easing": "quadraticOut",
    "translateX": { "from": -60, "to": 0 },
    "alpha":      { "from": 0,   "to": 1 },
    "layers": {
      "title": { "when": "after", "duration": 140,
                 "offsetX": { "from": -20, "to": 0 },
                 "opacity": { "from": 0,   "to": 1 } }
    }
  },
  "exit": { "alpha": { "from": 1, "to": 0 } },
  "show": { "alpha": { "from": 0, "to": 1 } },
  "hide": { "duration": 0, "alpha": { "from": 1, "to": 0 } }
}
```

| 位置 | 可写的键 |
|---|---|
| 事件级 | `duration`（毫秒，默认取主题 `metric.anim.normal`）、`easing`、`delay` |
| 视图级通道（整树一起动） | `translateX` / `translateY` / `scaleX` / `scaleY` / `alpha` |
| 层内通道 | `layers.<层id>.<属性名>`，属性就是 `states` 那 16 个 |
| 通道对象 | `from`（可省）、`to`（必写）、`duration`、`easing`、`delay` |
| 层内条目级 | `when`（`with` 默认 / `after`）、`delay`、`duration`、`easing` |

四条语义：

1. **不做预设** —— 没有 `"type": "slideFade"` 这种枚举，效果直接写通道名（写了 `type` 会报错并列出可选通道）
2. **外观里没写 `transitions`** → 调用方用默认的**淡入淡出**；**事件里没写某个层** → 那层不动，跟着整树一起淡
3. **`when: "after"`** = 这条层内动画延后到**视图级动画结束**才开始（解析期就把它加进 `delay`）；
   出场 / 隐藏写 `after` 只会得到一条警告并按 `with` 处理 —— 那时视图已经看不见了
4. **`duration: 0` = 瞬变**；写 `when: "before"` 会报错（出现之前 / 消失之后看不见，
   想让内容先动、整页后动请用 `delay` 表达提前量）

运行时（`core/transition_player.*` + `core/transition.hpp` 的 `ViewTransition`）：

- **外观里写了就用外观的**：视图级通道走 `View` 的绘制期变换（整树一起动、过渡期间零重排），
  层内通道写进 `AppearanceInstance`（开始前先撤掉同通道的状态补间 —— 状态动效和过渡不能同时对一条属性下手）
- **没写就是默认淡入淡出**（入场/显示 0→1、出场/隐藏 1→0），时长取主题 `metric.anim.normal`
- 没写 `from` 的通道**起点取当前值**
- `ViewTransition::getActiveDuration()` 报**本次实际时长**（spec 的 `totalMs` 或配置值），
  `Shell/Pager::getTransitionDuration()` 报的就是它 —— **出场播完才摘页/摘弹层**靠这里
- `playShow` / `playHide` 只有"外观里写了"或"默认淡入淡出"两种；隐藏要不要真的 `GONE`/摘掉
  由调用方在回调里做（提前设 GONE 动画根本播不出来）
- spec 路径收尾**不复位**（终值就是外观里写的，比如出场停在屏幕外）；预设路径照旧复位

时间账（解析期算好）：`totalMs = max(事件时长, 每条条目 delay + duration)`，
调用方拿 `totalMs` 决定"这段动画播完了"的时机 —— 出场播完才摘页 / 摘弹层，报早了动画会被砍。

## 扩展图层类型（第三方）

内置 `op` 只有 6 种：`rect` / `text` / `image` / `shadow` / `glass` / `custom`。
**别人可以加自己的**，而且**不需要改 `bklv/` 里任何文件**：写一个文件 + 注册一行，
主题里就能写 `"op": "你的名字"`。完整可运行的样例在 `demo/contrib/stripe_layer.cpp`。

### 推荐写法：注册模板 + 参数模式

```cpp
#include "bklv/compile/layer_register.hpp" // registerLayer<T>() 模板

struct StripeLayer
{
    static constexpr const char* kName     = "stripe";   // JSON 里的 op 值
    static constexpr const char* kLabel    = "斜条纹";    // 设计器显示名
    static constexpr const char* kIdPrefix = "stripe";   // 可省，默认用 kName
    static constexpr const char* kHelp     = "斜条纹：angle / lineWidth / gap";
    static constexpr auto kOrder = bklv::compile::LayerOrder::DECORATION;

    // 参数模式：声明一次，引擎负责解析（类型 / 720p 缩放 / 拼错提示），
    // 设计器的候选键、中文名与 ENUM 下拉都由它派生 —— 不用手写 keys，也不用写 parse。
    static std::vector<bklv::compile::LayerParam> params()
    {
        return {
            bklv::compile::LayerParam::number("angle", 45.0f, "角度"),
            bklv::compile::LayerParam::pixels("lineWidth", 2.0f, "线宽"),
            bklv::compile::LayerParam::pixels("gap", 16.0f, "间距"),
        };
    }

    static void draw(const bklv::compile::LayerDrawContext& ctx);
};

// 在你自己的初始化里调一次（第三方程序就是在 main 里调）
bklv::compile::registerLayer<StripeLayer>();
```

### 参数类型（五种）

| 工厂 | JSON 写法 | 说明 |
|---|---|---|
| `LayerParam::number(name, def, label)` | `0.5` | 数字，**不缩放**（系数 / 角度 / 比例） |
| `LayerParam::pixels(name, def, label)` | `16` / `"@style/metric.x"` | 像素量，按 720p 乘 `uiScale` |
| `LayerParam::color(name, def, label)` | `"#RRGGBB"` / `"@theme/color.a.b"` | 颜色 |
| `LayerParam::boolean(name, def, label)` | `true` / `false` | 布尔 |
| `LayerParam::enumeration(name, values, defIndex, label)` | `"vertical"` | 候选取值；拼错**只警告**并保持默认 |

**JSON 里没写 = 用模式里的默认值**（不是 0）—— "没写"和"写 0"是两件事。
参数值按声明顺序存在 `AppearanceLayer::params`（每层上限 8 个，超了注册期报错），
绘制钩子**按名字取**：

```cpp
static void draw(const bklv::compile::LayerDrawContext& ctx)
{
    const float angle = bklv::compile::numberParam(ctx, "angle", 45.0f);
    const float gap   = bklv::compile::numberParam(ctx, "gap", 16.0f);
    const bool  mono  = bklv::compile::boolParam(ctx, "mono", false);
    const char* mode  = bklv::compile::enumParam(ctx, "mode");  // 枚举值翻回字符串

    ImDrawList* drawList = static_cast<ImDrawList*>(ctx.drawList); // 不透明指针，见下
    // ctx.rect 已含这一层的矩形、圆角与有效 alpha；
    // ctx.layer.fill / .radius / .inset … 是引擎白送的通用字段（见下）
}
```

**`void*` 是有意的**：`compile/` 不认识 ImGui 与 widgets，所以上下文里的
`drawList`（= `ImDrawList*`）与 `view`（= `AppearanceView*`）用不透明指针传，
扩展层自己转回去。

### 引擎白送的字段（任何 op 都能用）

`fill` / `stroke` / `color`(`textColor`) / `strokeWidth` / `radius` / `opacity` / `fontSize` /
`width` / `height` / `feather` / `spread` / `cutout` / `inner` / `inset` / `offset` / `gradient`，
外加元信息 `op` / `id` / `data_id` / `align` / `valign` / `scaling` —— 全都解析好放在
`ctx.layer` 上，绘制钩子直接读。完整清单见 `compile::engineLayerKeys()`。

**别把这些名字再声明成参数**：那样同一层会有两份值（引擎一份、参数槽一份）。注册时会警告。

### 需要自己的结构参数时（可选 parse 钩子）

标量 / 颜色 / 枚举都不用写解析代码；只有数组、对象这类结构（像内置 `rect` 的 `gradient`）
才要自己写。`T` 里写了 `static parse` 就自动挂上（检测惯用法，不用手动赋给 `LayerType::parse`）：

```cpp
static bool parse(const Json& node, const ThemeData& theme, AppearanceLayer& out,
                  std::string& error, const std::string& where); // false = 整份外观编译失败
```

### 样例二：不规则四边形（多参数 + 钩子自己算动画）

`demo/contrib/quad_layer.cpp` 是第二个样例：**四个顶点各自随时间随机游走**，
演示"多参数 + 每帧自己算位置"的写法（`stripe` 那三个参数是静态的）。

```cpp
static std::vector<bklv::compile::LayerParam> params()
{
    return {
        bklv::compile::LayerParam::pixels("amplitude", 8.0f, "幅度"),   // 顶点最大位移
        bklv::compile::LayerParam::number("speed", 1.5f, "速度"),       // 每秒几个时间片
        bklv::compile::LayerParam::number("seed", 1.0f, "种子"),        // 同种子同画面
        bklv::compile::LayerParam::number("gain0", 1.0f, "左上幅度"),   // 每顶点幅度系数
        bklv::compile::LayerParam::number("gain1", 1.0f, "右上幅度"),
        bklv::compile::LayerParam::number("gain2", 1.0f, "右下幅度"),
        bklv::compile::LayerParam::number("gain3", 1.0f, "左下幅度"),
    };
}
```

主题里这样用（`amplitude` 会按 720p 缩放，`speed`/`seed` 不缩放）：

```json
{ "op": "quad", "id": "blob",
  "amplitude": 10, "speed": 1.2, "seed": 7,
  "gain0": 1.0, "gain1": 0.6, "gain2": 1.3, "gain3": 0.8,
  "fill": "@theme/color.accent.base",
  "stroke": "@theme/color.text.primary", "strokeWidth": 2,
  "inset": 6 }
```

**参数补间目前没有**（补间通道是固定的 `AppearanceProp` 枚举），所以"要动起来"的层
在绘制钩子里自己算时间与随机：

- 时间用 `bklv::getSecondsNow()`（`core/time.hpp`，单调、无窗口安全）；
  **不要**用 `ImGui::GetTime()` 或 `ImGui::GetIO()` —— 无窗口时没有 ImGui 上下文。
- 随机用**纯哈希 / 值噪声**（同输入同输出），把它放在一个纯函数里，就能无窗口测
  （见 `tests/quad_layer_test.cpp`：确定性、取值有界、相邻时刻连续、四顶点互不相同）。
- 幅度夹取要夹**最终位移**：`gain` 这种"乘在后面的系数"能绕过对 `amplitude` 的夹取
  （写 `gain0: 9` 时顶点会飞到层外面）。

### 三条硬规则

1. **参数名不许占用引擎保留键**：`op` / `id` / `width` / `height` / `inset` / `align` / `valign` /
   `offset` / `opacity` 是层几何与通用属性，引擎会先拿它们算矩形 —— 注册时**直接报错**
   （内置 6 种豁免）。踩过的后果很隐蔽：样例曾用 `width` 表示线宽，整个层被做成 3px 宽的条，
   画面上只剩左边缘几个毛刺。
2. **失败要报错，不要静默降级**：参数类型不符、ENUM 没给候选、参数超过 8 个、参数名重复，
   都在注册期或解析期报清楚（解析失败 = 整份外观编译失败并记日志）。
3. **draw 里不要再查 JSON、不要再查主题**：外观值在编译期就定下来，绘制只读
   `ctx.layer` / `ctx.rect` / `ctx.type`（和内置层同一条约定）。

### 注册之后免费得到

- **解析**：主题里写 `"op": "stripe"` 就能编译，落到 `Op::EXTENSION` + `extensionType`；
  **没注册过的 op 仍然报错**（不静默放过）
- **默认 id**：用你的 `idPrefix`（`stripe0` / `stripe1`），`states` / `transitions` 能按 id 寻址
- **设计器**：加层菜单、中文名、检查器候选键、参数中文名、ENUM 下拉全部从注册表与参数模式读
  → 第三方不用改设计器
- **绘制**：引擎的 `EXTENSION` 分支查注册表调你的 `draw`，并把"矩形 + 圆角 + 有效 alpha"给你
- **层序**：内置层之间按 **JSON 书写顺序**（只有阴影、玻璃被提到最前）；声明
  `DECORATION` / `OVERLAY` 的扩展层会画在所有内置层之上。**不按七档阶梯重排内置层**
  —— 那会让 121 份发布外观里的 `hlGlow` / `hlBorder` 掉到文字下面（见 `layerSortRank` 的注释）

### 注意

- 扩展层**只画，不参与布局**（不能改变控件尺寸或推动兄弟节点）—— 要"能装子节点"的组合
  属于控件级扩展（外观 JSON 的 `children`），不是 `op`。
- **别把用了扩展 op 的外观放进 `resources/theme/default/`**：那是随库发布的默认主题，
  而扩展 op 由**调用方**注册 —— 库的随包资源依赖 demo/第三方代码是本末倒置
  （默认主题里曾留下的 `base/stripe_demo.json` 就是这个反例，已按评审 §4.2 P0④ 撤掉）。
  演示/一次性外观直接在页面里**内联**：`AppearanceView::applyAppearanceJson(json, error)`
  就地编译一份（demo 的 JSON 页就是这么显示 quad 的）；换主题时若要让内联外观跟着重解析
  `@theme/`，在自己的子类里重写 `applyTheme()` 再套一次。
- 主题里用扩展 op 时，**主题加载前必须已经注册**（调用方的初始化里注册）。
  测试侧要整主题校验的话，注册要放在测试开头（见 `tests/test_contrib.hpp`）。
- 参数**目前不参与 `states` / `transitions` 补间**（补间通道是固定的 `AppearanceProp` 枚举）；
  需要动起来就画在 `custom` 层里、由控件自己按时间算。

## 外观里的子节点（`children`）

一份外观可以**自己声明子节点** —— 也就是"**一份外观 = 一个完整控件**"。结构、样式全在 JSON 里，
C++ 一行都不用写，也不用新加控件类。

```json
{
  "box": { "axis": "row", "gap": 10, "padding": 12, "alignItems": "center" },
  "layers": [ { "op": "rect", "id": "bg", "fill": "@theme/color.surface.card", "radius": 12 } ],
  "children": [
    { "appearance": "base/tab_cell.sidebar", "box": { "width": 36, "height": 36 }, "icon": "@icon/star" },
    { "box": { "grow": 1 }, "children": [
        { "appearance": "base/json_demo.title", "text": "标题" },
        { "appearance": "base/json_demo.title", "text": "副标题" }
    ] }
  ]
}
```

上面这份就是 demo 里的 `base/children_demo`：一张"图标 + 标题 + 副标题"的卡片。

### 三档节点（互斥，写两个报错）

| 写法 | 建出什么 |
|---|---|
| `"appearance": "<外观名>"` | 一个 `AppearanceView`（名字规则与 `AppearanceView` 一样，含 `.变体`） |
| 只写 `"box"` | 一个纯布局 `Box`（递归嵌套的载体，想套多深都行） |
| `"ref": "<视图模板名>"` | 用主题 `view/` 里的**视图模板**实例化 —— **库里已有的一切控件都能这么引用** |

公共可选键：
- `"box"`：覆盖/追加盒模型字段（**只覆盖写了的字段**，沿用"没写不动"的既有约定）
- `"children"`：递归（`ref` 节点不能再写，它的子节点写在模板里）
- 内容槽：`"text"` / `"icon"` / `"info"` —— 实例化后套到 `AppearanceView` 的既有槽位上。
  `icon` 支持 `"@icon/star"` 这种名字（会查字形），也可以直接写字形

### 与 `layers` 的本质区别

- `layers` **不进布局**：只是在这块矩形里画一笔笔（靠 `inset`/`align` 手排）
- `children` **进布局**：是真正的子视图，参与 flex 排版、能聚焦、能滚动

所以：**要"画装饰"用 `layers`，要"装控件"用 `children`**。

### 错误规则（全部硬错误，不静默降级）

- `appearance` 与 `ref` 同时写 -> 报错
- 三样都没写 -> 报错（不知道该建什么）
- `ref` 节点再写 `children` -> 报错（子节点写在模板里）
- **嵌套深度上限 16**，超过报错
- **循环引用**：A 的子节点里又引用 A（或 A→B→A）-> 报错并停止构建
- `box` 段里不认识的键：按主 `box` 段的既有行为处理（`children` 要与 `box` **平级**，
  写进 `box` 里不会生效 —— 这一点有测试钉住）

### 有什么、没什么（当前版本）

有：
- 三档节点、任意递归、`box` 覆盖、内容槽、循环与深度保护
- 子节点自己的外观由它自己负责（`states` / `transitions` 照常用）

没有（有意留到后面，不是忘了）：
- **父外观改子节点的状态**：`states` 的寻址空间仍然只有"层"（`<层id>.<属性>`），
  不会扩成树路径
- ~~`children` 的 `box` 段只认少数几个键~~ **已解决**：它现在与外观主 `box` 段**共用同一份解析**
  （`compile/appearance_box.cpp`），所以 `axis` / `alignItems` / `justifyContent` / `margin` /
  `padding` / `minWidth` / 背景 / 圆角 / 边框 / `measureContent` 等**全套字段都支持**，
  "只覆盖写了的字段"（`hasXxx` 标记）的语义也天然一致

## Recycler：JSON 描述列表/网格（结构归 JSON，数据与行为归 C++）

三条硬分工（这就是「JSON 只描述容器与 item 的组织方式」）：

| 谁 | 负责 | 在哪 |
|---|---|---|
| **JSON** | 容器类型、**一份** item 模板、列规则、数据源**名字**、绑定函数**名字**、段头模板、预加载数值 | 主题 `view/*.json` |
| **数据源**（注册） | "第 N 项是谁"：总数/高度/身份/分段 | C++ `data::registerSource("名字", 工厂)` |
| **绑定函数**（注册） | "第 N 项长什么样、点了干什么" | C++ `binding::registerItemBinder("名字", 函数)` |

**JSON 属性表**（`widgets/recycler_view.cpp` 的 `attributes()`）：

| 键 | 类型 | 说明 |
|---|---|---|
| `data` | STRING | 数据源**名字**，可写 `@data/xxx`（前缀会剥掉）；没注册过 -> 警告 + 不挂 |
| `itemAppearance` | STRING | item 的**一份**模板（外观名），所有 item 共用 |
| `itemTemplate` | STRING | 同上，但用**视图模板**（`loadViewTemplateFromTheme` + `instantiate`）|
| `itemBinder` | STRING | 绑定函数名：`(View& item, size_t 行号)` 里套数据/挂行为 |
| `sectionHeaderAppearance` | STRING | 段头模板（数据源给"有几段/段标题"，JSON 只描述段头长什么样）|
| `columns` | NUMBER | 列数（**Grid 就是 Recycler 的多列模式**，不另设类型）|
| `minItemWidth` | NUMBER | 自动列数用（0 = 用 `columns`）|
| `itemHeight` | NUMBER | 数据源没给高度时的兜底 |
| `preFetchLine` | NUMBER | 预取行数（默认 1）|
| `showScrollingIndicator` | BOOL | 是否显示滚动条 |
| `slotLoading` / `slotEmpty` | STRING | loading / empty 具名槽位（视图模板名）；**由 C++ 选择显示哪个**，不做 `if` 解释器 |
| `onEnd` | STRING | 增量加载的挂点**名字**（实现留 C++，见 `docs/list-incremental-loading-design.md`）|
| `count` | STRING | 目前只认 `"infinite"`（或省略），别的值警告 |

**两条容易踩的**（都踩过）：
1. **json 类型要先注册再装载**：`registerRecyclerViewType()` 由 `registerWidgetViewTypes()` 调用；
   测试里要自己调一次，否则报「未知类型 "Recycler"」。
2. **绑定函数挂在两个索引入口上**：回收器分段路径走 `setSectionIndex(段, 段内下标)`，
   单段路径才走 `setIndex(下标)` —— 只重写一个会让绑定函数**永远不被调用**（现象是列表整条空白）。

**能力边界（当前）**：`columns > 1` 已实现（视觉行顶按行内最高项算）；
**未知总数**的滚动条长度/焦点策略见 `docs/list-incremental-loading-design.md`（设计稿，未实现）。

# JSON 语法速查

bklv 里一共有四类 JSON，各管一件事。这份文档是它们的**完整语法**（键名、取值形式、
枚举拼写、默认值、报错行为），全部对照代码整理，不是示意。

| 类别 | 位置 | 用途 | 解析处 |
|---|---|---|---|
| 视图描述 | `resources/views/*.json`，或 `createFromJSONString()` | 用 JSON 搭视图树（类型 + 属性 + 子节点） | `core/json_view.cpp` `core/attributes.cpp` |
| 控件外观 | `resources/theme/<主题>/widgets/<控件>[.<变体>].json` | 一个控件的盒模型 / 绘制层 / 状态动效 | `core/appearance.cpp` |
| 主题 | `resources/theme/themes.json` + `<主题>/{theme,colors,metrics}.json` | 主题清单、颜色表、数值表 | `core/theme_data.cpp` |
| 文案 | `resources/i18n/<locale>.json` | i18n 文本 | `core/i18n.cpp` |

---

## 一、通用取值语法

### 长度 / 数值

| 写法 | 含义 |
|---|---|
| `12` 或 `"12"` | 像素，按 **1280x720 设计稿**写，运行时乘 `uiScale`（见 `core/ui_scale.*`） |
| `"auto"` | 由内容决定（只有 `width` / `height` 支持） |
| `"50%"` | 相对父容器的百分比（只有 `width` / `height` 支持） |
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

### 缩放规则（记住这一条就不会算错）

| 会乘 `uiScale` | 不乘 |
|---|---|
| `metric.*`（主题数值）、外观里的 `radius` / `strokeWidth` / `width` / `height` / `fontSize` / `inset` / `offset` / `feather` / `spread` | `opacity`（系数）、`duration`（毫秒）、`metric.anim.*`（毫秒）、`metric.font.lineHeight`（系数） |

代码里写死的像素值**不缩放**。想跟随比例就写进主题 metric 或外观 JSON。

---

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

### 公共属性（所有控件，定义在 `View::attributes()`）

| 键 | 类型 | 说明 |
|---|---|---|
| `width` `height` `minWidth` `minHeight` `maxWidth` `maxHeight` | SIZE | 长度语法（含 `"auto"` / `"50%"`） |
| `margin` `marginTop` `marginRight` `marginBottom` `marginLeft` | NUMBER | |
| `grow` `shrink` | NUMBER | flex 伸缩 |
| `alignSelf` | ENUM | `auto`(默认) / `flexStart` / `center` / `flexEnd` / `stretch` / `baseline` |
| `position` | ENUM | `relative`(默认) / `absolute` |
| `top` `left` `right` `bottom` | SIZE | 配合 `position: absolute` |
| `visibility` | ENUM | `visible`(默认) / `invisible`（不画但占位） / `gone`（不占位） |
| `backgroundColor` `borderColor` | COLOR | |
| `borderThickness` `cornerRadius` `alpha` | NUMBER | `alpha` 是 0~1 系数 |
| `clipsToBounds` | BOOL | 裁掉自己和子节点超出本控件矩形的部分 |

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
| `scalingType` | ENUM：`fit` / `fill` / `stretch` / `center` |
| `cornerRadius` | NUMBER |
| `placeholderColor` | COLOR：加载中/失败时的底色 |

> ⚠ **两套 DSL 的枚举拼写不一样，这是当前的实情**（见文末"已知不一致"）：
> 视图描述 JSON 用 **camelCase**（`flexEnd` / `spaceBetween`），
> 控件外观 JSON 用 **snake_case**（`flex_end` / `space_between`）。照表抄，别混用。

### 行为与限制

- **两段式**：`ViewTemplate`（可缓存、与语言无关）→ `instantiate()`（解析 `@i18n/` 等）。
  所以**切语言不会自动重译已有视图**；解析期就替换 `@i18n/` 会把缓存里的语言烤死，别再那么做。
- 路径要写出 `resources/` 这一层：`createFromJSONFile("resources/views/x.json")`。
- 上限：**深度 32 / 节点 4096 / include 栈 32**。
- 出错行为：结构错 → 返回 `nullptr` + 日志；**未知属性只警告**（带拼写建议），不中断。
- 跨「创建视图树」的操作必须 RAII（`std::unique_ptr<View>`），否则中途失败会泄漏半棵树。

---

## 三、控件外观 JSON

文件放在 `resources/theme/<主题>/widgets/`，**必须**在该主题 `theme.json` 的 `widgets[]`
里登记（**不扫目录**，没登记 = 不生效 + 一条 warn）。

一份外观 = 三段，`layers` 必填（纯布局控件写 `"layers": []`）：

```json
{
  "box":     { "…": "盒模型与布局" },
  "layers":  [ { "op": "rect", "…": "绘制结构，按数组顺序画" } ],
  "states":  { "focused": { "bg.fill": { "to": "#ffffff", "duration": 120 } } }
}
```

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
| `op` | 全部 | `rect`(默认) / `text` / `image` / `shadow` / `custom` |
| `id` | 全部 | 状态里引用它；不写 = `op+序号` |
| `fill` | rect, shadow | 填充色 |
| `stroke` `strokeWidth` | rect | 描边色 / 宽 |
| `radius` | rect, shadow | 圆角 |
| `color` / `textColor` | text | 文字色 |
| `fontSize` | text | 字号 |
| `source` | text, image | 取哪份数据（如 `"text"` = 控件 `setText()` 的值） |
| `scaling` | image | `fit`(默认) / 由控件解释 |
| `opacity` | 全部 | 透明度系数（**不**乘缩放） |
| `width` `height` | 全部 | **给了就按 align/valign 摆**（色条 4、下边线 1）；不给就铺满 inset 收缩后的区域 |
| `inset` | 全部 | `[上,右,下,左]`，负数向外扩（焦点描边就是 -2.5） |
| `offset` | 全部 | `[x, y]` 位移 |
| `align` | 全部 | `left`(默认) / `center` / `right` |
| `valign` | 全部 | `center`(默认) / `top` / `bottom` |
| `feather` `spread` | shadow | 羽化半径 / 额外外扩 |
| `cutout` | shadow | BOOL：挖空控件自身圆角形状，只画外圈（这样后面加的层即使半透明也不会透出阴影） |

**层序有三条硬约定**（`theme_test` 会遍历两套主题的每一份外观断言）：

1. `hlBg`（焦点高亮底）必须在 `text` / `image` / `custom` **之前** —— 排反了聚焦时那块
   不透明高亮底会把内容盖住（开关/单选图形消失过）。
2. `shadow` 层必须在本控件的底色**之前**。
3. `hlGlow` / `hlBorder` 排在最后（画在内容之上）。

`custom` 层的**内容留给 C++**（`AppearanceView::drawCustomLayer`）：进度弧、滑块轨道、
开关图形这类语义绘制不该塞进 JSON，否则等于写解释器。

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
{ "default": "default", "available": ["default", "ocean"] }
```

### `<主题>/theme.json`

```json
{ "name": "default", "version": 1, "colorScheme": "dark",
  "widgets": ["widgets/button.json", "widgets/card.json", "…"] }
```

`widgets[]` 就是这个主题的全部外观文件清单（**不扫目录**：romfs 上目录迭代真机未验证，
所以加外观文件一定要登记）。

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
| 取值 | `getStr("app/title")`；带占位符 `{}` 时 `getStr("shadow/quality_state", "smooth")` |
| 拆文件 | 主文件用 `"$files": ["pages"]` 显式列（**不扫目录**；写文件名不带 `.json`，它自己补） |
| locale 名 | 只能用规范名：`zh-Hans`（桌面 `zh-CN`、Switch `zh-Hans` 都归到这里）、`en-US` |
| 装载时机 | `App::App()`，早于视图树构建；**切语言不会自动重译已有视图** |

---

## 六、已知不一致（照抄时注意）

| 概念 | 视图描述 JSON | 控件外观 JSON |
|---|---|---|
| 主轴对齐 `justifyContent` | `flexStart` / `center` / `flexEnd` / `spaceBetween` / `spaceAround` / `spaceEvenly` | `flexStart` / `center` / `flex_end` / `space_between` / `space_around` / `space_evenly`（`end` 也是 `flex_end`） |
| 交叉轴对齐 `alignItems` | `flexStart` / `center` / `flexEnd` / `stretch` / `baseline` / `auto` | `flexStart` / `center` / `flex_end` / `stretch` / `baseline` |
| 方向 | `axis`（`row`/`column`）+ `direction`（`inherit`/`ltr`/`rtl`） | 只有 `axis`（`row`/`column`） |
| 图片缩放 | `scalingType`（`fit`/`fill`/`stretch`/`center`） | 层的 `scaling`（`fit` + 控件自己解释） |

两边都写错时的表现不同：视图描述 JSON 里拼错枚举值会**静默落到默认值**
（对齐变回 `flexStart`），外观 JSON 里拼错则落到最后一个 `if` 的兜底值。
建议将来统一成"两边都接受 camelCase 与 snake_case"，改之前先按上表写。

## 七、常见错误与排查

| 现象 | 原因 |
|---|---|
| 外观改了没反应 | 文件没登记进 `theme.json` 的 `widgets[]`（日志有 `主题里没有控件外观 "x"`） |
| 日志 `控件外观编译失败：… 缺少 layers 数组` | `layers` 是必填，纯 box 的外观写 `"layers": []` |
| 日志 `引用了主题里不存在的颜色/数值` | key 拼错；写法是 `@theme/color.a.b`，不是 `@theme/color/a.b`（后者会被容错成点分并警告） |
| 聚焦后控件里的图形消失 | 外观层序反了：`hlBg` 排到了内容层之后 |
| 阴影被底色盖住 / 透出来 | `shadow` 层要在底色之前；不透明底用不上 `cutout`，半透明底要 `"cutout": true` |
| 圆角变成直角 | 自己拼矩形带子会被 `AddRectFilled` 把圆角钳到边长一半；用现成的 `radius` / shadow 实现 |
| `"50%"` 没效果 | 只有 `width` / `height` 支持百分比 |
| 切语言界面没变 | 文案在控件构造时已解析成字符串，需要重建视图（见 AGENTS） |

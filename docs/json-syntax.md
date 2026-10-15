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
  "author": "beiklive", "description": "默认深色主题",
  "files": ["tokens/colors.json", "tokens/metrics.json",
            "base/button.json", "widget/sidebar.json", "view/page_json.json"] }
```

`files[]` 就是这个主题的全部文件清单（**不扫目录**：romfs 上目录迭代真机未验证，
所以加/删文件一定要登记）。`author` / `description` 给设计器"打开项目"用；
老主题的 `widgets[]` 仍然能读，但会打一条"请迁移"的警告。

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
| 阴影被底色盖住 / 透出来 | `shadow` 层要在底色之前；不透明底用不上 `cutout`，半透明底要 `"cutout": true` |
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
- 条目外观是 `base/tab_cell.sidebar.json`：`icon` 层（`"source": "icon"`，自动用图标字体）+
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

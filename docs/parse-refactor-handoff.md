# BekUI 解析层重构 —— 交付说明与示例

> 这份是给你（和第三方使用者）看的"现状 + 怎么用"。完整评审与过程记录在
> `docs/parse-refactor-review.md`，用法细节在 `docs/json-syntax.md`。

## 1. 目录结构（重构后）

解析层现在**独立成 `bklv/compile/`**，一种关注点一个文件：

```
bklv/compile/                     ← 只做「JSON -> 数据 + 错误」，不建视图、不含运行时
  appearance_box.cpp             165 行
  appearance_children.cpp        134 行
  appearance_gradient.cpp        128 行
  appearance_imports.cpp         147 行
  appearance_layer_fields.cpp     90 行
  appearance_layer_meta.cpp      110 行
  json_util.cpp                  134 行
  layer_registry.cpp             162 行
  theme_parse.cpp                568 行
  view_parse.cpp                 650 行
  appearance_box.hpp              25 行
  appearance_children.hpp         25 行
  appearance_gradient.hpp         22 行
  appearance_imports.hpp          25 行
  appearance_layer_fields.hpp     24 行
  appearance_layer_meta.hpp       27 行
  json_util.hpp                   47 行
  layer_registry.hpp              99 行

bklv/core/                        ← 运行时与数据模型（tweeny 只出现在 appearance.cpp）
  appearance.cpp                   1334 行   编译产物 + AppearanceInstance（补间）
  appearance.hpp                    379 行   公开头（路径不变，调用方零改动）
  box.hpp / view.hpp / ...               控件与布局
  json_view.hpp / theme_data.hpp         公开头（实现已搬去 compile/，头原地不动）

bklv/widgets/                     ← 看图、建视图
  appearance_view.cpp                    绘制（EXTENSION 走注册表的 draw 钩子）
  appearance_children.cpp                按 children 建子视图
```

## 2. 改动数字（都经过 `check.sh` 3422 项 + 三平台零告警）

| | 起点 | 现在 |
|---|---|---|
| `Appearance::compile` | 501 行 | **53 行** |
| `parseLayers` | 382 行 | **30 行** |
| `core/appearance.cpp` | 1906 行 | **1354 行** |
| `parseStates` | — | **12 行**（拆成 parseStates 12 / parseAllStates 23 / parseStateProps 67，全部 ≤80）|
| 解析层落点 | 散在 `core/` | **`bklv/compile/` 10 个文件** |

## 3. 示例 JSON（这份是**真实可编译**的：已进主题清单，`theme_test` 每轮都会编译它）

文件：`resources/theme/default/base/sample_card.json`
用法：`new AppearanceView("base/sample_card", false)` —— 一张卡片，
**结构（图标 + 标题 + 副标题 + 右侧间隔）全在 JSON 里，C++ 一行结构都没写**。

```json
{
  "box": {
    "axis": "row",
    "gap": 12,
    "padding": 12,
    "alignItems": "center"
  },
  "layers": [
    {
      "op": "shadow",
      "id": "shadow",
      "fill": "@theme/color.common.shadow",
      "feather": 16,
      "offset": [
        0,
        4
      ],
      "cutout": true
    },
    {
      "op": "rect",
      "id": "bg",
      "fill": "@theme/color.surface.card",
      "radius": 14
    }
  ],
  "children": [
    {
      "appearance": "base/tab_cell.sidebar",
      "box": {
        "width": 40,
        "height": 40
      },
      "icon": "@icon/star"
    },
    {
      "box": {
        "axis": "column",
        "gap": 4,
        "grow": 1
      },
      "children": [
        {
          "appearance": "base/json_demo.title",
          "text": "标题"
        },
        {
          "appearance": "base/json_demo.title",
          "text": "副标题"
        }
      ]
    },
    {
      "box": {
        "width": 8
      }
    }
  ],
  "states": {
    "pressed": {
      "bg.fill": {
        "to": "@theme/color.accent.base",
        "duration": 120
      }
    }
  },
  "transitions": {
    "enter": {
      "alpha": {
        "from": 0,
        "to": 1,
        "duration": 200
      }
    }
  }
}
```

### 逐段说明

| 段 | 说明 |
|---|---|
| `box` | 这一个控件自己的盒模型：`axis: row` 横向排、`gap` 子节点间距、`padding` 内边距、`alignItems: center` 垂直居中 |
| `layers` | **不进布局**的装饰层：`shadow` 必须排在底色之前（`cutout: true` 只画外圈，半透明底也不会透出阴影）；`rect` 是卡片底 |
| `children` | **进布局**的子控件（三档之一 `appearance`）：① 40×40 的图标块（带 `icon: "@icon/star"`）；② 一个 `grow: 1` 的横向盒，里面一列"标题/副标题"；③ 8px 宽的纯布局间隔盒（只写 `box` 那一档） |
| `states` | 属性动效：按下时把 `bg` 层的 `fill` 补到强调色，120ms |
| `transitions` | 视图过渡：入场时整树 alpha 0 -> 1，200ms |

### `children` 的三档（互斥，写两个报错）

```json
{ "appearance": "base/tab_cell.sidebar" }   // 引用一份外观 -> 建 AppearanceView
{ "box": { "width": 8 } }                  // 只写 box      -> 建纯布局 Box
{ "ref": "view/children_ref_demo" }         // 引用视图模板   -> 库里的控件都能这么用
```

`box` 段与外观主 `box` 段**共用同一份解析**，所以 `axis / alignItems / justifyContent / margin /
minWidth / 背景 / 圆角 / 边框 / measureContent` 全都支持（第 42 轮统一）。嵌套上限 16 层，
`appearance` 循环引用会报错并停止。

## 4. 第三方加一种自己的图层（不改 `bklv/` 任何文件）

```cpp
// 你自己仓库里的一个文件 + 初始化时注册一行（完整样例：demo/contrib/stripe_layer.cpp）
bklv::compile::LayerType t;
t.name = "stripe"; t.label = "斜条纹"; t.idPrefix = "stripe";
t.order = bklv::compile::LayerOrder::DECORATION;
t.keys  = { "angle", "lineWidth", "gap", "fill" };   // **不要用引擎保留键**
t.parse = myParse;   // 读自己的键 -> AppearanceLayer
t.draw  = myDraw;    // 绘制（引擎把矩形/圆角/有效 alpha 交给你）
bklv::compile::registerLayer(std::move(t), error);
```

注册之后：主题里能写 `"op": "stripe"`、默认 id 用你的 `idPrefix`、设计器的加层菜单/中文名/
候选键自动有它、`states`/`transitions` 能按层 id 寻址。保留键清单（`op/id/width/height/inset/
align/valign/offset/opacity`）会在注册时被拒绝并告诉你改名。

## 5. 还剩什么

1. ~~`parseStates` 87 -> ≤80~~ **已完成**（拆成三个函数、12/23/67 行）
2. ③ 内置 6 种绘制仍走枚举分支（扩展 op 已经走注册表 draw 钩子）—— 这是最后一块
3. ~~收尾截图对比~~ **已完成**（无回归：整图 JPEG 差 0.08%）

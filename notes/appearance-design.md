# 主题与外观：设计笔记

目标：**换肤 = 复制 `resources/theme/default/` 改名 + 改 JSON**，不需要改 C++、不需要重新编译。
控件的外观（连绘制结构）由 JSON 描述，状态动效由 tweeny 对属性插值。

## 1. 目录与分类（两级）

```
resources/theme/
  themes.json               主题清单：可用主题名 + 默认主题（**不扫目录**）
  default/
    theme.json              元信息 + 本主题的外观文件清单（同样显式列出）
    colors.json             全局色板   -> 拍平成 color.*
    metrics.json            全局数值   -> 拍平成 metric.*
    widgets/
      button.json           按控件分类的外观描述
      button.ghost.json     变体：文件名带点，控件用 variant 指向它
      rectangle.json
```

**为什么不扫目录**：Switch 的 romfs 上目录迭代真机未验证（i18n 那边也踩过同一条）。
所以主题清单和外观文件清单都显式列出来。代价：加一个控件外观要在 `theme.json` 里登记一行。

## 2. 一份外观长什么样

```json
{
  "layers": [
    { "op": "rect", "id": "bg", "fill": "@theme/color.surface.raised",
      "stroke": "@theme/color.border.default", "strokeWidth": 0,
      "radius": "@theme/metric.radius.md" },
    { "op": "text", "id": "title", "source": "text",
      "color": "@theme/color.text.primary",
      "fontSize": "@theme/metric.font.size.md",
      "align": "center", "valign": "center", "inset": [0, 16, 0, 16] }
  ],
  "states": {
    "focused": {
      "bg.fill":     { "to": "@theme/color.accent.base", "duration": 120, "easing": "quadraticOut" },
      "title.color": { "to": "@theme/color.text.onAccent", "duration": 120 }
    },
    "pressed":  { "bg.fill": { "to": "@theme/color.accent.pressed", "duration": 60 } },
    "disabled": { "bg.fill": { "to": "@theme/color.state.disabled", "duration": 150 } }
  }
}
```

- 层类型：`rect` / `text` / `image` / `custom`
- 13 个可插值属性；颜色按 4 通道、标量按 1 通道，**各自一个 tweeny tween**
- 状态属性写成 `"<层 id>.<属性名>"`；状态之间有优先级
  （disabled < focused < hover < pressed < selected）
- 补间从**当前值**补到目标值，所以连点两下不会跳变；
  取消状态后回到层的初值

## 3. 为什么保留 `custom` 层

`Label` 的换行/截断、`Image` 的缩放、`ProgressSpinner` 的弧、`Slider` 的轨道是**语义性绘制**，
不是装饰。把它们也塞进 JSON 等于写一个绘制解释器：性能（每帧解释 + 无法内联）和
可调试性（栈里看不到具体控件）都会明显变差。

所以边界是：**装饰层（底/边框/文字/图片/动效）走 JSON；语义绘制留在 C++，用 `custom` 层占位**。
`AppearanceView::drawCustomLayer()` 是那个出口。

## 4. 运行链路

```
启动/换肤  ThemeData::load()      读 JSON、解析 @theme/ 引用、拍平成 color.* / metric.*
           getWidgetAppearance()  编译外观（按 主题名/控件名 缓存）
控件构造   AppearanceView::applyTheme()  AppearanceInstance 抄初值
状态变化   AppearanceInstance::setState()  对变化了的属性启动 tweeny 补间
每帧       update() -> tweeny.step(dt) -> 属性当前值
绘制       draw() 逐层解释：rect/text/image/custom（draw 里不查主题）
```

换肤：`App::setTheme(name)` → 重新装载主题 + 清外观缓存 → `root->applyThemeRecursive()`
（每个控件重新取外观）→ `requestLayout()`（主题可能改了间距/字号）。

## 5. 主题只覆盖「未被显式设置过的值」

`Label` 这类控件既有主题默认值、又允许代码单独设定。约定：
调过 `setFontSize/setTextColor` 就算**显式管理**，`applyTheme()` 不再覆盖它；
想让某个 Label 跟着主题走，调 `setFontSizeExplicit(false)` / `setTextColorExplicit(false)`。

不这么做的话，用户代码精心调过的字号会被换肤冲掉。

## 6. 缺失项的回退

| 缺什么 | 行为 |
|---|---|
| 皮肤里没有某个颜色/数值 key | 回退 `default` 主题的同名 key |
| 皮肤里没有某个控件外观文件 | 回退 `default` 的该文件 |
| 两边都没有 | 控件记一条警告，用 `AppearanceInstance` 的空外观（不绘制装饰层，不崩） |
| 主题目录整个不存在 | `setTheme` 返回 false，保持当前主题 |

## 7. 相关文件

```
bklv/core/theme_data.hpp/.cpp     主题装载、拍平、回退
bklv/core/appearance.hpp/.cpp     外观编译 + 运行期补间（tweeny）
bklv/widgets/appearance_view.*    外观驱动的控件基类（解释并绘制 layers）
bklv/widgets/rectangle.*          纯装饰控件（示例：本身没有绘制代码）
bklv/widgets/button.*             按钮（语义：文字/点击/禁用；外观全在 JSON）
resources/theme/default|ios/    两套皮肤（ios 是复制 default 改出来的）
tests/theme_test.cpp              44 项：装载/回退/外观编译/状态动效/换肤
tests/widget_test.cpp             24 项：外观绑定/状态/换肤对已建视图生效/显式值不被覆盖
```

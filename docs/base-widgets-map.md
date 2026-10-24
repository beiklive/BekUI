# 基础控件页：控件 / 代码 / 路径 / JSON 外观名 对照表

> **2026-10 控件清理**：`bklv/widgets` 现在只剩 `AppearanceView` / `Button` / `Notification`
> + 六个 `layer_draw_*`。本文提到的 `Shell` / `Header` / `BottomBar` / `TabBar` / `Sidebar` /
> `TabFrame` / `Cell` 家族 / `Dialog` / `Dropdown` / `Slider` / `HintsBar` / `StatusArea`
> 以及它们在 `resources/theme/default/base/**` 下的外观 JSON 都已删除 —— 这些段落作为
> "重做时的约定"保留，别当成现成 API。

页面源码 `demo/app/pages/page_base_widgets.cpp`（一节 = header + 本体 + 各状态行）
；页面文案 `resources/i18n/zh-Hans.json` 的 `base_widgets/*`。

## 先记住三条

1. **外观名 = 主题目录里的相对路径去掉 `.json`**：`base/box.json` → 外观名 `base/box`；
   变体用后缀拼：`base/box` + variant `component` → `base/box.component`。
2. **外观只覆盖写了的字段**：没写的字段保留上一个值（换外观时旧值不会自动清掉，
   `base/widget/button.icon` 就得显式写 `minWidth: 0` 才能压掉 `base/button` 的 120）。
3. **两套主题**：`default`（扁平）与 `ios`（苹果风格）；清单在各自 `theme.json` 的 `files[]`。
   两套都是整份清单；某一套没写的字段会**回退 default**（下面标了「缺」的只针对后来的精简主题）。

## 改哪里 → 改什么

| 想改的东西 | 改哪里 |
|---|---|
| 尺寸 / 圆角 / 边框 / 内边距 / 外边距 | 外观 JSON 的 `box` 段；共用值改 `tokens/metrics.json` 的 `metric.*` |
| 底色 / 描边色 / 文字色 | 外观 JSON 的 `layers[].fill|stroke|color`（`@theme/color.*`），或 `tokens/colors.json` |
| 状态动效（聚焦/按下/禁用/开关） | 外观 JSON 的 `states.*`（`{ "to": …, "duration": …, "easing": … }`） |
| 层序 / 新增装饰层 | 外观 JSON 的 `layers`（高亮底必须在内容层之前） |
| 控件行为（内容、几何语义绘制、交互） | 对应 C++ 文件（下表「类文件」那一列） |
| 聚焦框（流光框） | **不在这里**：`Shell::setFocusRingEnabled/Type`；外观里的 focused 只剩 `hlBg.fill` |

---

## 1. 容器 · 基本盒

- 文案 key：`base_widgets/sec_box`
- 控件：`Rectangle` · 类文件：`bklv/ui/widgets/rectangle.hpp`
- 外观：`base/box`
- 备注：**只有盒模型**（axis/height），`layers` 是空的 —— 底/边框在 `base/box.component` 里。

**`base/box`**
- default：`resources/theme/default/base/box.json`
  - 层：（没有层）
  - 状态 / box：（没有状态） box:axis, height
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 基本盒：唯一"有底有框"的基础容器
{
        Rectangle* section = makeSection("base_widgets/sec_box");
        Rectangle* box    = new Rectangle();
        box->setAppearanceName("base/box");
        box->addView(bodyText(translate("base_widgets/box_inner")));
        section->addView(makeItemRow(box));
        section->addView(makeHint(translate("base_widgets/box_hint")));
        page->addView(section);
    }
```
</details>

## 2. 容器 · 布局

- 文案 key：`base_widgets/sec_layout`
- 控件：`Rectangle` · 类文件：`bklv/ui/widgets/rectangle.hpp`
- 外观：`base/box`、`base/box.layout`
- 备注：布局容器：`setAppearanceVariant("layout")` → 无底无框、只有内边距。

**`base/box`**
- default：`resources/theme/default/base/box.json`
  - 层：（没有层）
  - 状态 / box：（没有状态） box:axis, height
- ios：见 `resources/theme/ios` 同名文件

**`base/box.layout`**
- default：`resources/theme/default/base/box.layout.json`
  - 层：（没有层）
  - 状态 / box：（没有状态） import:base/box box:padding
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 布局容器：只负责摆位置（横排 + 居中），自己不带底
{
        Rectangle* section = makeSection("base_widgets/sec_layout");
        Rectangle* layout  = makeLayoutBox();
        layout->setAxis(Axis::ROW);
        layout->setAlignItems(AlignItems::CENTER);
        layout->addView(makeText("layout", getTheme().number("metric.font.size.sm", 15.0f),
            getTheme().color("color.text.dim")));
        Rectangle* inner = makeComponentBox();
        inner->addView(bodyText(translate("base_widgets/component_inner")));
        layout->addView(inner);
        section->addView(makeItemRow(layout));
        section->addView(makeHint(translate("base_widgets/layout_hint")));
        page->addView(section);
    }
```
</details>

## 3. 容器 · 组件内嵌

- 文案 key：`base_widgets/sec_component`
- 控件：`Rectangle` · 类文件：`bklv/ui/widgets/rectangle.hpp`
- 外观：`base/box.component`
- 备注：组件容器：`setAppearanceVariant("component")`，它 `import` 了 `base/box` —— 改 base，所有按钮一起变。边框/圆角就在这份文件的 `metric.component.*` 上。

**`base/box.component`**
- default：`resources/theme/default/base/box.component.json`
  - 层：shadow / bg
  - 状态 / box：（没有状态） import:base/box box:measureContent, padding
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 组件容器：它 import 了 base/box，改 base 这里一起变
{
        Rectangle* section = makeSection("base_widgets/sec_component");
        Rectangle* box     = makeComponentBox();
        box->addView(bodyText(translate("base_widgets/component_inner")));
        section->addView(makeItemRow(box));
        section->addView(makeHint(translate("base_widgets/component_hint")));
        page->addView(section);
    }
```
</details>

## 4. 元素 · 文本 Label

- 文案 key：`base_widgets/sec_label`
- 控件：`Label` · 类文件：`bklv/ui/widgets/basic/label.hpp`
- 外观：**没有外观**
- 备注：**没有外观**：字号/颜色/换行/截断都是代码设的。

<details><summary>页面里的代码</summary>

```cpp
// 文本：普通 / 次级 / 单行截断
{
        Rectangle* section = makeSection("base_widgets/sec_label");
        section->addView(makeItemRow(bodyText(translate("base_widgets/label_body"))));
        section->addView(makeItemRow(makeHint(translate("base_widgets/label_dim"))));
        Label* truncated = makeText(translate("base_widgets/label_long"),
            getTheme().number("metric.font.size.sm", 15.0f),
            getTheme().color("color.text.primary"), true);
        section->addView(makeStateRow("base_widgets/state_single_line", truncated));
        section->addView(makeHint(translate("base_widgets/label_hint")));
        page->addView(section);
    }
```
</details>

## 5. 元素 · 图片 Image

- 文案 key：`base_widgets/sec_image`
- 控件：`Image` · 类文件：`bklv/ui/widgets/basic/image.hpp`
- 外观：**没有外观**
- 备注：**没有外观**：资源 `resources/testcard.png`，尺寸代码设。

<details><summary>页面里的代码</summary>

```cpp
// 图片：解码后上传成纹理，尺寸由布局决定
{
        Rectangle* section = makeSection("base_widgets/sec_image");
        Image* image       = new Image();
        image->setImageFromFileAsync("resources/testcard.png");
        image->setWidth(120.0f);
        image->setHeight(120.0f);
        Box* row = makeRow(getTheme().number("metric.spacing.md", 12.0f));
        row->addView(image);
        row->addView(makeHint(translate("base_widgets/image_hint")));
        section->addView(row);
        page->addView(section);
    }
```
</details>

## 6. 元素 · 滑块 Slider

- 文案 key：`base_widgets/sec_slider`
- 控件：`Slider` · 类文件：`bklv/ui/widgets/slider.hpp`
- 外观：`base/slider`
- 备注：几何是 C++ 语义绘制；颜色/圆角/粗细在 JSON。

**`base/slider`**
- default：`resources/theme/default/base/slider.json`
  - 层：track / fill / knob / geometry
  - 状态 / box：focused / disabled box:height, width
- ios：`resources/theme/ios/base/slider.json`
  - 层：track / fill / knob / geometry
  - 状态 / box：focused / disabled box:height, width
  - 层：track / fill / knob / geometry
  - 状态 / box：focused / disabled box:height, width

<details><summary>页面里的代码</summary>

```cpp
// 滑块：进度决定几何（语义绘制），颜色/圆角/粗细来自 JSON
{
        Rectangle* section = makeSection("base_widgets/sec_slider");
        Label* value       = makeHint(translate("base_widgets/slider_value", 40));
        Slider* slider     = new Slider();
        slider->setProgress(0.4f);
        slider->setOnChange([value](float progress) {
            value->setText(translate("base_widgets/slider_value", (int)(progress * 100.0f)));
        });
        section->addView(makeStateRow("base_widgets/state_default", slider));
        section->addView(value);
        section->addView(makeHint(translate("base_widgets/slider_hint")));
        page->addView(section);
    }
```
</details>

## 7. 元素 · 进度圈 Spinner

- 文案 key：`base_widgets/sec_spinner`
- 控件：`ProgressSpinner` · 类文件：`bklv/ui/widgets/basic/progress_spinner.hpp`
- 外观：**没有外观**
- 备注：**没有外观 JSON**（纯视图）：颜色取主题 `color.spinner.bar`，时长/粗细是代码写的（1.2s / 2.4s）。

<details><summary>页面里的代码</summary>

```cpp
// 进度圈：ImDrawList 没有旋转 API，自己在 C++ 里拼四边形
{
        Rectangle* section = makeSection("base_widgets/sec_spinner");
        Box* row           = makeRow(getTheme().number("metric.spacing.lg", 16.0f));
        row->addView(new ProgressSpinner());
        ProgressSpinner* slow = new ProgressSpinner();
        slow->setDuration(2.4f);
        slow->setThickness(4.0f);
        row->addView(slow);
        row->addView(makeHint(translate("base_widgets/spinner_hint")));
        section->addView(row);
        page->addView(section);
    }
```
</details>

## 8. 按钮 · 纯文字

- 文案 key：`base_widgets/sec_btn_text`
- 控件：`Button` · 类文件：`bklv/ui/widgets/controls/button.hpp`
- 外观：`base/widget/button.text`
- 备注：普通按钮：文字居中。

**`base/widget/button.text`**
- default：`resources/theme/default/base/widget/button.text.json`
  - 层：hlBg / text / hlGlow / hlBorder
  - 状态 / box：focused / disabled import:base/box.component box:alignItems, axis, justifyContent
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 纯文字：最普通的按钮，四种状态都摆出来
{
        Rectangle* section = makeSection("base_widgets/sec_btn_text");
        addButtonStates(section, "text", translate("base_widgets/btn_text"), nullptr);
        section->addView(makeHint(translate("base_widgets/states_hint")));
        page->addView(section);
    }
```
</details>

## 9. 按钮 · 纯图标

- 文案 key：`base_widgets/sec_btn_icon`
- 控件：`Button` · 类文件：`bklv/ui/widgets/controls/button.hpp`
- 外观：`base/widget/button.icon`、`base/widget/button.icon_circle`
- 备注：两款：正方形（`icon`，48×48 + `minWidth: 0`）/ 圆形（`icon_circle`，五层 radius = `metric.radius.round`）。

**`base/widget/button.icon`**
- default：`resources/theme/default/base/widget/button.icon.json`
  - 层：hlBg / icon / hlGlow / hlBorder
  - 状态 / box：focused / disabled import:base/box.component box:alignItems, axis, height, justifyContent, minWidth, padding, width
- ios：见 `resources/theme/ios` 同名文件

**`base/widget/button.icon_circle`**
- default：`resources/theme/default/base/widget/button.icon_circle.json`
  - 层：shadow / bg / hlBg / hlGlow / hlBorder
  - 状态 / box：（没有状态） import:base/widget/button.icon
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 纯图标：两款 —— 正方形（icon）与圆形（icon_circle），图标都完全居中
{
        Rectangle* section = makeSection("base_widgets/sec_btn_icon");
        section->addView(makeHint(translate("base_widgets/shape_square")));
        addButtonStates(section, "icon", "", "star", false);
        section->addView(makeHint(translate("base_widgets/shape_circle")));
        addButtonStates(section, "icon_circle", "", "settings", false);
        section->addView(makeHint(translate("base_widgets/icon_hint")));
        page->addView(section);
    }
```
</details>

## 10. 按钮 · 图标 + 文字

- 文案 key：`base_widgets/sec_btn_icon_text`
- 控件：`Button` · 类文件：`bklv/ui/widgets/controls/button.hpp`
- 外观：`base/widget/button.icon_text`
- 备注：图标 + 文字：靠层的 `inset` 排队。

**`base/widget/button.icon_text`**
- default：`resources/theme/default/base/widget/button.icon_text.json`
  - 层：hlBg / icon / text / hlGlow / hlBorder
  - 状态 / box：focused / disabled import:base/box.component box:alignItems, axis, gap
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 图标 + 文字
{
        Rectangle* section = makeSection("base_widgets/sec_btn_icon_text");
        addButtonStates(section, "icon_text", translate("base_widgets/btn_icon_text"), "save");
        page->addView(section);
    }
```
</details>

## 11. 按钮 · 列表（无边框）

- 文案 key：`base_widgets/sec_btn_list`
- 控件：`Button` · 类文件：`bklv/ui/widgets/controls/button.hpp`
- 外观：`base/widget/button.list`
- 备注：列表按钮：`bg.stroke` 透明 + `strokeWidth: 0` 去边框，文字层 `align: left`。

**`base/widget/button.list`**
- default：`resources/theme/default/base/widget/button.list.json`
  - 层：bg / text
  - 状态 / box：disabled import:base/widget/button.text
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 列表按钮：普通按钮的"列表元素"款 —— 没有边框，文字正常左对齐
{
        Rectangle* section = makeSection("base_widgets/sec_btn_list");
        addButtonStates(section, "list", translate("base_widgets/btn_list"), nullptr);
        section->addView(makeHint(translate("base_widgets/list_hint")));
        page->addView(section);
    }
```
</details>

## 12. 按钮 · 信息型

- 文案 key：`base_widgets/sec_btn_info`
- 控件：`Button` · 类文件：`bklv/ui/widgets/controls/button.hpp`
- 外观：`base/widget/button.info`
- 备注：信息型：右侧 `info` 层文字由程序给。

**`base/widget/button.info`**
- default：`resources/theme/default/base/widget/button.info.json`
  - 层：hlBg / icon / text / info / hlGlow / hlBorder
  - 状态 / box：focused / disabled import:base/box.component box:alignItems, axis, gap
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 信息按钮：右侧信息文字由程序决定
{
        Rectangle* section = makeSection("base_widgets/sec_btn_info");
        addButtonStates(section, "info", translate("base_widgets/btn_info"), "home");
        Button* live = makeVariantButton("info", translate("base_widgets/btn_info"));
        live->setIcon(glyph("home"));
        live->setInfoText(translate("base_widgets/info_value"));
        live->setOnClick([live] {
            static int clicks = 0;
            clicks++;
            live->setInfoText(std::to_string(clicks));
        });
        section->addView(makeStateRow("base_widgets/state_clickable", live));
        section->addView(makeHint(translate("base_widgets/info_hint")));
        page->addView(section);
    }
```
</details>

## 13. 按钮 · 开关（文字）

- 文案 key：`base_widgets/sec_switch_text`
- 控件：`Button` · 类文件：`bklv/ui/widgets/controls/button.hpp`
- 外观：`base/widget/button.switch_text`
- 备注：文字开关：`on` / `off` 两个外观状态。

**`base/widget/button.switch_text`**
- default：`resources/theme/default/base/widget/button.switch_text.json`
  - 层：hlBg / icon / text / info / hlGlow / hlBorder
  - 状态 / box：focused / on / off / disabled import:base/box.component box:alignItems, axis, gap
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 文字开关：开 / 关两个状态（信息文字跟着变）
{
        Rectangle* section = makeSection("base_widgets/sec_switch_text");
        addSwitchStates(section, "switch_text", translate("base_widgets/btn_switch_text"), "info");
        Button* live = makeVariantButton("switch_text", translate("base_widgets/btn_switch_text"));
        live->setIcon(glyph("info"));
        live->setSwitched(false);
        live->setInfoText(translate("base_widgets/off"));
        live->setOnClick([live] {
            live->setSwitched(!live->isSwitched());
            live->setInfoText(live->isSwitched() ? translate("base_widgets/on")
                                                 : translate("base_widgets/off"));
        });
        section->addView(makeStateRow("base_widgets/state_clickable", live));
        page->addView(section);
    }
```
</details>

## 14. 按钮 · 开关（图案）

- 文案 key：`base_widgets/sec_switch_icon`
- 控件：`Button` · 类文件：`bklv/ui/widgets/controls/button.hpp`
- 外观：`base/widget/button.switch_icon`
- 备注：图案开关：轨道 + 滑块，位移是 `knob.offsetX` 补间。

**`base/widget/button.switch_icon`**
- default：`resources/theme/default/base/widget/button.switch_icon.json`
  - 层：hlBg / icon / text / track / knob / hlGlow / hlBorder
  - 状态 / box：focused / on / off / disabled import:base/box.component box:alignItems, axis, gap
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 图案开关：轨道 + 滑块，切换有过渡动画（补间写在 on/off 状态里）
{
        Rectangle* section = makeSection("base_widgets/sec_switch_icon");
        addSwitchStates(section, "switch_icon", translate("base_widgets/btn_switch_icon"), "settings");
        Button* live = makeVariantButton("switch_icon", translate("base_widgets/btn_switch_icon"));
        live->setIcon(glyph("settings"));
        live->setSwitched(true);
        live->setOnClick([live] { live->setSwitched(!live->isSwitched()); });
        section->addView(makeStateRow("base_widgets/state_clickable", live));
        section->addView(makeHint(translate("base_widgets/switch_hint")));
        page->addView(section);
    }
```
</details>

## 15. 按钮 · 下拉触发器

- 文案 key：`base_widgets/sec_dropdown`
- 控件：`Dropdown` · 类文件：`bklv/ui/widgets/dropdown.hpp`
- 外观：`base/widget/button.dropdown`、`base/dropdown_popup`、`base/dropdown_item`
- 备注：触发器外观挂在 `dropdown->getTrigger()` 上；弹窗是 `base/dropdown_popup` + 选项 `base/dropdown_item`。

**`base/widget/button.dropdown`**
- default：`resources/theme/default/base/widget/button.dropdown.json`
  - 层：hlBg / icon / text / info / hlGlow / hlBorder
  - 状态 / box：focused / disabled import:base/box.component box:alignItems, axis, gap
- ios：见 `resources/theme/ios` 同名文件

**`base/dropdown_popup`**
- default：`resources/theme/default/base/dropdown_popup.json`
  - 层：shadow / bg
  - 状态 / box：（没有状态） box:cornerRadius, padding, width
- ios：`resources/theme/ios/base/dropdown_popup.json`
  - 层：shadow / bg
  - 状态 / box：（没有状态） box:cornerRadius, padding, width
  - 层：shadow / bg
  - 状态 / box：（没有状态） box:cornerRadius, padding, width

**`base/dropdown_item`**
- default：`resources/theme/default/base/dropdown_item.json`
  - 层：bg / label
  - 状态 / box：focused / selected box:height, padding, width
- ios：`resources/theme/ios/base/dropdown_item.json`
  - 层：bg / label
  - 状态 / box：focused / selected box:height, padding, width
  - 层：bg / label
  - 状态 / box：focused / selected box:height, padding, width

<details><summary>页面里的代码</summary>

```cpp
// 下拉触发器：点它弹遮罩列表（弹层复用 Dropdown）
{
        Rectangle* section = makeSection("base_widgets/sec_dropdown");
        Dropdown* dropdown = new Dropdown();
        dropdown->setOptions({ translate("base_widgets/option_a"), translate("base_widgets/option_b"),
            translate("base_widgets/option_c") }, 0);
        AppearanceView* trigger = dropdown->getTrigger();
        trigger->setAppearanceName("base/widget/button.dropdown");
        trigger->setIcon(glyph("list"));
        trigger->setText(translate("base_widgets/btn_dropdown"));
        trigger->setInfoText(dropdown->getSelectedText());
        dropdown->setOnChange([dropdown, trigger](size_t) {
            trigger->setInfoText(dropdown->getSelectedText());
        });
        section->addView(makeItemRow(dropdown));
        section->addView(makeHint(translate("base_widgets/dropdown_hint")));
        page->addView(section);
    }
```
</details>

## 16. 选择器 · 数值

- 文案 key：`base_widgets/sec_selector_number`
- 控件：`SelectorButton` · 类文件：`bklv/ui/widgets/selector_button.hpp`
- 外观：`base/widget/button.selector`
- 备注：数值模式：0~100、步长 5；代码里 setNumberRange。

**`base/widget/button.selector`**
- default：`resources/theme/default/base/widget/button.selector.json`
  - 层：hlBg / icon / text / arrowLeft / info / arrowRight / hlGlow / hlBorder
  - 状态 / box：focused / editing / disabled import:base/box.component box:alignItems, axis, gap
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 选择器（数值模式）：进入设置态后焦点钉住，LR / 方向键调值，长按连发
{
        Rectangle* section = makeSection("base_widgets/sec_selector_number");
        SelectorButton* number = new SelectorButton(translate("base_widgets/btn_selector_number"));
        number->setIcon(glyph("palette"));
        number->setNumberRange(0.0f, 100.0f, 5.0f, 1, 40.0f);
        number->setOnValueChanged(
            [](float value) { BKLV_LOG_INFO("选择器（数值）：%.1f", (double)value); });
        section->addView(makeItemRow(number));
        section->addView(makeHint(translate("base_widgets/selector_hint")));
        page->addView(section);
    }
```
</details>

## 17. 选择器 · 列表

- 文案 key：`base_widgets/sec_selector_list`
- 控件：`SelectorButton` · 类文件：`bklv/ui/widgets/selector_button.hpp`
- 外观：`base/widget/button.selector`
- 备注：列表模式：LR 切换、越界环绕；选项文字是代码给的。

**`base/widget/button.selector`**
- default：`resources/theme/default/base/widget/button.selector.json`
  - 层：hlBg / icon / text / arrowLeft / info / arrowRight / hlGlow / hlBorder
  - 状态 / box：focused / editing / disabled import:base/box.component box:alignItems, axis, gap
- ios：见 `resources/theme/ios` 同名文件

<details><summary>页面里的代码</summary>

```cpp
// 选择器（列表模式）：LR 切换选项，越界环绕
{
        Rectangle* section = makeSection("base_widgets/sec_selector_list");
        SelectorButton* list = new SelectorButton(translate("base_widgets/btn_selector_list"));
        list->setIcon(glyph("list"));
        list->setOptions({ translate("base_widgets/option_a"), translate("base_widgets/option_b"),
            translate("base_widgets/option_c") });
        list->setArrowStyle(SelectorArrow::LR);
        list->setOnSelectionChanged([](int index, const std::string& text) {
            BKLV_LOG_INFO("选择器（列表）：" + std::to_string(index) + " " + text);
        });
        section->addView(makeItemRow(list));
        page->addView(section);
    }
```
</details>


## 液态玻璃版本

基础控件页里**每个控件都追加了一行液态玻璃版本**（不另开页面）：

| 控件类别 | 玻璃版本怎么来 |
|---|---|
| 有自己面的控件（按钮一族 / 开关 / 下拉触发器 / 选择器） | 外观变体 `glass`（`base/widget/button.*.glass`）：底换成 `op: "glass"`，文字/图标不变 |
| 没有面的控件（文字 / 图片 / 滑块 / 进度圈） | 放在玻璃板 `base/glass_plate` 上 |

小节卡片用半透明玻璃卡（`base/rectangle.card.glass`）、壁纸铺在页面第一层，
这样玻璃才有背景可采。规则与验收见 [`glass-layer.md`](glass-layer.md)。

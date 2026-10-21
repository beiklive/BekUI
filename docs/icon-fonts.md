# 图标字体：两套字体、码位配对、按层指定

bklv 里有**两套图标字体**，它们是两个全局指针，控件/外观层各自声明"这一层用哪一套"。
这份文档把规则写死在这里，避免再出现"字写对了却画成豆腐块 / 画成别的图标"。

## 1. 两套字体是哪两套

| 代号 | 指针 | 字体文件 | 装它的入口 | 用途 |
|---|---|---|---|---|
| `IconFontKind::MATERIAL` | `icon::font()` | `resources/font/MaterialIcons-Regular.ttf` | `icon::setFont(ImFont*)`（`loadFonts()` 里自动装） | **控件图标**（默认）：`icon::glyphByName("star")` / `icon::allGlyphs()` |
| `IconFontKind::SWITCH` | `bklv::getIconFont()` | `resources/font/switch_icons.ttf` | `bklv::setIconFont(ImFont*)`（`loadFonts()` 里自动装） | **按键 / 系统图标**：提示栏、菜单、选择器箭头 |

两套都是按**整个私有区**（`U+E000~U+F8FF`）装进图集的，所以字体里的字形远多于手写表：

```cpp
bklv::scanFontCodepoints(font)       // 扫一个字体在私有区里实际有的码位（升序）
bklv::getIconFontCodepoints()        // switch_icons 的全部可用码位（已裁掉占位字形）
bklv::icon::allGlyphs()              // Material 的全部码位 + UTF-8
bklv::icon::utf8Glyph(0xE5CB)        // 码位 -> UTF-8 字形（**取字形就用它**）
```

判断"字体里到底有没有这个字形"用 `ImFont::IsGlyphInFont(codepoint)`（走 stbtt，不是查范围）。
注意 `ImFont::FindGlyphNoFallback` 在 ImGui 1.92 里搬到了 `ImFontBaked` 上。

## 2. 铁律：**码位必须和字体配套**

同一个码位在 A 字体里是箭头，在 B 字体里可能是笑脸 —— 查表才知道。实测（`switch_icons.ttf` / `MaterialIcons-Regular.ttf` 的 cmap）：

| 码位 | switch_icons | Material | 说明 |
|---|---|---|---|
| `U+E0A4` / `U+E0A5` | **有** | **没有** | Switch 的 L / R 按键图标（带字母的圆角方块） |
| `U+E0ED` / `U+E0EE` | 有 | 有，**但是别的图标**（笑脸、表格） | Switch 方向箭头；在 Material 里是无关图标 |
| `U+E5CB` / `U+E5CC` | 没有 | **有** | Material 的 `chevron_left` / `chevron_right` |

⚠️ 症状对照：
- 用错字体（码位在该字体里不存在）→ 画成**豆腐块**（或空白）
- 用错字体（码位存在但是别的图标）→ 画成**笑脸、表格**之类完全不相干的图形
  （真踩过：选择器箭头走 Material，画出来是笑脸 + 表格）

## 3. 怎么指定"用哪一套"

三层，从粗到细：

```cpp
// ① 统一入口：拿两套指针里的任一个（没装好返回 nullptr，调用方自己回退正文字体）
static ImFont* AppearanceView::iconFontFor(IconFontKind kind);

// ② 整个控件一起换（默认 MATERIAL）
this->setIconFontKind(IconFontKind::SWITCH);

// ③ 按层指定（推荐：同一个控件里两套混排时用这个）
this->setLayerIconFontKind("arrowLeft", IconFontKind::SWITCH);
this->setLayerIconFontKind("arrowRight", IconFontKind::SWITCH);
this->clearLayerIconFontKind("arrowLeft");     // 撤销指定
this->hasLayerIconFontKind("arrowLeft");       // 这一层单独指定过吗
this->getLayerIconFontKind("arrowLeft");       // 没指定 = 返回 ② 那个全局 kind
```

`AppearanceView::getLayerFont(layer)` 的解析顺序（子类只要不重写它就自动吃到规则）：

1. 这一层**显式指定过** kind → `iconFontFor(指定的 kind)`
2. 否则是图标层（`layer.id == "icon"` 或 `layer.source == "icon"`）→ `iconFontFor(setIconFontKind 的全局 kind)`
3. 都不是 → 正文字体（`getDefaultFont()`）

指定/撤销都会 `markMeasureDirty()`：两套字体的字宽不一样，不重新测会让尺寸算错。

### 例子：选择器箭头（`widgets/selector_button.cpp`）

构造函数里声明两句就够了，**不需要重写 `getLayerFont`**：

```cpp
this->setAppearanceName("base/widget/button.selector");
// 箭头的码位只在 switch_icons 里；控件自己的 icon 层仍走默认的 Material
this->setLayerIconFontKind("arrowLeft", IconFontKind::SWITCH);
this->setLayerIconFontKind("arrowRight", IconFontKind::SWITCH);
```

配合 `getLayerText()`（**码位就是字符串字面量，不要包 `std::to_string`**）：

```cpp
if (source == "arrowLeft")
    return this->arrowStyle == SelectorArrow::LR ? "\uE0A4" : "\uE0ED";
if (source == "arrowRight")
    return this->arrowStyle == SelectorArrow::LR ? "\uE0A5" : "\uE0EE";
```

运行时要换风格/换字体，一句话，不用重编：

```cpp
selector.setArrowStyle(SelectorArrow::LR);                                   // 换码位
selector.setLayerIconFontKind("arrowLeft", IconFontKind::MATERIAL);          // 换字体
```

## 4. 写错的三种典型

| 写法 | 结果 | 正确写法 |
|---|---|---|
| `std::to_string("\uE0A4")` | **编不过**（`to_string` 没有 `const char*` 重载）。就算能编，意思是"把指针当整数"，会得到 `"1"` | 直接返回 `"\uE0A4"`（`\uXXXX` 本身就是字符串字面量 = 该字形的 UTF-8 字节） |
| 字写对了但没配字体 | 豆腐块 / 别的图标 | `setLayerIconFontKind(layerId, kind)` |
| 用正文字体画私有区字形 | 画成方块，而且**量出来的宽度也是错的**（私有区字形不在正文字体里） | 走 `getLayerFont()` 给的字体，别自己 `getDefaultFont()` |

## 5. 测试在哪里钉着

| 断言 | 文件 |
|---|---|
| 四个箭头码位"哪套字体里有 / 哪套没有" | `tests/icon_font_test.cpp` |
| `iconFontFor(MATERIAL/SWITCH)` 拿到对应指针 | `tests/icon_font_test.cpp` |
| 箭头层 kind 是 SWITCH、`icon` 层跟随全局、运行时可切、`clear` 后回落 | `tests/base_widget_test.cpp`（6b 节） |
| 箭头文字 = `icon::utf8Glyph(0xE0A4/0xE0A5/0xE0ED/0xE0EE)`、不是 `"1"` | `tests/base_widget_test.cpp`（6b 节） |

## 6. 还没做

- **外观 JSON 里指定字体**（比如给层写 `"iconFont": "switch"`）：现在"哪层用哪套"是 C++ 里声明的。
  要改成主题可配，需要给外观解析加一个字段 + 三套主题各写一遍。
- 第三套字体（自定义图标字体）：`setIconFont(ImFont*)` 能手动塞一套，但 `IconFontKind` 只有两档；
  要多套就得把"kind"扩成"字体表 + 索引"。

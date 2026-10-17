# 外观设计器 · 设计方案与实施细案

> 项目根 = `designer/`（见 [`README.md`](README.md)）：子目录、链接 `bklv::bklv`、资源走 `resources/` 原路径。
> 本文是设计与实施依据；实现进度在 §9 勾选，与代码同一提交更新。

---

## 1. 研究结论：这个库"是什么"，决定了设计器只能怎么长

从源码核实到的事实（不是文档推断）：

| 事实 | 依据 | 对设计器的影响 |
|---|---|---|
| 界面有两种来源：**视图描述 JSON**（节点树）与**外观 JSON**（一个控件的样式） | `compile/view_parse.cpp`、`compile/appearance_*.cpp` | 设计器必须同时编辑两种文件，但工作方式不同 |
| 视图 JSON 的属性是"按类静态表"，27 种类型 | `core/attributes.hpp`、各 `attributes()` | 检视器可以**从表里生成**，不用手写一份 UI |
| 外观由 `layers`（6 种内置 op + EXTENSION）+ `box` + `states` + `transitions` + `children` 组成 | `compile/appearance_layer_fields.cpp`、`layer_registry` | 层的候选键、参数类型、中文名、默认值都能从注册表读 |
| 布局是 **Yoga**，不是自由坐标 | `core/box.*` | **不能做拖拽摆放**；设计器是"盒模型 + 层"的编辑器 |
| 绘制值在 `applyTheme()` 时取定，绘制期不查主题 | `widgets/appearance_view.*` | 改完必须重新套用（有 `applyAppearanceJson` 可绕过缓存，见 §6） |
| 所有像素量按 1280×720 写、取值时乘 `uiScale` | `core/ui_scale.hpp`；视图 JSON 也已统一（`AttributeDef::pixels`） | 画布要能切"预览分辨率"，否则看到的是 1:1 的假象 |
| JSON 有引用层：`@theme/` `@style/` `@res/` `@i18n/` `@icon/`、同文件 `vars`（`&`）、`import`/`include` | `compile/json_vars.*`、`core/attributes.cpp` | 值可能是"引用"而不是字面量，检视器必须**显示引用 + 解析后的值**，不能一律转成 float |
| 错误策略可配：`STRICT` / `SKIP_FIELD` / `FALLBACK_DEFAULT` + 诊断表 | `compile/json_policy.*` | 设计器用 `SKIP_FIELD` 打开"改坏也能继续编辑"，并把诊断显示出来 |
| 库没有：树、分栏、表格、代码编辑器、取色器 | `bklv/views/`、`bklv/widgets/` 目录清单 | 外壳的信息密集区必须用 ImGui 或新控件，§7 有逐项判定 |
| ImGui 未启用 docking | `core/app.cpp` 无 `DockSpace` | 面板布局要自己搭（分栏器） |
| 已有 `AppearanceView::applyAppearanceJson()`、`ThemeData::getColorKeys/getNumberKeys/getWidgetNames`、`validateThemeAppearances()` | `widgets/appearance_view.hpp:113`、`core/theme_data.hpp:78-82`、`core/appearance.hpp:385` | 实时预览、令牌列表、保存前校验都不需要新造 |

**文档与源码的差异（以源码为准）**：

1. `notes/designer-requirements.md` 开头写"P1 + P2a 已实现（见 docs/designer.md）"——`docs/designer.md` 与整个 `designer/` 已删除（`3688411`），该状态已过时。
2. `AGENTS.md` 早期版本说"设计器是独立程序、与 demo 互不依赖"，现行约定是**子目录 + 链接 bklv**（`1cbb76b` 起）。

### 能力清点（A / B / C）

| 类 | 能力 | 说明 |
|---|---|---|
| **A 可直接用** | 主题装载（任意目录）、外观编译与缓存、实时套用单份外观、视图模板两段式实例化、属性表、层注册表（中文名/参数模式/候选键）、令牌键列表、全主题外观校验、JSON 错误策略与诊断、截图（GL）、`uiScale` | 全部有测试或调用方佐证 |
| **B 有基础要扩展** | ① 诊断定位到"文件 + JSON 路径"（现在 `JsonDiagnostic` 有 file/path，但**没有行列号**）② 主题热重载（`App::setTheme` 有，但"只重载一份文件"没有）③ 视图模板缓存无失效接口（改完 view JSON 要能丢掉旧缓存） | 都是小改，列入 P1/P4 |
| **C 完全缺失** | 文档模型（按键路径读写 + 保序 + 未知键保留 + 原子保存 + 撤销）／分栏器／树／检视器控件／代码编辑与高亮／取色器／资源挑选器 | 其中"文档模型"必须新写（无窗口可测）；界面件优先 ImGui |

---

## 2. 三个候选形态（差异在"主工作流"，不是面板位置）

### 方案 A：表单 + 实时预览（上一版路线）
文件树 ｜ 层树 ｜ 预览 ｜ 属性表。每一项都在自己面板里。
- 优点：实现最直接；属性表可从注册表自动生成。
- 缺点：**用户看到的东西和改的东西是两块**。要改"按钮左边那个图标"，得先在预览里认出它是哪一层，再去层树找同名 id，再去属性表找 `icon`。上一版失败的根因就在这里（不是 UI 丑）。
- 结论：不采用。

### 方案 B：文本优先的双栏（JSON IDE + 预览）
左 = JSON 文本（schema 补全 + 诊断下划线），右 = 实时预览；选中双向同步。
- 优点：数据零损耗、实现最省、对 `gradient`/`transitions`/`EXTENSION` 这些开放集天然覆盖。
- 缺点：**门槛高**。想改一个颜色得知道 `@theme/color.x` 的键名；视觉用户没有抓手。
- 结论：不作为主形态，但**保留为第一等入口**（每个对象都有"看源码"）。

### 方案 C：画布为中心 + 上下文检视器（采用）
画布（真实 bklv 渲染）是主表面：**点什么选什么**；大纲与检视器跟着选中项走，JSON 是随时可切的第三个视图。
- 优点：路径最短（看见 → 点 → 改 → 立刻看到）；符合"外观驱动"的本质；开放集有源码兜底。
- 缺点：命中测试要把"屏幕坐标 → 视图/层"映射清楚（重叠层要能循环选）；实现量最大。
- **采用理由**：本库的核心价值是"JSON 决定外观"，而人对样式的判断只能靠眼睛。把画布做成主表面，等于让 JSON 的结构直接映射到用户已经看见的东西上；同时因为 `layers` 是显式数组（有 id），"点中哪一层"是可判定的，不像自由画布那样反解不出意图。

---

## 3. 功能范围（推导，不是清单）

| 功能 | 为什么需要 | v1 | 用户操作 | 技术依赖 | 验收 |
|---|---|---|---|---|---|
| 打开主题目录 | 设计器要编辑的不只是自带主题 | ✅ | 菜单/启动参数给目录，或从 `themes.json` 选 | `ThemeData::loadFromDirectory` | 打开别的目录后画布用的是那份主题 |
| 文件树（base/widget/view/令牌） | 99 个文件，按名字找不可行 | ✅ | 左侧树，带过滤、最近打开 | `getWidgetNames` / 文件系统 | 点一下 → 画布/检视器切换，200ms 内 |
| **画布：真实渲染 + 点选** | 主工作流 | ✅ | 点控件/层 → 选中；Alt+点循环重叠层 | `ViewTemplate` 实例化 + `hitTest` + 层矩形 | 点中即选中，选中框与该层矩形吻合 |
| 检视器（由注册表生成） | 属性太多，手写 UI 必漏 | ✅ | 选中后右侧出现字段；改完立刻生效 | `AttributeDef`、`LayerParam`、`applyAppearanceJson` | 27 种视图类型的属性**无一遗漏**（用表遍历断言） |
| 层编辑（增删/排序/复制） | 外观的核心结构 | ✅ | 层列表拖拽排序、右键增删；排序按 `layerSortRank` 显示真实绘制顺序 | `compile::sortLayers` 的排序规则 | 保存后 `theme_test` 的层序断言仍通过 |
| 状态与过渡 | 交互反馈全靠它 | ✅（表单） | 状态页签；过渡先只读展示 + 「去 JSON」 | `states`/`transitions` 解析 | 加一个 focused 状态能在画布上试出来（`setState` 预览） |
| 令牌编辑（色/数值） | 改全局配色是高频 | ✅ | 令牌表：改色 + 看引用数 | `getColorKeys` + 反向引用扫描 | 改一个键，所有引用处立刻变 |
| JSON 文本视图 | 开放集的唯一出口 | ✅ | 每个对象/文件都能切"源码"；语法错误行内标红 | 现有解析器 + 诊断 | 手动写坏 → 提示行列 + 不落盘 |
| 保存（校验 + 备份 + 原子写） | 不能弄丢用户数据 | ✅ | Cmd+S；失败弹出原因并可定位 | `Appearance::compile` / `ViewTemplate::fromString` 校验先行 | 写坏的文件不会被覆盖；`.bak` 存在 |
| 撤销/重做 | 编辑必备 | ✅ | Cmd+Z / Cmd+Shift+Z；同字段连续拖拽合并 | 文件快照 + coalesce | 50 步内任意回退，保存后栈不清空 |
| 诊断面板 | 出错要知道在哪 | ✅ | 底部状态栏计数 → 点开列表 → 双击跳转 | `getJsonDiagnostics` | 每条诊断能跳到文件+路径 |
| 预览分辨率切换 | 720p 基准，必须能验 | ✅ | 状态栏切 720p/1080p/自定义 | `setUiScale` + 重排 | 切换后布局按比例变化、不错位 |
| 搜索（文件内 / 全局键名） | 找东西 | ✅（简单） | Cmd+F 过滤当前文件键 | — | 命中高亮 + 回车跳转 |
| 视图 JSON 的结构化编辑 | 页面是另一类文件 | ⚠️ 只做"选中→改属性" | 画布点中节点 → 检视器 | 视图属性表 | 能改 text/icon/src/宽高/布局属性 |
| 自由拖拽摆放 / 关键帧 / 时间线 / 蓝图 | — | ❌ | — | — | 不做：布局是 Yoga（反解不出意图）；动画是 JSON 声明式，做时间线等于写第二套解释器 |
| AI 辅助 / 协作 / 组件市场 | — | ❌ | — | — | 成本高、无数据支撑 |

**留作未来**：多文件批量操作、主题差异对比（两套主题同屏 diff）、导出组件缩略图墙、`EXTENSION` 层的参数表单（要等第三方插件注册进设计器进程）。

---

## 4. 界面与交互

### 4.1 主界面（默认 1440×900 起，最小 1024×640）

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ 工具栏： 打开主题 ▾ │ base/widget/view 面包屑 │ ⟲ ⟳ │ 校验 │ 保存 ●  │ 预览 1080p ▾ │
├──────────────┬───────────────────────────────────────────────┬───────────────┤
│ 文件树        │ 画布                                          │ 检视器        │
│ ┌──────────┐ │  ┌─────────────────────────────────────────┐  │ ┌───────────┐ │
│ │▾ base    │ │  │  ▢ 画布（真实 bklv 渲染，等比缩放居中）   │  │ │ 对象       │ │
│ │  button  │ │  │   选中项：蓝色描边 + 四角把手（只示意）   │  │ │ button.text│ │
│ │  cell    │ │  │   悬停：淡描边 + 层 id 气泡              │  │ ├───────────┤ │
│ │▾ widget  │ │  └─────────────────────────────────────────┘  │ │ 页签：属性 │ │
│ │  button…│ │  缩放 100% ▾ │ 100% 网格 ☐ │ 选中: layer.bg  │ │  层  状态  │ │
│ │▸ view    │ │                                               │ │  过渡 源码 │ │
│ └──────────┘ │                                               │ │ ┌─────────┐│ │
│ 过滤… 最近    │                                               │ │ │ fill  ◼ ││ │
│              │                                               │ │ │ radius 8││ │
├──────────────┴───────────────────────────────────────────────┴───────────────┤
│ 状态：JSON 有效 · 3 条诊断（2 警告） · 已保存 · 1280×720 · 位置 info          │
└──────────────────────────────────────────────────────────────────────────────┘
```

**为什么是这个结构**：左=去哪、中=看什么、右=改什么。三者围绕**一个选中对象**（`Selection`），不各自维护状态。

### 4.2 选中与反馈

| 状态 | 表现 |
|---|---|
| 悬停 | 该视图/层的矩形描 1px 细边 + 右上角小气泡显示 `id`（层）/类名（视图） |
| 选中 | 2px 强调色描边 + 四角短把手（**不可拖拽**，只表示"这是选中"）+ 检视器标题显示路径 |
| 重叠 | `Alt+点击` 在命中栈里循环；检视器标题旁显示 `1/3` 计数 |
| 未保存 | 工具栏「保存」变实心 + 文件名后 `●` |
| 错误 | 该字段描红 + 行内原因；状态栏计数；诊断面板可跳转 |
| 禁用（不可编辑） | 值来自 `import`/`include` 的字段：灰底 + 「来自 base/box.component」链接 |
| 预览中 | 画布加 1px 虚线外框 + 角落标「预览」（表示不是真实运行时） |

### 4.3 三条典型流程

**① 改一个按钮的圆角（外观）**
1. 文件树点 `widget/button.text` → 画布出现该外观的单件渲染（用主题里真实的值）。
2. 点画布里的底色区域 → 选中 `layer: bg`；右侧检视器自动停在「层」页签并高亮 `radius`。
3. 拖动 `radius` 数值（或直接输入）→ 松手前每帧 `applyAppearanceJson` 预览；检视器值同步。
4. 改完 → 状态栏出现「未保存」；Cmd+S → 先 `Appearance::compile` 校验；通过 → `.bak` + 原子写；工具栏「保存」变回。
5. **失败路径**：若 `radius` 被改成非法值（如文本），字段描红 + 状态栏「1 条错误」，保存被拒并定位到该字段；Cmd+Z 可退。

**② 加一个聚焦状态（状态）**
1. 选中 `widget/button.text` → 检视器「状态」页签 → `+ 新状态` 输入 `focused`。
2. 展开 `bg` 层 → `+ 通道` → 选 `fill` → 选色（取色器，支持 `@theme/` 引用选择器）。
3. 画布右上「试状态」下拉切到 `focused` → 立刻看到聚焦样式（走 `setState`）。
4. 切回 `default`；保存。

**③ 改全局强调色（令牌）**
1. 文件树进 `tokens/colors.json` → 右侧变令牌表（300+ 行，带过滤）。
2. 找 `accent.base` → 点色块 → 取色器；旁边「12 处引用」点开列表（外观名 + 层 + 键）。
3. 画布所有引用处实时变色（重载该令牌 + 重新套用当前外观）。
4. 保存：只写 `colors.json`，其它文件不动（不影响 git diff）。

### 4.4 快捷键

`Cmd/Ctrl+S` 保存 · `Cmd+Z / Cmd+Shift+Z` 撤销/重做 · `Cmd+F` 过滤当前文件 · `Cmd+P` 快速打开（文件名/键名） · `F2` 重命名 id · `Delete` 删层/删状态 · `Cmd+D` 复制层 · `Alt+点击` 循环重叠 · `Esc` 取消选中/关闭弹层 · `Tab` 在检视器字段间移动 · `空格` 试状态（预览切换）

### 4.5 空间不足

- < 1200px：文件树折叠成图标条（悬停浮出）。
- < 900px：检视器变右侧抽屉（覆盖画布，Esc 关闭）。
- 画布区域 < 400×300：自动切「适配」缩放；低于 200×150 显示「空间不足」空态而不是挤成一团。

### 4.6 上下文菜单（右键）—— 每个界面都有，不是只挂在画布上

原则：**右键 = 对"这个对象"能做的事**，左键 = 选中它。菜单项与快捷键同源（菜单里显示快捷键，
按了等效），禁用项要显示**原因**（tooltip），不能只是灰着。

| 上下文 | 右键菜单项 | 说明 / 快捷键 |
|---|---|---|
| 文件树 · 文件 | 打开 · 在文件夹中显示 · 复制名字 · 重新加载（丢弃改动，需确认） · 设为启动页 | `Enter` 打开、`Cmd+R` 重载 |
| 文件树 · 空白 | 新建外观 · 新建视图 · 新建令牌键 · 打开主题目录… | 新建先弹名字输入，落盘前校验 |
| 画布 · 选中项 | 复制为文本（JSON 片段）· 复制 id · 重命名 id（`F2`）· **复制层**（`Cmd+D`）· **删除**（`Del`）· 上移/下移（`Cmd+]`/`Cmd+[`）· 在源码中显示 · 仅看这一层 | 「仅看这一层」= 隐藏其它层的预览（排查用） |
| 画布 · 空白 | 适配缩放 · 100% · 显示网格 · 显示层边界 · 切预览分辨率 | 与状态栏缩放控件同一份状态 |
| 大纲 · 节点 | 重命名 · 复制 · 删除 · 折叠子树 · 展开全部子层 | 与画布右键同源（同一命令） |
| 检视器 · 字段 | 复制值 · 粘贴值 · 重置为默认 · 复制字段路径（`/layers/2/fill`）· 去源码 | 「重置」= 删除该键（不是写默认值），保证 JSON 最小 |
| 检视器 · 层行 | 复制层 · 删除层 · 移到最前/最后 · 切换可见（编辑期） | — |
| 源码 · 选中文本 | 剪切/复制/粘贴 · 格式化（2 空格）· 校验 · 跳转到诊断 | — |
| 令牌表 · 行 | 复制键名（`accent.base`）· 复制完整引用（`@theme/color.accent.base`）· 查看引用 · 删除键（确认） | 「查看引用」打开引用列表 |
| 状态/过渡 · 条目 | 复制通道 · 删除 · 试播（`play_arrow`） | 试播 = 在画布上真播一遍 |

**实现**：全部走 ImGui `BeginPopupContextItem` / `BeginPopupContextWindow`（自带键盘关闭、点外部关闭、
子菜单），样式从主题映射（`PopupBg`/`Border`/`Header`/`Text`）。库的 `Dropdown` 是"模态弹层"语义，
不适合右键菜单（会抢焦点、要 overlay），**不混用**。画布右键要先做一次点击命中，把选中项切过去
再弹菜单 —— 保证"菜单作用于你点的那个东西"。

### 4.7 按钮与图标规范

**图标来源只有一个**：`core/icon_font.*` 的 Material 名字表（当前 **60 个**：`add` `delete` `edit`
`save` `refresh` `search` `settings` `info` `warning` `error` `help` `close` `check` `menu` `more_vert`
`folder_open` `create_new_folder` `description` `list` `view_list` `content_copy` `undo` `redo`
`drag_indicator` `format_color_fill` `text_fields` `widgets` `layers` `layers_clear` `tune` `visibility`
`grid_on` `reorder` `image` `palette` `zooms` `play_arrow` …），或 `@icon/U+E000` 的码位写法。
图标字体的码位必须与字体配套（`iconFont: material|switch`），**不要混用两套的名字**。

**三种按钮形态**（全部走库外观，不自绘）：

| 形态 | 外观名 | 尺寸 | 用途 |
|---|---|---|---|
| 图标按钮（无文字） | `widget/button.icon` | 32×32（工具栏）/ 24×24（行内） | 工具栏的 ⟲ ⟳ 保存 校验 网格 缩放 适配 |
| 图标 + 文字 | `widget/button.icon_text` | 高度 32，`padding` 8/12 | 主操作（"保存"带图标）、空态里的引导按钮 |
| 文字按钮 | `widget/button.text` | 高度 32 | 对话框按钮（取消/保存/丢弃） |

> 固定尺寸的组合外观必须**显式写 `minWidth: 0`**（AGENTS 里那条踩过的坑：换外观时旧 `minWidth`
| 不会被清掉）。图标按钮一律 32×32，允许时以 `Tooltip` 兜底说明。

**状态**：`default` / `hover` / `pressed` / `disabled` / `selected`（可切换按钮，如"网格"开）。
选中态用 `color.accent.base` 底 + 深色字；禁用态 `opacity: 0.4` + 不响应点击 + tooltip 写原因。

**工具提示**：`图标 + 名称 + 快捷键`，例如 `保存（Cmd+S）`。悬停 400ms 出现，画在指针上方
（不出屏）；禁用时第一时间出现（用户最需要知道为什么不能点）。

**工具栏图标语义表**（固定，不随意换）：

| 动作 | 图标 | 快捷键 |
|---|---|---|
| 打开主题 | `folder_open` | `Cmd+O` |
| 保存 | `save` | `Cmd+S` |
| 撤销 / 重做 | `undo` / `redo` | `Cmd+Z` / `Cmd+Shift+Z` |
| 校验 | `check` | `Cmd+Shift+V` |
| 重新加载文件 | `refresh` | `Cmd+R` |
| 快速打开 | `search` | `Cmd+P` |
| 适配缩放 / 100% | `center_focus_strong` / `zoom_in` `zoom_out` | `Cmd+0` / `+` `-` |
| 网格 / 层边界 | `grid_on` / `layers` | `Cmd+'` / `Cmd+;` |
| 试状态（预览） | `play_arrow` | `空格` |
| 更多 | `more_vert` | — |
| 拖拽把手 | `drag_indicator` | 层/状态重排 |
| 复制 / 删除 | `content_copy` / `delete` | `Cmd+D` / `Del` |

**检视器里的"图标编辑"**（编辑目标对象的图标，不是设计器自己的按钮）：值显示为
`预览字形 + @icon/star`，点开图标选择器 —— 左侧 60 个名字（带中文名与搜索），右侧按码位浏览
（`icon::allGlyphs()` 全量，滚动虚拟化），可切 `iconFont: material|switch`，实时预览在画布上。
没选图标时显示"（无图标）"，清空 = 删除 `icon` 键。

### 4.8 弹层、确认与提示

- **对话框**（库 `Dialog` + `Button` 行）：① 有未保存改动时关闭/切文件 →「保存 / 丢弃 / 取消」三选一，
  `Esc` = 取消，焦点默认在"取消"；② 重命名 id → 单行输入 + 冲突检测（同层重名禁止，提示占用者）；
  ③ 删除层/键 → 一句话确认（"删除层 `bg`？可用 Cmd+Z 撤销"）；④ 打开主题失败 → 显示路径 + 原因
  （缺 `theme.json` / 清单里的文件缺后缀 / JSON 语法错）。
- **Toast（库 `Notification`）**：保存成功 / 校验通过 / 已回退到快照，2 秒自动消失；错误不用 toast
  （错误进状态栏与诊断面板，避免"一闪而过就找不到"）。
- **外部修改**：保存前比较 mtime，不一致弹「重新加载 / 强制覆盖 / 取消」；强制覆盖前自动备份
  `.bak.时间戳`。

### 4.9 拖放

- **层/状态重排**：拖 `drag_indicator` 把手，插入位置显示 2px 强调色线；松手写回数组顺序。
  **拖进不允许的位置要给出理由**（例如 `shadow` 层不能拖到 `bg` 之后 —— 绘制顺序由 `layerSortRank`
  决定，拖了也无效 → 直接禁止并提示"阴影层总在最底"）。
- **文件拖入**（v1 不做）：从外部拖 JSON 进入 → 未来。
- 拖拽期间：源行半透明、目标行高亮、画布不高亮（因为还没确定对象）。

### 4.10 空态 / 加载 / 错误

| 场景 | 表现 |
|---|---|
| 未打开主题 | 画布中央：图标 `folder_open` + "打开一个主题目录开始" + 打开按钮 + 最近 3 个 |
| 文件为空 | "这个文件还没有层" + 「添加第一层」下拉（6 种 op） |
| 加载中（大主题/大图） | 画布右上 24px `ProgressSpinner`，可取消 |
| 解析失败 | 画布位置显示"无法解析" + 首条错误 + 「打开源码」按钮；左侧树该文件标红 |
| 资源缺失 | 该图处显示占位色块 + 右上角 `warning` 角标，悬停显示路径 |

---

## 5. 视觉规范（与主题一致，不另立一套）

- 颜色**全部取自当前主题**：面板底 `color.common.card`、分隔线 `divider`、强调 `accent.base`、文字 `text.primary/muted`，错误 `common.red`。ImGui 面板用 `ImGui::GetStyle().Colors` 逐项映射这些键，所以换主题设计器跟着变。
- 字体：正文用主题字号档（`metric.font.size.*`）；JSON/代码用等宽；图标用 `@icon` 同一套字体。
- 圆角/间距：面板圆角 `metric.radius.lg`、内边距 `metric.spacing.md`，与控件一致。
- 密度：行高 28px（720p 基准 → 随 `uiScale`），比运行时控件更紧 —— 设计器是工具，优先信息密度。
- 层级：画布最高对比；面板次之；状态栏最弱（muted 小字）。

---

## 6. 架构

### 6.1 文档模型（无窗口、可测）

```
designer/core/
  theme_project.{hpp,cpp}      // 主题目录、文件清单、最近打开；只读枚举 + 路径解析
  json_document.{hpp,cpp}      // 基于 nlohmann::ordered_json：按 JSON Pointer 读写、保序、
                               //   未知键原样保留、原文格式化（2 空格缩进）
  appearance_doc.{hpp,cpp}     // AppearanceDocument : JsonDocument —— 层/状态/过渡的语义操作
                               //   （addLayer/removeLayer/moveLayer/duplicateLayer/setLayerField…）
  view_doc.{hpp,cpp}           // ViewDocument：children 增删、属性读写（写回的是属性表里存在的键）
  tokens_doc.{hpp,cpp}         // 令牌表读写 + 反向引用扫描（扫全主题外观的 @theme/ 键）
  save_transaction.{hpp,cpp}   // 校验（compile/instantiate）→ .bak → 原子写（临时文件 + rename）
  undo_stack.{hpp,cpp}         // 整文件快照 + coalesce(key, 500ms) + 上限 100
  selection.{hpp,cpp}          // {文件, JSON Pointer, 层 id}；三处界面共用同一份
  diagnostics.{hpp,cpp}        // 复用 compile::JsonDiagnostic + 补充行列号（解析失败时从 json 异常取）
```

**关键约束**：所有修改都是**路径补丁**（`set("/layers/2/radius", 8)`），不是"解析→重建对象→整体序列化"。这样未知键、键序、注释键（`$comment`）在保存后逐字节保留。

### 6.2 界面层

```
designer/ui/
  designer_app.{hpp,cpp}   // App 子类/装配：主题装载、诊断、快捷键、保存流程
  layout.{hpp,cpp}         // 三栏 + 分隔条（ImGui 手搭，无 docking）；尺寸记忆
  file_tree.{hpp,cpp}      // 文件树（ImGui table + selectable）
  canvas.{hpp,cpp}         // 画布：实例化真实 View / 单件外观，命中测试→Selection，缩放
  inspector.{hpp,cpp}      // 检视器：按 AttributeDef / LayerParam **生成**字段
  json_panel.{hpp,cpp}     // 源码视图：InputTextMultiline + 校验 + 行列错误标注
  tokens_panel.{hpp,cpp}   // 令牌表
  status_bar.{hpp,cpp}     // 状态栏 + 诊断抽屉
  style.{hpp,cpp}          // 主题色 → ImGui style 映射
```

### 6.3 与运行时的隔离

- 预览用 `Appearance::compile()` 得到临时 `shared_ptr`，再 `applyAppearanceJson` 换进**画布里的那个 AppearanceView**；**不写库的 appearance 缓存**（`getWidgetAppearance` 的 cache 只被运行时用）。
- 视图预览用 `ViewTemplate::fromString` 新建模板（`B` 类扩展：给库加一个按路径失效模板缓存的接口，P4）。
- 设计器内的"试状态"只在画布那个 view 实例上 `setState`，不影响全局。

---

## 7. 复用映射（路径 A / B / C 逐项判定）

| 界面元素 | 路径 | 实现 | 判定依据 |
|---|---|---|---|
| 画布（真实渲染） | **A** | `ViewTemplate` / `AppearanceView` 直接挂进画布区域，用 `ImGui::SetCursorScreenPos` + 手动落位 | 库本来就负责渲染，不再包一层 |
| 画布命中测试 | **A** | `View::hitTest` + 层矩形（`AppearanceView` 可给出层 rect） | 已有能力 |
| 工具栏按钮、状态栏、空态、提示 | **A** | 库的 `Button` / `Label` / `HBox` 组合 | 它们就是外观驱动的 |
| 文件树 / 大纲 | **B** | ImGui `Clipper` + `Selectable`（自绘缩进与拖拽） | 库无 TreeView；用 bklv 写一个带拖拽的树成本高且只服务设计器 |
| 检视器字段 | **B** | ImGui 控件，但**字段列表由 `AttributeDef`/`LayerParam` 生成**；描边/间距用主题值 | 避免手写 200 个字段又漏 |
| 分栏（三栏） | **B** | ImGui child + 自绘分隔条（拖动改宽度） | 无 docking |
| JSON 源码 | **B** | `InputTextMultiline` + 现有解析器校验 | 库无代码编辑器 |
| 取色器 / 令牌挑选 | **B** | `ColorEdit4` + 「从 `@theme` 选键」下拉 | 无取色器控件 |
| 资源挑选（`@res/`） | **B** | 文件列表 + 缩略图（用库 `Image` 加载真实图） | 库有 `Image`，但无文件浏览器 |
| 资源选择器里的图标预览 | **A** | `@icon/` 字形直接用库的图标字体画 | 已有 |
| 诊断定位 | **A** | `getJsonDiagnostics()` + 补充行列号（B） | 已有主体 |
| 右键菜单 | **B** | ImGui `BeginPopupContextItem`，样式从主题映射 | 库的 `Dropdown` 是模态弹层语义，会抢焦点，不适合右键 |
| 工具栏按钮（图标 / 图标+文字） | **A** | 库的 `widget/button.icon` / `button.icon_text`；图标用 `@icon/名字` 或 `icon::glyphByName` | 三种形态主题里都有现成外观 |
| 图标选择器 | **B**（预览用 A） | ImGui 网格 + 库的图标字体实时预览；数据来自 `icon::allGlyphs()` | 无现成选择器控件 |
| 确认对话框 | **A** | 库 `Dialog` + `Button` 行（三选一） | 已有 |
| 保存/校验提示（toast） | **A** | 库 `Notification` | 已有 |
| 保存事务 | **C** | `designer/core/save_transaction`（原子写 + `.bak` + 校验） | 库无文件写入层 |
| 撤销栈 | **C** | 自实现（快照 + coalesce） | 库无 |
| 模板缓存失效接口 | **C（改库）** | `ViewTemplate` 加 `invalidateCache(path)`；`theme_parse` 加 `reloadOneFile()` | 否则改 view JSON 后预览不更新。必要性：有；影响：只新增 API；兼容：不影响现有调用 |

**不新增 bklv 控件的理由**：树/表/代码编辑/取色是"信息密度型"UI，与库擅长的"外观驱动型"UI 不同域。给库加它们会把维护成本转嫁给运行时用户，而且第一期就会拖慢设计器。等设计器稳定后，如果 `TreeView` 的交互被验证好，再考虑下沉进库（那时有真实需求与用例）。

---

## 8. 分阶段实施与验收

| 阶段 | 内容 | 验收（可执行） |
|---|---|---|
| **P0 文档** | 本文 + `README.md` 边界 | 文档评审通过（本轮） |
| **P1 核心** | `json_document` / `appearance_doc` / `view_doc` / `tokens_doc` / `save_transaction` / `undo_stack` / `selection` + `tests/designer_doc_test.cpp` | ① 读→改一个字段→写回，**未知键与键序逐字节保留**；② 保存前校验失败则**不落盘**且 `.bak` 未产生；③ 撤销 50 步后可回到任意中间态；④ 令牌反向引用扫描与 `theme_usage` 一致 |
| **P2 外壳** | `main.cpp` + `App` + 三栏 + 文件树 + 状态栏 + 主题切换 + 诊断抽屉 + **工具栏（图标按钮）与全部右键菜单** | 能打开默认主题、切文件、显示诊断；窗口缩放不错位；`check.sh` 零警告 |
| **P3 检视器** | 属性页签（由表生成）、层列表（增删/排序/复制/拖拽）、字段右键菜单、状态与过渡表单 | 27 种视图类型遍历：每个注册属性都出现在检视器里（测试断言）；层序修改后 `theme_test` 层序断言仍过 |
| **P4 画布** | 真实渲染 + 命中测试 + 选中框 + 缩放/适配 + 试状态 + 模板缓存失效 | 点选到的对象与检视器一致；改值后 1 帧内可见；`Alt+点` 能循环 3 个重叠层；1080p 预览下位置比例正确 |
| **P5 JSON 视图** | 源码页签 + 校验 + 行列标注 + 双向（文本→模型→预览） | 写坏 JSON 有行列提示且不落盘；改好点保存，运行时（demo）能加载 |
| **P6 令牌与资源** | 令牌表 + 引用列表 + `@res/` 挑选 + **图标选择器（60 名字 + 码位全量）** | 改 `accent.base` 全画布变色；选一张图后画布出现该图 |
| **P7 打磨** | 快捷键全量、Cmd+P 快速打开、自动保存（崩溃恢复）、撤销 UI、搜索 | 3 条典型流程逐条走通并录屏/截图；异常路径（只读目录、非法 JSON、资源缺失）都有明确提示 |

每阶段都跑：`bash scripts/check.sh`（GL + VK、零警告）+ `ctest` + 手动跑 `dist/designer` 看效果；保存产物再用 `theme_test` 的校验路径复验。

---

## 9. 反向审查（三轮，结论已并入上文）

**① 产品与体验**
- 发现：检视器如果和画布选中不同步，就退化成方案 A → 定为**单一 Selection**，三处只读它。
- 发现：层列表按 JSON 书写顺序显示会让用户以为绘制顺序即书写顺序 → 层列表**按 `layerSortRank` 显示**，并标注"绘制顺序"，与 `theme_test` 的断言一致。
- 砍掉：画布上的拖拽摆放、命中后的"拖动把手改尺寸"（Yoga 下无法反解意图）。

**② 技术与架构**
- 发现：实时预览若走 `getWidgetAppearance` 会污染运行时缓存 → 改为"编译出临时对象再 `applyAppearanceJson`"。
- 发现：整体序列化会丢未知键 → 改为路径补丁 + `ordered_json`。
- 发现：改 view JSON 后 `ViewTemplate` 缓存不失效 → 列入 P4 的库改动（新增 API，不改行为）。

**③ 边界与异常**
- 只读目录 / 磁盘满 → `SaveTransaction` 先写临时文件再 rename，失败保留原文件与 `.bak`，状态栏给出原因。
- JSON 被外部改过（编辑器/另一实例）→ 保存前比较文件 mtime，变了就提示"文件已被外部修改（重新加载 / 强制覆盖）"。
- 深层嵌套（视图上限 32 层）→ 大纲显示截断标记，不递归爆栈（实例化本身有上限）。
- 资源缺失（`@res/` 指向不存在的文件）→ 画布显示库自带的占位色 + 诊断；不阻止保存。
- 撤销跨文件：一次只动一个文件，切换文件时保留各自的撤销栈（不混）。

---

## 10. 进度与遗留

- 已完成：边界与目录（`1cbb76b`）、UML/文档去设计器残留（`56ce9bc`）、JSON 侧能力补齐（错误策略、白名单、`@icon/`、图片比例/偏移、像素基准统一、容器布局属性）。
- 本轮交付：本文档（§1 研究、§2 三方案、§3 范围、§4 交互、§5 视觉、§6 架构、§7 映射、§8 阶段、§9 审查）。
- **P1 已完成**（`designer/core/*` + `tests/designer_doc_test.cpp`，55 项）：
  文档模型（路径补丁、保序、未知键保留）· 保存事务（校验先行/`.bak`/原子写/外部修改检查）·
  撤销栈（同字段合并、上限）· 选中（三处共用）· 外观层语义操作（增删/移动/复制）。
  **实施中发现的两处偏差（已按实际处理）**：
  ① 库的 `bklv::Json` 是 `nlohmann::json`（`std::map`）—— `dump()` 会把键**按字母重排**，
     而主题 JSON 是手写顺序（有语义）。设计器因此自己用 `ordered_json`（`designer::Json`），
     交给库时只传字符串。
  ② `set()` 在数组下标已存在时是**覆盖**、不是插入，所以"移动层"不能用它；
     层操作改成"取数组 → 重排 → 写回"，元素本身仍是原 Json（未知字段随之保留）。
- **P2 已完成**（`designer/main.cpp` + `designer/ui/designer_app.*`）：命令行 `--theme / --theme-name /
  --file / --screenshot / --frames`；打开任意主题目录 → 面板列出 99 个文件（可过滤、可重新扫描）→
  点开看 JSON；外壳顶部是**库的 `Header`**（标题 + 每帧更新的副标题：主题 · 文件数 · 当前文件 · 错误）。
  已实际启动并截图验证（2560×1440）。
  **偏差**：根视图只用 `Header`，没把三栏做成库的视图 —— 根 `Box` 的高度在 `App` 里是 auto，
  `Padding(grow)` 撑不开，底部状态行会浮在顶部（第一次截图抓到的重叠）。三栏留给 P4 的
  分栏器，届时用显式尺寸的容器。
- **P3 已完成（第一版）**：`designer/core/layer_fields.*`（字段由**图层注册表**派生）+
  `designer_fields_test`（31 项：6 个内置 op 的每个注册参数都出现在字段表里、未知 op 空表、
  字段写回 JSON 的 round-trip）+ 检视器 UI（层列表、按 kind 渲染数字/颜色/布尔/枚举/文本/
  inset/offset、改值即写文档并记撤销、`Cmd+S` 先 `Appearance::compile` 校验再原子保存）。
  已实机截图验证：`widget/button.text` 的 4 层与 hlBg 的字段（`&color.none` 以引用形式显示）。
  **待补**：画布实时预览（P4 才有对象可套）、`@theme/` 引用的选择器（P6）、视图节点的属性
  （P5）、层增删排序按钮（下一轮）。
- **P4 已完成（第一版）**：根视图改成 `Header / [左让位 · 画布 · 右让位]` 三栏（左右让位宽度与
  ImGui 面板同源，都乘 `uiScale`，画布由 flex 夹在中间）；画布是真实的 `AppearanceView`
  （`setGrow(1)` + `AlignSelf::STRETCH`），文件载入与**每次改字段**都走
  `applyAppearanceJson`（绕过库的外观缓存，未保存也立刻可见）；点选由
  `bklv::AppearanceView::getLayerRect()`（**本轮新增的库 API**）命中、取最上层。
  已实机截图：`widget/button.text` 的外观铺满中间画布。
  **本轮未做**：缩放/适配、网格、状态预览下拉、画布内选中框描边、视图节点的画布预览。
  **偏差**：画布与 ImGui 面板的对齐靠"左右让位 Box"，不是通用分栏器；窄窗口下需要 P7 的降级。
- **P5 已完成（第一版）**：检视器右栏加「属性 / 源码」两个页签。源码页签是可编辑的
  `InputTextMultiline` + `应用（Cmd+Enter）` + `还原`；**应用失败时文档一动不动**（只把带行列的
  错误显示出来），成功后同步层列表、检视器、画布。非外观文件暂时只做语法校验。
  `--source` 启动参数便于自动化截图。测试补第 6 节（`designer_doc_test` 61 项）：
  坏 JSON 应用失败后旧文档逐字节不变。
  **待补**：行号跳转高亮、格式化、等宽字体（P7）、视图文件的结构化编辑。
- **P6 已完成（第一半：令牌）**：`designer/core/tokens_doc.*` —— 嵌套对象展平成点分键
  （保留原样写法，"#RRGGBBAA" 不被转成数）、按点分键写回（路径补丁，键序/未知键保留）、
  `scanThemeReferences(dir, kind, key)` 扫描 base/widget 里所有 `@theme/<kind>.<key>` 引用。
  UI：打开 `tokens/colors|metrics` → 右栏变令牌表（过滤、键/值/改/引用），改值后
  **写盘 + 重载主题 + 重画布**；「引用」列出文件与 JSON Pointer。
  测试 `designer_tokens_test`（12 项）：真实 colors.json 展平 >50 条、写/删走路径补丁、
  真实主题里 `accent.base` 至少一处引用、不存在的键空列表。
  **待补（P6 第二半）**：`@res/` 文件挑选与缩略图、`@icon/` 图标选择器 —— 它们作用于
  **视图文件**的字段，等视图结构化编辑（P5 之后的后续）接上再一起做，避免只做半截 UI。
- **P7 第一切片已完成**：层操作行（＋加层下拉 / 复制 / 删 / 上移 / 下移）、快捷键
  `Cmd+D` 复制、`Del` 删、`F2` 重命名（带 id 冲突检查）、`Cmd+Z` / `Cmd+Shift+Z` 撤销重做；
  未保存切换文件时的「保存并切换 / 丢弃 / 取消」三选一。
  **架构修正**：快捷键一开始直接调了 `drawLayerOps()` —— 那是界面函数，在 `update()` 里调
  等于凭空渲染控件；改成「操作本体（`addLayerOp`/`duplicateSelected`/`deleteSelected`/
  `moveSelected`/`renameSelected`）+ 界面只是按钮」两层，快捷键只调本体。
  重命名弹窗也从 `update()` 挪进检视器（弹窗需要窗口上下文）。
- **仍未做**（按价值排序）：视图文件的结构化编辑与画布预览（含 `@res/`、`@icon/` 挑选器）、
  快速打开（Cmd+P）、画布缩放/网格/状态预览、源码页签的等宽字体与行号跳转、
  自动保存与崩溃恢复、诊断面板列表化（现在只有副标题一行）。
- **视图文件（预览 + 节点树）已完成**：`designer/core/view_doc.*`（深度优先列节点、路径补丁
  读写属性、真实文件可装载）+ `tests/designer_view_test.cpp`（16 项）+ UI：打开 `view/*.json`
  即用 `ViewTemplate` 实例化到画布（与外观预览互斥、`removeView` 换新），右栏列出节点树
  （缩进 + 指针 + type），点选同步；源码页签应用成功后重建预览。
  已实机截图：`view/page_json` 真实渲染出卡片（头像/标题/副标题/徽章）+ 节点树。
  **待补**：节点属性的表单（`ViewDoc::setAttribute` 已有并有测试，UI 尚未接）、
  `@res/` 与 `@icon/` 挑选器、画布点选节点（现在只能从节点树选）。
- **视图节点属性编辑器已完成**：`designer/core/view_fields.*` 从控件自己的属性表派生字段
  （实例化一个只有 type 的最小节点，读 `attributes()` + 新公开的 `commonAttributesTable()`）；
  右栏节点树下方按字段编辑（BOOL 勾选 / ENUM 下拉 / 其余文本回车提交，数字与布尔按字面量写、
  `"auto"`/`"50%"`/`@res/` 原样保留），`icon` 键附一排常用图标按钮（写 `@icon/名字`）；
  改完重建预览。`tests/designer_view_fields_test`（7 项）：24 种类型都有字段且都含 `width`、
  Image 有 `src`、Button 有 `text`/`icon`。
  **待打磨**：字段标签在 `SetNextItemWidth(-1)` 下被挤掉（截图可见，只显示组合框）；
  `@res/` 文件挑选器；画布点选节点。
- 状态：**P1–P6 完成，P7 完成第一切片；视图预览 + 节点属性编辑完成**。目标保持 active。
- 明确不做：自由拖拽布局、关键帧/时间线、AI 辅助、协作、组件市场（理由见 §3）。

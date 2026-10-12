# 外观设计器（Designer）需求梳理

状态：**需求确认中**（P1 + P2 已定，实现未开始）

## 1. 目标

做一个**独立的桌面程序**，读取并编辑主题的外观 JSON，边改边看效果，最后写回文件。
P1 只读浏览，P2 表单编辑 + 实时预览 + 保存。

**非目标**（明确不做，避免预期错位）：

- **不做画布拖拽**。布局是 Yoga 算的（`grow` / `shrink` / 百分比 / padding / gap），
  反解不出"拖到这里该写什么 JSON"。设计器是**表单 + 实时预览**，不是所见即所得拖拽。
- 不编辑视图描述 JSON（`resources/views/*.json`）——那是搭界面结构，属于另一个工具。
- 不编辑文案 JSON（`resources/i18n/*.json`）。
- 颜色表 / 数值表的可视化编辑（P4）本轮不做，但表单里要能**选**现有主题 key。
- Switch 上不跑（桌面工具；构建脚本不参与）。

## 2. 形态：与 demo 分开的两个程序

```
designer/                    设计器（独立可执行，不依赖 demo/）
  main.cpp                   入口：建 App、装项目、开窗口
  project.*                  项目模型：新建（模板）/ 打开（直接编辑）/ 保存 / 备份
  theme_files.*              主题文件读写：theme.json / themes.json / widgets/*.json
  editor_appearance.*        P2：外观编辑（box / layers / states 的表单）
  editor_browser.*           P1：主题 → 控件 → 层/状态 的浏览树 + 高亮
  ui_helpers.*               表单控件小工具（数值/颜色/枚举下拉，枚举清单来自库里的表）
  project.json               当前项目元数据（见 §3）
```

- 构建：根 `CMakeLists.txt` 的 `BKLV_TARGET_LIST` 加一行 `"designer|designer/main.cpp|core"`，
  源码加进 `BKLV_APP_SOURCES`，并把 `designer/` 放进允许目录（`BKLV_APP_ALLOWED`）。
- 输出：**`dist/designer/designer`**（自己一个目录，不跟 demo 抢 `dist/`；
  AGENTS 的"产物分目录"约定照旧）。
- GL / Vulkan 两个后端都要能编（AGENTS 的硬约定）。
- 两个程序共用 `bklv::bklv`，**不共用** demo/app 里的代码（页面、ui 小工具都在 demo 侧）。

## 3. 项目模型（这是本轮需求的核心）

设计器操作的对象叫**项目**。创建项目时二选一：

| 模式 | 行为 | 产物 |
|---|---|---|
| **① 模板新建** | 选一个已有主题当模板（`default` / `ocean` / 任意 `themes.json` 里的），起个新主题名 → 复制整个主题目录到 `resources/theme/<新名>/`，并把名字加进 `themes.json` 的 `available` | 全新主题，原主题**只读不动** |
| **② 直接编辑** | 选一个已有主题目录 → 之后所有编辑**直接写回该主题的文件** | 就地修改（每次保存前自动备份） |

两种模式在编辑阶段的行为完全一样，区别只在**写到哪里**、以及要不要动 `themes.json`。

项目元数据（`designer/project.json`，程序自己用，不进主题目录）：

```json
{
  "mode": "fork",                 // fork | inplace
  "theme": "my-skin",             // 当前编辑的主题名
  "template": "default",          // fork 时记来源；inplace 时为 null
  "recent": ["my-skin", "ocean"]  // 最近打开（可选）
}
```

**单项目**：一次只打开一个项目，元数据一份；不做多项目工作区（要的话再加）。

## 4. 功能范围

### P1：只读浏览

- 左侧树：`主题 → 控件外观列表（来自 theme.json 的 widgets[]）→ 该外观的 box / 每一层(id, op) / 每个状态`
- 点任一节点 → 中间面板显示它的**原始 JSON 片段**（格式化只读）+ 编译后的解析值
- **高亮联动**：选中某个控件外观时，把界面上对应的控件用一圈醒目标记框出来
  （复用调试层那套思路；控件可能不在当前页，就给"这一页没有该控件"的提示）
- 顶部显示：当前主题、外观文件路径、编译状态（成功 / 失败 + 错误全文）

### P2：编辑 + 预览 + 保存

- **box 表单**：`width/height/minWidth/minHeight`（数字 / auto / %）、`padding/margin`（单值↔四边）、
  `gap/grow/shrink`、`axis`、`justifyContent`、`alignItems`、`backgroundColor/borderColor`、
  `cornerRadius/borderThickness`。数字与颜色都能**选主题 key**（下拉里列 `colors.json` / `metrics.json` 的键）
- **layers 表单**：增 / 删 / 上下移动 / 复制一层；按 `op` 显示可用字段
  （rect：fill/stroke/strokeWidth/radius；text：source/color/fontSize/align/valign；
  image：source/scaling/radius；shadow：fill/feather/radius/offset/spread/cutout；custom：只读）
  共用字段：`id`、`inset`、`offset`、`width/height`、`opacity`
- **states 表单**：增删状态与通道（`<层id>.<属性名>`），编辑 `to` / `duration` / `easing`（下拉）
- **实时预览**：改动 → 重新编译这份外观 → 成功就替换 + 重排，界面**立刻**变；失败就不应用并显示错误
- **保存**：写回该项目的主题文件（fork 写新主题，inplace 写原主题），
  写盘前**先跑一遍编译校验**（不合法宁可不写），并自动备份 `<文件>.bak`
- **还原 / 另存**：把当前外观恢复成上次保存的内容；"另存为"导出一份 JSON 到指定路径

## 5. 数据流

```
读：ThemeData::widgetAppearanceJson(name)（原文）        ← 已有
  ↓ 解析成编辑器内存模型（nlohmann::ordered_json，保键序）
编辑：表单改动 → 内存模型
  ↓ 序列化（统一缩进）
校验：Appearance::compile(text, theme, error)             ← 已有，错误信息带路径
  ↓ 通过才应用
预览：替换该控件的外观 + applyThemeRecursive() + requestLayout()   ← 已有机制
  ↓ 用户点保存
写盘：临时文件 → fsync → rename（原子替换）+ 先备份 .bak
  ↓
热重载：loadThemeIntoGlobal(theme)（每次读盘 ✓）+ 清外观缓存 + 重排
```

## 6. 库侧要补的东西（都很小，保持设计器薄）

1. `core/appearance.*`：**清外观缓存** `clearAppearanceCache()`（热重载必须；
   否则改完还是旧的，因为缓存 key 是 `主题/控件`）
2. `core/theme_data.*`：`reloadThemeFromDisk(name)`（`loadThemeIntoGlobal` 已经每次读盘，
   这里只是包一层语义 + 重置 `themeLoaded` 状态）
3. `core/`：给设计器用的**枚举取值清单**导出（`AttributeDef.options` 已有字符串，
   再加一层"列出某属性可选值"的访问器，编辑器下拉直接用；避免编辑器里再抄一份清单）
4. 主题文件的枚举/清单读取：`themes.json` 的 `available`、某主题 `theme.json` 的 `widgets[]`、
   `colors.json`/`metrics.json` 的键列表（`ThemeData` 现在能查值，但列不出全部键 → 加两个只读访问器）

## 7. 验收标准

- [ ] `cmake --build` 出 `dist/designer/designer`（GL 与 Vulkan 各一份都能跑）
- [ ] 启动后能**模板新建**一个主题：新目录出现在 `resources/theme/` 下，`themes.json` 已登记，原主题文件字节不变
- [ ] 能**直接编辑**一个主题，改 `box.height` 后界面立即变化
- [ ] 非法输入（拼错的枚举、不存在的 `@theme` key）**不写盘**，界面上显示编译错误（带 `layers[2].fill` 这类路径）
- [ ] 保存后文件是合法 JSON、键序稳定、有 `.bak` 备份
- [ ] 保存后热重载生效（不重启程序，改动仍在界面上）
- [ ] P1 的树能列出主题→控件→层/状态，选中时界面上对应控件被标出
- [ ] 两个程序互不影响：designer 不引用 demo 的代码，demo 不依赖 designer

## 8. 风险与取舍

| 风险 | 处理 |
|---|---|
| 写回会丢注释与手写排版 | 统一重排缩进（`ordered_json` 保键序）；有 `.bak` + git 可回滚；另存路径可选 |
| 编辑器改坏主题导致 demo 打不开 | 写盘前必须过编译校验；非法就只显示错误不写 |
| 热重载后旧外观残留 | 必须清外观缓存（§6 第 1 条），否则"改了没反应" |
| 设计器自身崩了影响主题文件 | 只在"保存"那一步碰文件，且原子替换 + 备份 |
| 两个目标共用 `dist/resources` 会重复复制 | 设计器给独立输出目录 `dist/designer/`，各自复制一份 |

## 9. 待确认

1. **输出目录**叫 `dist/designer/designer` 行不行（还是你想放到别处 / 换名字）？
2. **P1 的"高亮对应控件"**：只在当前页找（找不到就提示），还是允许设计器切换 demo 的页面去找？
   —— 设计器是独立程序，没有 demo 的页面；所以实际是"把选中的外观**临时套到设计器自己的预览区**"更现实。
   要不要就用一个**预览区**（放常见控件：按钮/卡片/单元格/滑块/弹窗）来承担实时预览？
3. 项目元数据就放 `designer/project.json`（单项目）可以吗？要不要多项目？
4. 界面语言用现有 i18n（`resources/i18n/`，中英各一份）还是先只写中文？

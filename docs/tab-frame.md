# TabFrame：统一的 Tab 容器（横向 / 纵向）

> **2026-10 控件清理**：`bklv/widgets` 现在只剩 `AppearanceView` / `Button` / `Notification`
> + 六个 `layer_draw_*`。本文提到的 `Shell` / `Header` / `BottomBar` / `TabBar` / `Sidebar` /
> `TabFrame` / `Cell` 家族 / `Dialog` / `Dropdown` / `Slider` / `HintsBar` / `StatusArea`
> 以及它们在 `resources/theme/default/base/**` 下的外观 JSON 都已删除 —— 这些段落作为
> "重做时的约定"保留，别当成现成 API。

> 状态：**规格已定，实现分 P1–P5**。本文是实现的唯一口径，改行为先改这里。

## 1. 目标

把现在的三个东西合并成一个：

| 现状 | 合并后 |
|---|---|
| `TabFrame`：横向 tab + 内容框（栏外观无） | **`TabFrame`**：一个组件覆盖两种方向 |
| `TabBar`：只有横向，无内容框 | 由 `TabFrame` 内部承担（栏 + 内容框） |
| `Sidebar`：只有纵向，"焦点即切"，外观走 `applyWidgetBox` | 由 `TabFrame(orientation=VERTICAL, trigger=FOCUS)` 表达 |

删除：`tab_bar.{hpp,cpp}`、`sidebar.{hpp,cpp}`、`base/tab_cell*.json`、
`base/widget/tab_bar.json`、`base/widget/sidebar.json`。

## 2. 方向与位置

```cpp
enum class TabOrientation { HORIZONTAL, VERTICAL };
enum class TabBarPlacement { START, END };
```

| 方向 | 默认（START） | 交换（END） |
|---|---|---|
| HORIZONTAL | 栏在**顶部** | 底部 |
| VERTICAL | 栏在**左侧** | 右侧 |

布局轴不随位置变，只换"栏 / 内容框"的先后顺序：

```
HORIZONTAL + START:  COLUMN[ 栏 , 内容(grow) ]
HORIZONTAL + END:    COLUMN[ 内容(grow) , 栏 ]
VERTICAL   + START:  ROW   [ 栏(固定宽) , 内容(grow) ]
VERTICAL   + END:    ROW   [ 内容(grow) , 栏(固定宽) ]
```

## 3. 触发模式与焦点规则（**用户拍板**）

```cpp
enum class TabTriggerMode { CLICK, FOCUS };
```

| 模式 | 切页时机 | 切完焦点 |
|---|---|---|
| `CLICK` | 点击 / A **才切页** | **仍留在 tab 上**（方向键可继续浏览 tab） |
| `FOCUS` | **焦点移到哪个 tab 就切哪个** | 点击（或 A）后**焦点进入子页面的默认焦点** |

两条推论，实现时要保证：

1. `CLICK` 模式下，方向键只是在 tab 之间移动焦点，**不切页**；切页只由点击 / A 触发。
2. `FOCUS` 模式下，选择由 `onFocusGained` 驱动；鼠标点击 / A 额外把焦点交给内容区
   的默认控件（内容区没有可聚焦项时焦点留在 tab 上，不报错）。

## 4. 视觉：两套按钮 + 一份内容框

**目录约定（用户追加要求）**：外观目录按**控件分类**组织，不按用途组织 ——

```
base/button/tab_button.<方向>.json   按钮（TabButton 是按钮）
base/frame/tab_bar.<方向>.json       栏容器
base/frame/tab_content.json          页面框
```

理由：同一个按钮外观会被多个容器复用（tab / 分段控件 / 工具栏），把它放进容器目录等于按用途复制；
改动也要能隔离（按钮样式只动 `button/`，栏布局只动 `frame/`）。当前只做 `default` 主题，
`ios` 缺项时按"当前主题 → default → 空外观"回退。名字 = 相对主题根的路径去掉扩展名。
**新增文件必须登记进 `theme.json` 的 `files[]`**（漏了会静默回退成空外观）。

| 文件 | 用途 |
|---|---|
| `base/tab_button.horizontal.json` | 横向按钮：图标在上、文字在下，等分宽度，选中态 `accent` 底部指示条 |
| `base/tab_button.vertical.json` | 纵向按钮：图标在左、文字在右，固定行高，选中态 `accent` 左侧竖条 + 底色 |
| `base/widget/tab_bar.horizontal.json` | 横向栏容器（高度、内边距、背景/玻璃） |
| `base/widget/tab_bar.vertical.json` | 纵向栏容器（宽度、内边距、圆角/边框、分组线） |
| `base/widget/tab_content.json` | **子页面框：横向纵向共用同一份** |

控件侧只有**一个** `TabButton` 类，按方向 `setAppearanceVariant("horizontal" | "vertical")`；
样式差异全部写在 JSON 里（沿用"样式归 JSON、行为归 C++"的约定）。

## 4b. 三个名字

| 名字 | 是什么 |
|---|---|
| `TabFrame` | 聚合体：栏 + 内容框 + 按钮，管方向/位置/触发/切页 |
| `TabItem` | 视图 JSON 里的描述节点（`title`/`icon` + 子节点=页面），建树后即删 |
| `TabButton` | 真正的按钮控件（原 `TabCell`），外观 `base/button/tab_button.<方向>` |

## 5. C++ API

```cpp
class TabFrame : public Box {
  public:
    TabFrame(TabOrientation orientation = TabOrientation::HORIZONTAL,
             TabTriggerMode trigger = TabTriggerMode::CLICK);

    void setOrientation(TabOrientation);
    void setTriggerMode(TabTriggerMode);
    void setBarPlacement(TabBarPlacement);   // START 默认

    size_t addTab(const std::string& title, View* content);
    size_t addTab(const std::string& title, const std::string& icon, View* content);
    void addSeparator();                     // 纵向分组线；横向忽略（打日志）
    void setTitle(const std::string&);       // 栏标题（纵向侧栏常见）

    void setSelectedIndex(size_t);
    size_t getSelectedIndex() const;
    void setTabTitle(size_t, const std::string&);
    void setTabIcon(size_t, const std::string&);

    Box* getBarBox();       // 栏容器（内部按轴用滚动器）
    Box* getContentBox();   // 内容框（共用 tab_content）
};
```

## 6. 聚合体，不进视图 JSON（用户拍板）

`TabFrame` 是**组装体**，不是单体控件，所以**不注册** `registerViewType`、也不由一份 JSON 描述。
它由四个部件组成，样式各自独立：

| 部件 | 外观 | 可替换入口 |
|---|---|---|
| 栏容器（横向） | `base/frame/tab_bar.horizontal.json` | `setBarAppearance(name)` |
| 栏容器（纵向） | `base/frame/tab_bar.vertical.json` | 同上（按方向取变体） |
| 内容框 | `base/frame/tab_content.json` | `setContentAppearance(name)` |
| tab 按钮（横向/纵向） | `base/button/tab_button.<方向>.json` | `setTabButtonAppearance(name)` |

按钮还支持**实例级自定义**：`addTab(TabButton* button, View* content)` 接管调用方传入的
`TabButton`（或其子类），宿主不要再 delete。

这样分工的理由：TabFrame 只负责**组装与行为**（方向、位置、触发、焦点、切页）；
"长什么样"归四个部件的 JSON —— 换栏样式不影响按钮，换按钮样式不影响内容框。

## 7. 测试与验证

- `tests/tab_frame_test.cpp`（替换 `tab_bar_test.cpp`）：
  1. 两种方向 + 两种位置：栏与内容框的先后兄弟顺序正确；
  2. `CLICK`：焦点在 tab 间移动**不**切页；点击 / A 切页且焦点仍在 tab；
  3. `FOCUS`：焦点移动即切页；点击后焦点落到内容区第一个可聚焦控件；
  4. 选中不触发重排（布局计数不增长）；
  5. `addSeparator` 在纵向才有条目、横向忽略。
- `theme_test`：新外观的层序断言自动覆盖；
- 实机：demo 加一个双方向示例（横向顶部 + 纵向左侧）各截一张图。

## 8. 阶段

| 阶段 | 内容 | 完成判据 |
|---|---|---|
| P1 | `TabButton` → `TabButton`（加图标层、双外观）+ 两份按钮 JSON | `check.sh` 绿；外观能编译 |
| P2 | 重写 `TabFrame`（方向/位置/触发/栏+内容框） | 新测试全过 |
| P3 | 删除 `TabBar`/`Sidebar`，迁移 demo 与 `widget_test` | demo 启动、无残留引用 |
| P4 | JSON 注册 + 属性表 + 样例 `view/tab_frame_demo.json` | 视图 JSON 能建出两种方向 |
| P5 | 文档（`docs/json-syntax.md` 的 TabFrame 一节）、旧注释清理、截图 | 全量门禁 + 截图 |

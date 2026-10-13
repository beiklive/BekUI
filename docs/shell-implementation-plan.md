# bklv 外壳重构 · 完整实施方案（可执行版）

> 定稿：用户确认"都按你的建议来"。设计讨论见 `docs/layers-and-focus.md`，
> 方案总览见 `docs/shell-refactor-plan.md`，**本文件是实现落地清单**。

## 0. 范围、不变量与工作纪律

**范围**：`bklv/`（库）新增外壳与焦点管理；`demo/`（14 页）迁移到新外壳；`tests/` 补测试与迁移。

**不变量（每步都必须成立）**：
1. `./scripts/check.sh` 全绿（当前基线 **GL+VK 1832 项、零警告**），不得回归既有测试
2. 改代码用**精确锚点 + 单处替换 + 立刻构建 + 截图复核**；删代码块用**括号配对器**
   （同时数 `()` 与 `{}`）。正则删块 / 找 `});` / 宽泛字面量替换这三种方式本项目各翻车过一次
3. 改坏立即 `git checkout` 回退，**不把坏代码留在工作区**
4. 样式能在 JSON 里表达的，就不要只留 C++ 接口（本项目约定：控件样式全在 JSON）

## 1. 接口风格约定（照 `AGENTS.md` / borealis）

| 类别 | 规则 | 例 |
|---|---|---|
| 类型 | PascalCase，无前缀 | `Shell` / `FocusManager` / `Pager` |
| 枚举值 | UPPER_SNAKE | `PositionType::ABSOLUTE`、`CrossBoxPolicy::SAME_PARENT_ONLY` |
| 方法 | camelCase，动词开头 | `setPositionType` / `addToCorner` / `enterLayer` |
| 状态查询 | `is*` / `has*` / `can*` | `isHeaderVisible()` / `canFocus()` |
| 取值 | `get*` | `getContentLayer()` / `getCurrent()` |
| 开关 | `set<名词>Enabled/Visible/Animated` | `setHeaderVisible` / `setBackdropVisible` |
| 回调 | `setOnXxx` | `setOnFocusLost` |
| 文件 | snake_case，一控件一对 `.hpp/.cpp` | `shell.hpp` / `focus_manager.hpp` / `pager.hpp` |

## 2. 现有可复用原语（已核实，带行号）

| 能力 | 位置 |
|---|---|
| 定位方式 `PositionType{RELATIVE,ABSOLUTE}` + `setPositionType` | `core/view.hpp:93,167`（注释写明"对应 borealis 的 PositionType"） |
| 四边偏移 + 百分比：`setPositionTop/Right/Bottom/Left`、`...Percentage` | `core/view.hpp:159-166` |
| 交叉轴自对齐 `AlignSelf{AUTO,FLEX_START,CENTER,FLEX_END,STRETCH,BASELINE}` + `setAlignSelf` | `core/view.hpp:82,170` |
| 外边距 `setMargins(t,r,b,l)` | `core/view.hpp:147` |
| 裁剪 `setClipsToBounds` | `core/view.hpp:204` |
| **绝对坐标** `getFrame()` | `core/view.cpp:413`（= getX/getY/w/h） |
| 模态弹层 `App::pushOverlay(view, closeOnBack)` / `popOverlay(view, free)` | `core/app.hpp:99-100` |
| 默认焦点扩展点 `getDefaultFocus()` | 子类扩展点（AGENTS 约定 + 控件已用） |
| 可聚焦查询 `isFocusable()` | `core/view.hpp:259` |
| 转场期间不挪焦点（既有约定） | `widgets/applet_frame.hpp:62` |
| `setContentView` **只摘不删** | `views/scrolling_frame.cpp:71-74`（注释写明所有权在调用方） |
| JSON 属性表已支持 `alignSelf` | `core/view.cpp:1044` |
| JSON 属性表**未支持** `positionType` / 四边偏移 | 同上（只有函数，无表项）→ 本方案补 |

## 3. 要新增的能力（清单）

| # | 能力 | 归属 | 说明 |
|---|---|---|---|
| 1 | `Shell` 四层 + insets 查询 + `pushDialog` + `addToCorner/addToEdge` | `bklv/widgets/shell.*` | 取代旧 `AppletFrame` |
| 2 | `positionType` / `positionTop/Right/Bottom/Left`(+%) 进 JSON 属性表 | `core/view.cpp` 属性表 | 让布局能在 JSON 表达 |
| 3 | `margin: auto`（`setMarginAuto(Edge)` + JSON `"marginLeft": "auto"`）| `core/view.*` | 流式布局里把单个子件推到边（成本小，建议纳入） |
| 4 | `FocusManager`：默认焦点 / 栈式记忆 / 补救 / 几何最近邻导航 / 跨 box 三档 / 模态约束 | `bklv/core/focus_manager.*` | 本次重点 |
| 5 | 动作路由：B 键优先级 弹窗 → 内容 → 底栏/页头 → shell 默认 | `Shell` + `core/input.cpp` | 分层后父链冒泡失效 |
| 6 | `Pager`：懒加载 + 预加载（`IPage::preload/build/release`） | `bklv/widgets/pager.*` | 页面不留常驻 |
| 7 | `FlyingItem`：跨层幽灵动画（不真移动视图） | `bklv/widgets/flying_item.*` | 共享元素转场 |
| 8 | 静态背景图 | `Shell` 的 `BackdropLayer` + 现成 `Image` | 视频后续排期 |

### 接口草案（按第 1 节风格）

```cpp
// ---- Shell ----
enum class Corner { TOP_LEFT, TOP_CENTER, TOP_RIGHT,
                    CENTER_LEFT, CENTER, CENTER_RIGHT,
                    BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT };
enum class Edge { TOP, RIGHT, BOTTOM, LEFT };

class Shell : public View {
public:
    Shell();
    Box* getBackdropLayer();  Box* getContentLayer();
    Box* getOverlayLayer();   Box* getHudLayer();

    // 顶层件（页头/底栏）放覆盖层
    void setHeaderVisible(bool visible);
    void setFooterVisible(bool visible);
    bool isHeaderVisible() const;
    bool isFooterVisible() const;
    // 页面显式查询留白（内容避让不自动）
    EdgeInsets getHeaderInsets() const;
    EdgeInsets getFooterInsets() const;

    // 角落/边缘定位（内部 = PositionType::ABSOLUTE + 对应边偏移）
    void addToCorner(View* view, Corner corner, float insetX, float insetY);
    void addToEdge(View* view, Edge edge, float inset);

    // 弹窗挂覆盖层顶部（模态标记），系统级模态仍走 App::pushOverlay
    void pushDialog(View* dialog, bool closeOnBack = true);
    void popDialog(View* dialog);

    void setOnBackRequested(std::function<bool()> handler);  // 返回 true 表示已处理
};

// ---- 焦点 ----
enum class FocusDirection { UP, DOWN, LEFT, RIGHT };
enum class CrossBoxPolicy { ALLOW, SAME_PARENT_ONLY, NONE };

struct FocusRecord {                       // 存"路径 + hint"，不存裸 View*
    int          layer;                    // 0=内容层 1=覆盖层
    std::string  pageId;
    std::string  viewPath;
    int          hint = -1;                // 上次列表下标等，交给 getDefaultFocus(hint)
};

class FocusManager {
public:
    View* getCurrent() const;                       // 唯一真相（焦点框只读它）
    void  setCurrent(View* view, FocusReason reason);

    void  enterLayer(Box* layerRoot);               // remember() → 取该层默认焦点
    bool  leaveLayer();                             // restore() → 失败退本层默认焦点
    void  remember();                               // 入栈
    bool  restore();                                // 出栈：校验存在性 + hint

    bool  move(FocusDirection dir);                 // 几何最近邻 + 冒泡 + 出口策略
    void  setCrossBoxPolicy(View* box, CrossBoxPolicy policy);  // 局部生效，内层可覆盖
    bool  canFocus(View* view) const;               // 四条件（见第 4 步）
};

// ---- 页面 ----
struct PageContext { /* GameRepo& repo; ImageCache& images; AppSettings& settings; */ };
class IPage {
public:
    virtual ~IPage() = default;
    virtual void  preload(PageContext& ctx) = 0;
    virtual View* build(PageContext& ctx)   = 0;
    virtual void  release()                 = 0;
    virtual bool  isReady() const           = 0;
};

class Pager {
public:
    void  setSlot(ScrollingFrame* slot);
    void  registerPage(const std::string& id, IPage* page);
    void  preload(const std::string& id);           // 只预热数据
    void  show(const std::string& id);              // 即时构建 + 焦点交接 + 删旧页
    View* getCurrentView() const;
};
```

## 4. 分步实施（每步都可独立验收）

### 第 1 步 · 布局原语补齐（先做，因为后面都依赖它）
- 改动：`core/view.cpp` 属性表加 `positionType`（`relative|absolute`）与
  `positionTop/Right/Bottom/Left`（数值或 `"50%"`）；`View::setMarginAuto(Edge)` + JSON `"auto"`
- 测试：`tests/layout_test`（或并入 `box_test`）：绝对定位不参与 flex；四边偏移生效；
  百分比偏移；`alignSelf` 覆盖父 `alignItems`；`margin auto` 把子件推到边
- 验收：`check.sh` 全绿 + 截图确认角落定位（页头左图标/中标题/右按钮）

### 第 2 步 · `Shell` 四层骨架
- 新增 `bklv/widgets/shell.*`；四层用绝对定位铺满视口；`insets` 查询；层跟随画布尺寸
- `addToCorner/addToEdge` 语义封装
- 测试：层尺寸随视口；页头/底栏显隐后 `getHeaderInsets()` 正确变化；
  绝对层不触发布局（用现有 `App::getLayoutCount()` 断言静止时 0 次）
- 验收：demo 里临时挂一个 Shell 显示四层颜色/背景图，截图

### 第 3 步 · `FocusManager`（本次重点）
- 新增 `bklv/core/focus_manager.*`；`canFocus` 四条件
  （`isFocusable && !isDisabled && isVisibleInTree && isReachableFromModal`）
- 跨层：`enterLayer/leaveLayer` 时序（**显示完成后**取默认焦点；**开始关闭**就恢复；
  恢复失败退本层默认；本层无默认焦点则保持焦点、输入归模态）
- 层内：几何最近邻导航 + 同 Box 冒泡 + `axis` 定主轴 + 跨 box 三档
- 测试（6 条）：记住/恢复；原控件销毁退默认；覆盖层无默认焦点时保持；
  模态下方向键不进页头底栏；跨 box 三档；弹窗套弹窗逐级还原
- 验收：6 条测试全绿

### 第 4 步 · 动作路由 + 模态标记 + `pushDialog`
- B 键优先级：弹窗 → 内容页 → 底栏/页头 → shell 默认
- `pushDialog()` 挂覆盖层顶部并打模态标记；`App::pushOverlay` 保留给系统级
- 测试：模态时方向键/确定键只作用于弹窗；关闭后焦点按第 3 步规则恢复

### 第 5 步 · `Pager` 懒加载 + 迁移 demo
- 新增 `bklv/widgets/pager.*`；切页顺序：**先挂新页 → 给焦点 → 再删旧页**
- 迁移 demo 14 页到 `IPage` + `Shell`（`demo/app/demo_app.*`、`pages/page_settings`、
  `pages/page_pokedex`、`pages.hpp`、`main.cpp`）
- 迁移 `tests/widget_test.cpp` / `tests/transition_test.cpp`（它们直接用 `AppletFrame`）
- 测试：切页后只有 1 个页面视图存活；预加载只拉数据（不建视图）；焦点交接正确
- 验收：demo 全部页面可切换、无内存增长、截图对比

### 第 6 步 · 跨层飞行 + 背景图 + 收尾
- `FlyingItem`（幽灵绘制，不真移动视图）；`BackdropLayer` 接 `Image`
- 删旧 `AppletFrame`（此时已无使用者；`bklv/bklv.hpp` 伞头文件同步）
- 文档：更新 `AGENTS.md`（Shell 约定、焦点规则）、`docs/`；清理构建缓存

## 5. 新旧对照

| 旧 | 新 |
|---|---|
| `AppletFrame : Box(COLUMN)`（header/content/footer 三段式） | `Shell`（四层） |
| `setHeaderVisible/setBottomBarVisible` | `setHeaderVisible/setFooterVisible`（件在覆盖层） |
| 内容自动避让 | 页面显式 `getHeaderInsets()/getFooterInsets()` |
| 焦点靠树序 | `FocusManager`（显式焦点链 + 几何导航 + 栈式记忆） |
| 页面常驻（demo `pages[]`） | `Pager` 懒加载 + 预加载 |
| `pushContentView/popContentView` 用于下钻 | 保留（层级导航），同层换页走 `Pager` |
| 模态只由 `App::pushOverlay` | `Shell::pushDialog`（覆盖层内）+ `App::pushOverlay`（系统级） |

## 6. 验收总表

| 项 | 标准 |
|---|---|
| 测试 | `./scripts/check.sh` 全绿；焦点 6 条 + 布局 5 条 + Shell 3 条 + Pager 3 条 |
| 警告 | GL/VK **零警告** |
| 视觉 | 每步截图复核：角落定位、四层可见性、切页、弹窗层级、跨层飞行 |
| 性能 | 界面静止时 `App::getLayoutCount()` 不再增长（绝对层不参与重排） |
| 不回归 | demo 14 页可切换；既有 widget/transition 测试语义不变（仅 API 迁移） |

## 7. 风险与对策

| 风险 | 对策 |
|---|---|
| 分层后焦点顺序失效 | 第 3 步先落地 + 6 条测试，之后再动 demo |
| 交叉轴/主轴搞混导致"单个子件推不到边" | 明确：主轴靠绝对定位、交叉轴用 `alignSelf`；测试各覆盖一条 |
| 页面销毁留下悬空焦点/指针 | 焦点记录存路径 + hint；切页顺序"先挂新页给焦点、再删旧页" |
| 绝对层尺寸不同步 | 绝对层挂画布尺寸变化通知，测试覆盖改视口 |
| 迁移面大（demo 14 页 + 2 个测试） | 先新增后迁移再删旧；旧 `AppletFrame` 活到最后一步才删 |

## 8. 未纳入（明确不做）

- 背景**视频**（后续排期；本期只做图片）
- 用 `TabFrame` 做懒加载（语义是"内容托管、切换不销毁"，与"离开即释放"相反）
- 跨层动画用"真移动视图"（要处理换父节点/Yoga 抖动/坐标换算/焦点悬空四件事）

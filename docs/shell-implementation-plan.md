# bklv 外壳重构 · 完整实施方案 v2（经三轮场景推演审查）

> 本文件是唯一实现依据。讨论记录：`docs/layers-and-focus.md`、`docs/shell-refactor-plan.md`。
> v2 相对 v1 的改动全部来自三轮场景推演（见文末"三轮审查记录"）。
> 原则：**每个能力先推演使用场景，再决定需要哪些配套能力**，不留"能编译但不好用"的接口。

## 0. 不变量与工作纪律

**不变量（每步都必须成立）**
1. `./scripts/check.sh` 全绿；**验收基数为当前 1832 项的下限**，只增不减、GL/VK 零警告
2. 改代码用**精确锚点 + 单处替换 + 立刻构建 + 截图复核**；删代码块用**括号配对器**
   （同时数 `()` 与 `{}`）—— 正则删块 / 找 `});` / 宽泛字面量替换这三种方式本项目各翻车过一次
3. 改坏立即 `git checkout` 回退，不把坏代码留在工作区
4. 样式能在 JSON 表达的，不要只留 C++ 接口

## 1. 接口风格（照 AGENTS / borealis）

PascalCase 类型 · UPPER_SNAKE 枚举值 · camelCase 动词方法 · `is*/has*/can*` 查询 · `get*` 取值 ·
`set<名词>Enabled/Visible` 开关 · `setOnXxx` 回调 · snake_case 文件。
已有 `PositionType` / `AlignSelf` 的注释就写着"对应 borealis 的 …"，新接口继续同风格。

## 2. 架构

```
Shell（取代旧 AppletFrame；保留 JSON 类型名 "AppletFrame" 作为别名，见 5.3）
├─ BackdropLayer   背景层  绝对定位；本期只做图片；**支持按页切换背景**（淡入）
├─ ContentLayer    内容层  Yoga 根；Pager（页面栈 / 同层换页）
├─ OverlayLayer    覆盖层  Yoga 根；页头 | 底栏 | **遮罩 + 弹窗**
├─ HudLayer        浮层    绝对定位、不吃输入；toast、常驻指示
└─ TransitionHost  过渡层  绝对定位、**仅动画期间存在**；跨层飞行元素（v2 修正：不再挂 HudLayer）
```

### 层契约

| 规则 | 内容 |
|---|---|
| 绘制顺序 | 背景 → 内容 → 覆盖 → 浮层 → 过渡 |
| 重排 | **表述修正**：绝对定位节点**仍在 Yoga 树里**。真实含义是"内容变化不触发 layout，只有尺寸变化才 layout" |
| 焦点 | 只在**内容层与覆盖层**之间切换；页头/底栏默认**不可聚焦**（需显式 `setFocusable(true)`） |
| 模态 | 用**标记**（`pushModal` / `modalityRoot`），不靠层号 |
| 内容避让 | **不自动**：页面显式 `getHeaderInsets()/getFooterInsets()`；**动画期间返回目标值** |
| 安全区 | `setSafeAreaInsets(EdgeInsets)`：所有层按安全区内缩（Switch 前置需求） |
| 坐标 | 跨层读 `getFrame()`（绝对）；不换算 |
| 视口变化 | 绝对层挂画布尺寸变化通知 → `requestLayout()`；百分比定位自动跟随，像素定位保持不变（文档写明） |

## 3. 能力清单（v1 全部 + 三轮推演新增）

### 3.1 `Shell`
```cpp
enum class Corner { TOP_LEFT, TOP_CENTER, TOP_RIGHT, CENTER_LEFT, CENTER,
                    CENTER_RIGHT, BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT };
enum class Edge { TOP, RIGHT, BOTTOM, LEFT };

struct DialogOptions {
    bool dim             = true;   // 弹窗后加遮罩
    bool closeOnDimClick = true;
    bool closeOnBack     = true;
};

class Shell : public View {
public:
    Box* getBackdropLayer(); Box* getContentLayer();
    Box* getOverlayLayer();  Box* getHudLayer(); Box* getTransitionHost();

    void setHeaderVisible(bool);  void setFooterVisible(bool);
    bool isHeaderVisible() const; bool isFooterVisible() const;
    EdgeInsets getHeaderInsets() const;   // 动画期间返回**目标值**
    EdgeInsets getFooterInsets() const;
    void setSafeAreaInsets(EdgeInsets);

    void addToCorner(View*, Corner, float insetX, float insetY);
    void addToEdge(View*, Edge, float inset);
    void fillLayer(View*);                       // 四边 0（loading 遮罩等）

    void setBackdropFor(const std::string& pageId, View* backdrop);  // 按页背景，切换淡入
    void showBackdropFor(const std::string& pageId);

    void pushDialog(View*, DialogOptions = {});  // 挂覆盖层顶部 + 模态标记 + 遮罩
    void popDialog(View*);

    void setSystemAction(Key, std::function<void()>);        // 不被模态吞：截图/Home/退出
    void setOnBackRequested(std::function<bool()>);          // true = 已处理
};
```

### 3.2 焦点（`bklv/core/focus_manager.*`）
```cpp
enum class FocusDirection { UP, DOWN, LEFT, RIGHT };
enum class CrossBoxPolicy { ALLOW, SAME_PARENT_ONLY, NONE };

struct FocusRecord {            // 存**路径 + hint**，绝不存裸 View*
    int layer;                  // 0=内容层 1=覆盖层
    std::string pageId;         // 同层换页也用它做键
    std::string viewPath;
    int hint = -1;              // 上次列表下标等
};

class FocusManager {
public:
    View* getCurrent() const;                      // **唯一真相**（App 只转发，见 5.1）
    void  setCurrent(View*, FocusReason);

    void  enterLayer(Box* layerRoot);              // remember() → 取该层默认焦点
    bool  leaveLayer();                            // restore() → 失败退本层默认焦点
    void  remember(); void rememberFor(const std::string& key);   // v2：同层换页按页记忆
    bool  restore();  bool restoreFor(const std::string& key);

    bool  move(FocusDirection);                    // 几何最近邻 + 冒泡 + 出口策略
    void  setCrossBoxPolicy(View*, CrossBoxPolicy);
    void  setLayerSwitchOnEdge(bool);              // v2：到边界是否跨层，**默认 false**
    bool  canFocus(View*) const;                   // 四条件
    void  ensureFocusVisible();                    // v2：把焦点元素滚进视野（回收列表必需）
};
```

### 3.3 页面（`bklv/widgets/pager.*`）
```cpp
using PageFactory = std::function<View*(PageContext&)>;

class Pager {
public:
    void setSlot(ScrollingFrame* slot);
    void setPageFactory(const std::string& id, PageFactory);     // v2：工厂式，不强推继承
    PreloadHandle preload(const std::string& id);                // v2：可取消
    void cancelPreload(const std::string& id);                   // 快速连按必需
    void show(const std::string& id);                            // 构建 → 挂载 → 给焦点 → 删旧页
    View* getCurrentView() const;
};
struct PageState {            // v2：页面级记忆
    FocusRecord focus;
    float scrollOffset = 0.0f;
};
```
`ScrollingFrame` 补 `getScrollOffset()/setScrollOffset()`（页面级记忆与"返回原地"需要）。

### 3.4 其它
- `FlyingItem`：跨层飞行元素（挂 `TransitionHost`，动画结束销毁或交回）
- 静态背景图：`Image` + `BackdropLayer`

## 4. 七步实施（每步独立验收）

| 步 | 内容 | 测试 | 关键验收 |
|---|---|---|---|
| 1 | 布局原语：`positionType`+四边偏移(+%)**进 JSON 属性表**、`EdgeInsets` 类型、`addToCorner/addToEdge/fillLayer` | 5 | 角落定位截图；JSON 可表达 |
| 2 | `Shell` 四层 + insets（目标值语义）+ 安全区 + 视口通知链 | 4 | 层随视口；静止时 `getLayoutCount()` 不增长 |
| 3 | **`FocusManager`**：跨层 + 同层换页记忆 + 层内几何导航 + 三档 + `ensureFocusVisible` | **10** | 见 4.1 |
| 4 | 焦点真相合一（App 转发）+ 动作路由 + `setSystemAction` + `pushDialog`（遮罩/dim） | 5 | 模态下系统键仍生效；关闭后焦点正确 |
| 5 | `Pager`（工厂 + 取消 + 失败 + loading + 滚动记忆）+ **迁移 demo 14 页** | 6 | 切页只活 1 个页面；快速连按不崩 |
| 6 | 背景按页切换 + `TransitionHost` 跨层飞行 | 4 | item 飞到覆盖层之上再落回 |
| 7 | 删旧 `AppletFrame`（保留 JSON 类型名）+ 设计器回归 + 文档 + 清缓存 | — | 全绿、`designer_core_test` 不回归 |

### 4.1 焦点验收测试（10 条）

1. 记住 → 恢复：焦点回原控件
2. **原控件已销毁** → 退本层默认焦点（不崩、不悬空）
3. 覆盖层无默认焦点 → **保持焦点**、输入仍被模态吃住
4. 模态打开时方向键**不**进页头/底栏
5. 跨 box 三档：`ALLOW` 出去 / `SAME_PARENT_ONLY` 出不去 / `NONE` 停边界
6. 弹窗套弹窗 → 焦点栈逐级还原
7. **同层换页**：主页 → 游戏库 → 返回主页，焦点回到原卡片（`rememberFor`）
8. **回收列表**：滚动后焦点元素不可见时 `ensureFocusVisible()` 把它滚回来
9. **边界策略**：`setLayerSwitchOnEdge(false)` 时在内容层顶部按 UP 停在边界，不去页头
10. **快速连按**：A→B→C 连续切页，取消的预加载不得再改 UI

## 5. v1 审查的 5 个必修项（已并入）

**5.1 焦点真相合一**：`App` 已有焦点/模态状态（`resetFocusState`、`isInsideModal`）→
`FocusManager` 为**唯一真相**，`App` 只转发；实现时列出 App 旧字段的迁移/删除清单，
并在 `App::setRootView()` 时把根注册给 FocusManager。

**5.2 "绝对定位不参与重排"措辞修正**：见层契约表（内容变化不触发 layout ≠ 不在树里）。

**5.3 JSON 类型名兼容**：`registerWidgetViewTypes()` 注册过 `AppletFrame` →
**保留 `"AppletFrame"` 类型名**（内部构造 `Shell`），否则用户已有 JSON 失效；新增 `"Shell"` 也可用。

**5.4 insets 与显隐动画**：页头折叠是逐帧改高度 → insets **返回目标值**，动画结束一次性重排；
页面若想逐帧跟随可自行 `setOnInsetsChanged`。

**5.5 转场归属**：`SLIDE_FADE` 由 **`Pager`** 负责（ContentLayer 内绘制期变换），
Shell 不做页面转场，只做层间淡入（背景/遮罩）。

## 6. 风险与对策（v2 增量）

| 风险 | 对策 |
|---|---|
| 焦点与回收列表冲突（View* 被复用） | 焦点记录存路径 + hint；`ensureFocusVisible()`；测试 8 |
| 快速连按导致"幽灵页面" | 预加载可取消 + 取消后回调必须校验（测试 10） |
| 遮罩吃掉系统键 | `setSystemAction` 优先级**高于**模态（测试见第 4 步） |
| 安全区在 Switch 上被忽略 | 层统一按 `setSafeAreaInsets` 内缩，纳入第 2 步验收 |
| 过渡层残留 | `TransitionHost` 仅动画期存在，动画结束断言层内为空（测试见第 6 步） |
| 迁移面大（demo 14 页 + 2 测试 + 设计器） | 先新增 → 再迁移 → 最后删旧；旧类活到第 7 步 |

## 7. 明确不做

背景视频（后续排期）· 用 `TabFrame` 做懒加载（语义相反）· 跨层动画"真移动视图" ·
`margin: auto`（v2 移出关键路径；如需要单独一小步做）· RTL 镜像布局。

---

## 附：三轮审查记录（每轮 = 场景推演 → 发现 → 并入）

### 第 1 轮 · 场景推演：页面与焦点
| 场景 | 发现 | 并入 |
|---|---|---|
| 主页 → 游戏库 → 返回主页，希望回到原卡片 | 同层换页也需要焦点记忆，`enterLayer` 只管跨层 | `rememberFor/restoreFor`（能力 3.2） |
| 游戏库 → 详情 → 返回，希望回到原列表项与位置 | 焦点记了，**滚动位置没记** | `PageState{scrollOffset}` + `ScrollingFrame::get/setScrollOffset` |
| 回收列表滚动后，焦点元素被移出屏幕 | 焦点"存在但看不见" | `ensureFocusVisible()`（测试 8） |
| 内容层顶部按 UP 是否跳到页头 | 需要策略，不能写死 | `setLayerSwitchOnEdge(bool)`，**默认 false**（测试 9） |
| 截图/Home/退出键在弹窗打开时还要生效 | 系统键不能被模态吞 | `setSystemAction()` 优先级高于模态（第 4 步） |

### 第 2 轮 · 场景推演：层与定位
| 场景 | 发现 | 并入 |
|---|---|---|
| 主页有大封面、游戏库要另一张背景 | 背景层需要**按页切换** | `setBackdropFor(pageId, view)` + 淡入 |
| 弹窗要压暗背景、点遮罩关闭 | 遮罩没地方放 | 覆盖层内加遮罩 + `DialogOptions{dim, closeOnDimClick}` |
| 全屏 loading 遮罩 | 需要"四边 0"的便捷写法 | `fillLayer(view)` |
| Switch 有安全区/圆角 | 层要统一内缩 | `setSafeAreaInsets(EdgeInsets)`（第 2 步验收） |
| 跨层飞行元素放哪 | 挂 HudLayer 会污染常驻浮层 | **新增 `TransitionHost`**（仅动画期存在） |
| 视口尺寸变化后绝对件怎么算 | 百分比跟随、像素不变 | 契约表写明 + 第 2 步测试 |
| 绝对定位是否"不参与 Yoga" | 措辞错误，会导致错误实现 | 修正措辞（5.2） |

### 第 3 轮 · 场景推演：异步、集成与回归
| 场景 | 发现 | 并入 |
|---|---|---|
| A→B→C 快速连按切页 | 被取消的预加载仍可能改 UI | `preload` 返回 `PreloadHandle` + `cancelPreload`（测试 10） |
| 预加载失败（文件缺失/解析错） | 会显示空页，没有反馈 | `IPage::onPreloadFailed` + 错误视图 |
| 数据未就绪就进入页面 | 需要统一 loading | `IPage::buildLoading()`（Pager 负责显示） |
| 游戏库 3000 条元数据 | 全量预加载会爆内存 | 约定 `preload` 只拉"首屏 + n"；缩略图按预算淘汰 |
| demo 14 页 + 2 测试 + 设计器都要迁移 | 漏了设计器 | 第 7 步加设计器回归（`theme_edit.cpp` 模板串、`designer_core_test`） |
| `App` 与新 `FocusManager` 状态重叠 | 两套真相必打架 | 5.1 合一（App 只转发） |
| 删旧类会让用户 JSON 失效 | 类型名兼容 | 5.3 保留 `"AppletFrame"` |
| 验收基数会随测试增加而变 | 表述不严谨 | 改为"以 1832 项为下限，只增不减" |

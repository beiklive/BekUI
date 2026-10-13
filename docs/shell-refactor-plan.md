# bklv 外壳重构方案（分层 + 焦点 + 页面架构）

> 结论已定稿（用户确认"都按建议来"）。实现时以本文件为准，替代旧 `AppletFrame` 三段式设计。
> 讨论过程记录见 `docs/layers-and-focus.md`。

## 0. 目标与背景

要拿 bklv 做**模拟器前端**（主页 / 游戏库 / 设置），现有 `AppletFrame : Box(COLUMN)`
（`header / contentBox / footer`，`widgets/applet_frame.cpp:15-40`）把界面限死成三段式：

- 内容被夹在中间那条带子里：封面大图铺不到状态栏下面，转场元素飞不过页头；
- 背景（图片）无处安放；
- 弹窗 / toast / 焦点指示没有明确层级；
- 页面**常驻**（demo 的 `pages[]` 长期持有），不符合"即时加载、离开即释放"的内存要求。

## 1. 四层结构

```
Shell（取代旧 AppletFrame）
├─ BackdropLayer   背景层   绝对定位，不参与 Yoga；本期只支持**图片**（视频后续排期）
├─ ContentLayer    内容层   Yoga 根；页面栈 / 路由都在这里
├─ OverlayLayer    覆盖层   Yoga 根；**页头、底栏、弹窗**都在这里
└─ HudLayer        浮层     绝对定位、不吃输入；**焦点指示**、toast、跨层飞行的临时元素
```

命名理由：原提议的 `PopFrameLayer` 容易被理解成"弹窗层"，而弹窗在覆盖层；
`HudLayer`（抬头显示）更贴合"瞬时、不吃输入、总在最上"。备选 `FloatLayer`。

### 层规则（实现契约）

| 规则 | 内容 |
|---|---|
| 绘制顺序 | 背景 → 内容 → 覆盖 → 浮层（后者盖前者） |
| Yoga | **只有内容层与覆盖层参与重排**；背景层、浮层绝对定位不参与 |
| 焦点 | **只在内容层与覆盖层之间切换**；背景层、浮层永远不参与 |
| 模态 | 用**挂载标记** `pushModal()/modalityRoot` 表示，**不用层号判断** |
| 内容避让 | **不自动**：页面显式调用 `getHeaderInsets()/getFooterInsets()` 决定留白 |
| 坐标 | 跨层读 `View::getFrame()`（`core/view.cpp:413`，**绝对坐标**）；不换算、不重排 |
| 层尺寸 | 绝对层跟随视口尺寸，必须挂"画布尺寸变化"通知，否则改窗口后尺寸不同步 |

### 已定决策

1. 弹窗挂**覆盖层顶部**（新增 `Shell::pushDialog()`）；
   `App::pushOverlay()` 保留给**系统级/全屏模态**（它挂 root 之上，比浮层还高）。
2. 跨 box 导航默认 **ALLOW**，另提供 `SAME_PARENT_ONLY` / `NONE` 两档（局部生效、内层可覆盖）。
3. **放弃旧 `AppletFrame` 设计**；demo 与测试一并迁移（`widget_test` / `transition_test` 有多处使用）。
4. 背景层本期**只做图片**；视频单独排期。

## 2. 页面架构（懒加载 + 预加载）

页面**不常驻**：进入时构建、离开即释放。`ScrollingFrame::setContentView` **只摘不删**
（`views/scrolling_frame.cpp:71-74`，注释里写明"调用方可能还留着指针"），所以**所有权在路由手里**，
这一点对懒加载正好。

```
PageRouter
├─ PageId current / pending
├─ preload(PageId)   焦点移到某项时触发：只加载**数据**（元数据 + 首屏缩略图）
└─ show(PageId)      确认时触发：用已加载数据**即时构建**视图

IPage
├─ preload(ctx)  异步预热数据（不建视图）
├─ build(ctx)    用已就绪数据构建 View（毫秒级）
├─ release()     放下重资源
└─ isReady()

PageContext { GameRepo& repo; ImageCache& images; AppSettings& settings; }
```

切页顺序（**焦点交接必须在删旧页之前**）：

```cpp
void PageRouter::show(PageId id) {
    View* next = pages[id]->build(ctx);
    contentSlot->setContentView(next);   // 先挂新页
    focusDefaultOf(next);                // 再把焦点交给新页
    if (currentView) delete currentView; // 最后删旧页（裸指针立刻置空）
    currentView = next;
    shell->setTitle(names[id]);
}
```

预加载触发点：`Sidebar::addItem(title, onActivate, onConfirm)` —— `onActivate`（焦点到位）预热数据，
`onConfirm` 才真正切页。空闲预加载用 `task::delay(ms, cb)`（主线程泵，不占线程）；
耗时扫描/解码用 `task::run*` + **`AliveFlag` 护栏**（回调里先判存活，否则 use-after-free）。

缩略图：`ImageCache` 设预算 + 淘汰，只保留首屏 + 最近若干个；解码走
`Image::setImageFromFileAsync`（异步）。

## 3. 焦点管理

### 3.1 跨层切换

- **内容层 → 覆盖层**：覆盖层**显示完成后**才切焦点 → `remember()` 保存 → 取覆盖层默认焦点
- **覆盖层 → 内容层**：**开始关闭时**就 `restore()` → 失败则退内容层默认焦点；
  **动画播完再摘视图**（与库"等落位再给新页""播完再摘"的既有约定一致）
- 时序依据：`widgets/applet_frame.hpp:62`"转场期间焦点先不挪"

### 3.2 三项能力

1. **获取默认焦点**：`getDefaultFocus()` 链（库已把它列为子类扩展点）
2. **记录 / 恢复**：做成**栈**（弹窗套弹窗、详情页下钻各占一层）；
   记录 **路径 + hint**（层 / 页 id / 控件路径 / 上次列表下标），**不存裸 `View*`**
3. **补救**：恢复时校验存在性 → 不存在退本层默认焦点 → 本层也没默认焦点则
   **保持当前焦点、把输入路由给模态**（绝不把 current 置空）

### 3.3 层内导航

- `canFocus = isFocusable() && !isDisabled() && isVisibleInTree() && isReachableFromModal()`
  （可见性**沿父链**判断；模态要同时约束**同层其它子树**，否则方向键会跑到被遮住的页头/底栏上）
- 同一 Box 内**依次向下冒泡**；`axis` 决定主轴方向（row→左右 / column→上下）；
  交叉轴方向用于跨 box；四方向统一用**几何最近邻**（同向 + 重叠度 + 中心距），**不是索引顺序**
- 跨 box 策略：`ALLOW`（默认）/ `SAME_PARENT_ONLY` / `NONE`，设在 box 上、影响子树、内层可覆盖

### 3.4 焦点框绘制

- **唯一真相**是 `FocusManager::getCurrent()`；浮层只是读者
- 控件自带的 `hlBg / hlGlow / hlBorder`（focused 状态的外观层）**保留** —— 它是外观的一部分，
  主题可逐控件调参；浮层只放**跨层/临时**指示（toast、跨层飞行元素、无外观控件的通用焦点圈）

### 3.5 接口草案

```cpp
enum class FocusDirection { UP, DOWN, LEFT, RIGHT };
enum class CrossBoxPolicy { ALLOW, SAME_PARENT_ONLY, NONE };

class FocusManager {
public:
    View* getCurrent() const;                       // 唯一真相
    void  setCurrent(View*, FocusReason);
    void  enterLayer(View* layerRoot);              // remember() → 取默认焦点
    bool  leaveLayer();                             // restore() → 失败退本层默认焦点
    void  remember();                               // 入栈：{层, 页 id, 控件路径, hint}
    bool  restore();                                // 出栈：校验 + 用 hint 找焦点
    bool  move(FocusDirection dir);                 // 几何最近邻 + 冒泡 + 出口策略
    void  setCrossBoxPolicy(View* box, CrossBoxPolicy);
};
```

## 4. 动作路由（分层后必须显式转发）

页头 / 内容 / 底栏不再是父子，"沿父链冒泡"不再成立。B 键（返回）优先级：

```
弹窗（模态） → 内容页（自己的 B 语义） → 底栏 / 页头 → shell 默认（回主页）
```

## 5. 动作项（分层带来的新增成本，不能省）

| 新增能力 | 为什么必须 |
|---|---|
| **跨层焦点链** | 页头在覆盖层、页面在内容层，是两个 Yoga 根 → 树序 ≠ 视觉顺序，方向键会卡在页头或跳过底栏 |
| **动作路由** | 见上 |
| **模态标记** | 覆盖层里既住着可点的页头/底栏，又住着要挡的弹窗，不能按层号判断 |

## 6. 落地里程碑

| 步 | 内容 | 验收 |
|---|---|---|
| 1 | `Shell` 四层骨架 + insets 查询 + 层跟随视口 | 层尺寸正确；页面按 insets 留白正确；改窗口后背景层同步 |
| 2 | **`FocusManager` + 跨层焦点链 + 动作路由 + 模态标记** | 下列 6 条测试全绿 |
| 3 | 弹窗挂覆盖层（`pushDialog`）+ toast 队列到浮层 | 弹窗盖住页头但被 toast 覆盖；toast 不压底栏 |
| 4 | `Pager` 懒加载 + 预加载 + 跨层飞行元素（幽灵）+ 静态背景图 | 切页只活一个页面视图；item 从内容层飞到覆盖层之上再落回 |

### 焦点验收测试（无窗口可测）

1. 记住 → 恢复：焦点回到原控件
2. **原控件已销毁** → 退到内容层默认焦点（不崩、不悬空）
3. 覆盖层无默认焦点 → 焦点保持、输入仍被模态吃住
4. 模态打开时方向键**不**跑到页头/底栏
5. 跨 box：`ALLOW` 能出去、`SAME_PARENT_ONLY` 出不去、`NONE` 停在边界
6. 弹窗套弹窗 → 焦点栈两层，逐级正确还原

## 7. 明确不做

- 背景视频（后续排期）
- 旧 `AppletFrame` 的兼容层（用户确认可完全放弃）
- 用 `TabFrame` 做懒加载（它的语义是"内容托管、切换不销毁"，与"离开即释放"相反）
- 跨层动画用"真移动视图"（改为幽灵绘制/过渡层：真移动要处理换父节点、Yoga 抖动、
  两层坐标换算、焦点悬空四件事）

## 8. 风险与对策

| 风险 | 对策 |
|---|---|
| 分层后焦点顺序失效 | 第 2 步先做焦点链 + 6 条测试，再谈其它 |
| 页面销毁留下悬空焦点/指针 | 焦点记录存路径 + hint；切页顺序"先挂新页给焦点、再删旧页" |
| 内容与页头/底栏重叠或留空 | insets 显式查询 + 测试覆盖"栏显隐"两种情况 |
| 绝对层尺寸不同步 | 层挂画布尺寸变化通知 |
| 每层各一个 Yoga 根导致重排次数上升 | 背景层/浮层不参与重排；只有内容层每帧参与 |

## 9. 实现时的工作纪律（本项目踩过的）

- 改代码用**精确锚点 + 单处替换 + 立刻构建 + 截图复核**；删代码块用**括号配对器**
  （同时数 `()` 与 `{}`）—— 正则删块 / 找 `});` / 宽泛字面量替换这三种方式本项目各翻车过一次
- 每步：自审 → 改 → 补测试 → GL/VK 零警告与全绿 → 提交
- 不留坏代码在工作区：改坏立即 `git checkout` 回退，不提交半成品

# 列表/网格：增量追加、大数据滚动、预加载与缓存销毁 —— 设计方案（评审稿）

> 目标读者：维护者。本文只讲**方案**，不含实现。承接 `docs/parse-refactor-review.md` 的分层红线。

## 1. 现状（先说清楚我们手上有什么）

- `RecyclerFrame : ScrollingFrame`：`rows[]` 与 item **1:1**（每项高度 + top，追加/滚动/焦点/回收都按它索引）
- 已经有：**cell 复用池**（摘下来不删、等下次复用）、`registerCell(identifier, factory)`、
  `getItemIdentifier` 复用身份、`setPreFetchLine(n)`、`setSectionHeaderIdentifier`
- cell 与数据源**都是下拉式（pull）**：库只问"有几项/多高/什么身份" -> **没有任何"该加载更多了"的信号** ✗
- 追加数据目前会走**全量重建 rows** ✗ -> 1000 行时每页 O(n)

## 2. 目标与非目标

目标：① 滚动到底自动追加（10 -> +20 -> …）② 千行级滚动不掉帧 ③ 预加载可调
④ 内存有上限（cell 与业务数据都能被回收）⑤ 未加载区域不把滚动条/滚动位置搞乱

非目标：库内**不做网络**（网络与分页策略留在应用的数据源里）；v1 **不做**百万行的"元数据裁前缀"（见 §7）

## 3. 核心模型：数据窗口 + 三态

```
[可回收区]      [已加载窗口 loaded]      [未加载/未知]
   ↑ keepBefore      ↑ 可见区 ± prefetch        ↑ hasMore
```

数据源对外暴露（都在**已有的** `RecyclerDataSource` 上加**可选**虚函数，默认实现保证旧源零改动）：

```cpp
virtual bool   hasMore()                       { return false; }   // 还能不能再要
virtual size_t getLoadedCount()                { return getItemCount(); }
virtual void   requestMore(size_t want, size_t from) { }           // 异步发起（实现里用 task::run*）
virtual void   setLoadListener(std::function<void()> cb) { }        // 数据到了通知回收器刷新
virtual float  getEstimatedItemHeight()        { return 0.0f; }      // 未知区域的占位高度（0=用默认）
virtual void   onWindowReleased(size_t from, size_t to) { }          // 库告诉应用"这段可以丢了"
```

**分层**：这些是**纯数据接口**（views）✓；网络、分页、缓存策略全在应用的实现里 ✓。

## 4. 触发时机（预加载）

- 每次滚动/每帧算：`tail = firstVisibleRow + visibleRows + prefetchLines`
- 若 `tail >= loadedCount - threshold` 且 `hasMore()` 且**没有请求在飞** -> `requestMore(pageSize)`
- **单飞去抖**：一个请求在飞时不重复发；拿到数据后 `loadListener` 通知 -> 只做**尾部增量**
- `pageSize` 由**数据源**决定（10/20 是它的事）；JSON 里只写 `onEnd` 名字（已有）+ `prefetchLines`

## 5. 追加的性能：绝不全量重建

- `rows` **增量追加**：新条目只 `push_back`，`top` 从上一行接着累加（已有 top 语义）
- 多列的**视觉行顶**只重算**尾部**（最后一个不完整行）
- `notifyDataChanged()` 拆成两个语义：`notifyItemsAppended(from)`（增量）/ `notifyDataReset()`（全量）
  -> 追加不再走全量 ✗

## 6. 缓存与销毁（两层，职责分开）

| 层 | 归谁管 | 策略 |
|---|---|---|
| **cell（视图对象）** | 库 | per-identifier 池 + **上限**（超出**真正销毁**）-> 滑过 1000 行后池里不会堆 1000 个 cell |
| **元数据 `rows[]`** | 库 | v1 保留全部（每行 ~几十字节，几千行没问题）；百万行级见 §7 |
| **业务数据（图片/网络对象）** | **应用** | 库只给时机：`onWindowReleased(from,to)`；丢不丢、丢多少由应用决定 |

> 关键取舍：**"什么时候能丢"由库说、"丢什么"由应用说** —— 这样库不认识业务，应用也不用猜时机 ✓

## 7. 未知总数：滚动条长度、滚动位置、焦点

- 滚动范围 = 已加载高度 + `剩余估算高度`（`estimatedItemHeight × 未知数量`，未知时给"至少一屏"）
- 拿到数据后**修正**范围；为防跳动，滚动位置按"**锚定当前首行**"重算
- 焦点导航到未加载区：先触发加载 + 记一个 `pendingFocusIndex`，数据到了再 `ensureFocusVisible`
- 前端常见的"骨架屏"就在这里：`slotLoading`（已有）在未加载区显示

**v1 明确不做**：把 `rows[]` 改成"窗口偏移 + 环形"以支持百万行 —— 那会动到滚动/焦点/回收三处的索引语义，
收益不足（几千~几万行用全量元数据完全够）。

## 8. JSON 侧新增属性（很小，仍然"只写名字/数值"）

```json
{ "type": "Recycler",
  "data": "@data/feed",
  "itemAppearance": "cell/feed",
  "itemBinder": "feedCell",
  "columns": 1,
  "onEnd": "loadMore",          // 已有：增量加载的挂点名（实现留 C++）
  "prefetchLines": 3,           // 预加载行数（默认 1）
  "pageSize": 20,               // 提示数据源每次要多少（可选）
  "estimatedItemHeight": 72,    // 未知区域的占位高度（可选）
  "slotLoading": "view/skeleton" }
```

**不做**：`"items": [...]`（JSON 不描述数据 ✗，这条红线不破）

## 9. 落地顺序（每刀独立提交、可回滚、三平台 0 告警）

1. **数据源扩展接口**（views，纯接口，默认实现不改行为）+ mock 分页源测试
2. **rows 增量追加 + notifyItemsAppended**（性能）+ "追加 1000 次不 O(n²)"测试
3. **cell 池上限与销毁**（内存）+ "滑过 1000 行后池内 ≤ 上限"测试
4. **未知总数**：估算高度 + 锚定滚动 + pendingFocus（+ 现有 slotLoading 占位）
5. **JSON 属性 + 图鉴页切"分段 + 分页加载"** -> 截图验收

## 10. 风险与已知取舍

- 异步加载与 cell 复用的竞态：数据到了只改 `rows` + 通知刷新；item 内容由 binder 在
  `setSectionIndex` 时重套（已有）；异步回调按 AGENTS 的 `AliveFlag` 加护栏
- `rows[]` 与**绝对下标**绑定：这是 v1 不做"元数据裁前缀"的原因（§7）
- 别把分页/缓存策略塞进库：库只提供**信号**（hasMore / onWindowReleased / 估算高度）

## 11. 落地进度（实施记录）

| 刀 | 内容 | 状态 | 测试 |
|---|---|---|---|
| 1 | 数据源扩展接口（hasMore/getLoadedCount/requestMore/setLoadListener）+ `RecyclerLoadConfig`（5 个旋钮，安全默认值）| ✅ 完成（`77d4ca7`）| 21 项 |
| 2 | `rows` 增量追加（`notifyItemsAppended`）—— **实测取高度次数 = 1000**（全量重建是 1 万+）| ✅ 完成（`d37c3c3`）| 7 项 |
| 3 | cell 池**可配置上限**，超出**真正销毁**（`shouldPoolCell` 纯函数）| ✅ 完成（`ffbb5da`）| 12 项 |
| 4a | **加载触发**：滚到末尾附近要下一页 + **单飞去抖** + 数据到达走增量追加 | ✅ 完成（`c6e43a2`）| 12 项 |
| 4b | 未知区域**高度估算** `getEstimatedScrollHeight()`（可配置、不虚报）| ✅ 完成（`eec07c1`）| 10 项 |
| 4c | `pendingFocusIndex`：焦点进未加载区 -> 先加载 -> 数据到了交给调用方滚 | ✅ 完成（`4e69c81`）| 13 项 |
| 5a | 旋钮接成 **JSON 属性**（pageSize/loadThreshold/maxPoolPerIdentifier/estimatedItemHeight；负值夹 0）| ✅ 完成（`fdac783`）| 30 项 |
| 4d | 估算**接进滚动条长度** + 数据到达时**锚定首行不跳动** | ⏳ 待做（要碰 `ScrollingFrame`，风险较高，带测试做）| — |
| 5b | 图鉴页切"分段 + 分页加载更多" + **截图验收** | ⏳ 待做 | — |
| 3.5 | **图片资源**：共享纹理缓存（引用计数 + 预算 + LRU）/ 请求代际 token / 背压（并发上限 + 每帧上传配额）| ⏳ 待做（建议与第 3 刀共用一套预算与淘汰逻辑）| — |

### 11.1 关于"图片资源"的补充设计（3.5 刀）

列表项的数据与**图片资源**是两类生命周期，不能一套糊过去：

- **加载的所有权不能挂在 cell 上** —— 挂在 cell 上，cell 被回收/销毁就会取消或丢结果，
  滚动一快就"图一直加载不出来/一直闪"。正确做法是挂在**资源缓存**上，cell 只借用 `TextureRef`，
  销毁 cell 不影响加载，加载完的图还能被别的 cell 复用。
- **现状（已核对代码，更正我先前的说法）**：
  · ✅ `AliveFlag` 护栏：视图先销毁、回调后到是安全的（有测试覆盖）
  · ✅ **代际保护已经有了** —— `setImageFromFileAsync` 里
    `const uint64_t generation = ++this->loadGeneration;`，回调第一件事就是
    `if (generation != this->loadGeneration) return;` -> **"同一个 Image 被复用后旧结果写进新内容"
    这件事已经被挡住**。⚠️ 我此前在对话里说它"只是推断、未实测"——**那是我说错了**，
    代码里本来就有（`ui/widgets/basic/image.cpp`，发起时自增那行之下几行）。
  · ✅ **共享纹理缓存也已经有** —— `graphics/render/graphics.cpp` 里有 `std::map<std::string, int> g_pathCache`
    （path -> 纹理下标），命中就直接复用；`initTextureStore()` 换后端时才清空。
    ⚠️ 我先前说"同一张图被多个 cell 显示就是多份，内存 ×N"——**那也是我说错了**，
    路径缓存本来就在。（这是我在这一小块里第二次凭印象说错，都记在这里备查。）
  · ❌ **真正缺的是"预算与淘汰"**：`g_pathCache` 只增不减 —— 一个千行网格滚过去，
    所有图片纹理**永远留着**，没有预算上限、没有 LRU、没有"离窗口很远就释放"的机制。
    这才是大数据集滚动时的真实内存问题。
  · ❌ **没有背压**：一次滚出 20 个新 cell = 20 次解码 + 20 次上传（不是死锁，是掉帧风险）；
    也没有"离视口近的先解码"这种优先级。

- **所以 3.5 真正要做的两件（修正后）**：
  ① **纹理预算与淘汰**（给 `g_pathCache` 加引用计数 + 预算 + LRU；被 cell 用着的不能淘汰，
     淘汰就是释放 GPU 纹理）
  ② **背压**（解码并发上限 + 主线程每帧上传配额 + 按离视口距离排优先级）
  代际护栏、销毁护栏、路径共享**三个都已经有了，不要重复造**。
- **策略**：key = 资源路径 + 尺寸，引用计数 + 预算 + LRU；每个 `Image` 自增 **token**，回调比对；
  只在"可见 + prefetchLines"范围发起；队列按离视口距离排序；主线程**每帧最多上传 N 张**。
- **配置**（与加载旋钮同风格，全部有安全默认值）：`decodeConcurrency` / `maxUploadsPerFrame` /
  `textureBudgetMB` / `poolLimitPerIdentifier`。

### 11.2 待办：绑定函数的「段内下标」契约（5b 收尾的卡点）

现状：`binding::ItemBinder` 是 `void(View& item, size_t index)`。而回收器**分段**路径走
`RecyclerCell::setSectionIndex(section, indexInSection)`、单段路径走 `setIndex(index)` ——
所以绑定函数拿到的"下标"在分段时是**段内下标**，它**不知道是哪一段**，无法索引正确的数据。

**已定契约（下一步照此实现）**：把签名扩展为

```cpp
using ItemBinder = std::function<void(View& item, size_t section, size_t indexInSection)>;
```

理由：
- 显式传段号，**不需要**让使用者 `static_cast` 到某个 cell 具体类（也就不会重犯那一轮的 UB）
- 单段列表里 section 恒为 0，语义不变（迁移成本只是多一个参数）
- `RecyclerItemCell` 的两个入口都天然有段号（`setSectionIndex` 直接给；`setIndex` 补 0）

影响面（都在本仓库内，一次改完）：`views/item_binder.{hpp,cpp}`（类型与测试）、
`ui/widgets/recycler_view.{hpp,cpp}`（两个索引入口）、`demo/app/pages/page_pokedex.cpp`（绑定函数）、
`tests/item_binder_test.cpp` 与 `tests/recycler_json_test.cpp`。

之后再做：夹具换成**分段 + 分页**的数据源 + `sectionHeaderAppearance`，图鉴页即可恢复
「两段 + 段头」的原样，并仍然是"JSON 只描述结构与规则、数据与行为在 C++"。

### 11.3 网格演示（列表页）的现状与已排除项

「列表」页现在是：页头提示 -> **网格区块**（`setColumns(3)` 的多列模式 + 外层固定高度 Box）-> **列表**（grow）。

**已经确定正常的**（都有实测读数，不是推断）：
- 回收器的多列布局：探针打在 `layoutCell` 上，实测 `row=0..2 cols=3 cellW=270 cellH=140 top=0` —— 尺寸/位置/列数**全对**
- 可视范围：3 列时会创建 3 个 cell（此前只建 2 个，已修：范围要对齐到视觉行首/行尾）
- 绑定链路：`GridCell::applyIndex` 被调用、段号/行号/图片路径都正确
- 列表区块本身：`cellW=810 cellH=64 top=0/64/128` 正常，图片 FIT、glass 卡片、行间距都在

**还没解决的**：网格里的 cell **只画出背景、子节点（图/字）不出现**。
关键对照实验（一次性定性）：把网格的 cell **换成已验证能渲染的 `ListCell`** ->
网格里出现 3 个灰框、**依然没有图与字** -> 说明问题在**网格容器这一侧**（`RecyclerFrame`
+ 外层固定高度 Box + `columns>1` 的组合），**不在 cell 类**。

**下一轮的单变量排查顺序**（每次只改一个、只跑一次截图、失败即回退）：
1. 去掉外层 `Box`（网格直接挂页面，用 `setHeight(300)` + `setShrink(0)`）
2. 若仍不行 -> 把 `setColumns(1)` 试一次（判断是不是多列路径）
3. 若仍不行 -> 查"帧的尺寸来源"（外层 Box 的 100% vs 像素高度）

### 11.4 3.5 的现状（进行中）

- 已核对：代际护栏、销毁护栏、**路径纹理缓存**三者**代码里本来就有**（我先前两次凭印象说"缺"，均已更正）
- **真正缺的**：① 纹理**预算与淘汰**（`g_pathCache` 只增不减）② **背压**（解码并发上限 + 每帧上传配额 + 优先级）

**进度**：
| 小步 | 内容 | 状态 |
|---|---|---|
| 1 | 预算账目 + 可配置旋钮（`setTextureBudget(0=不限)` / `getTextureBudget` / `getTextureCount` / `isTextureBudgetExceeded`，超预算只警告**不淘汰**）| ✅ `9ba9409` |
| 2a | 只读观察：`getTextureCacheCount()` | ✅ `bab8f38` |
| 2b | LRU 使用序记账（`g_useTick` / `g_pathLastUse` / `getLeastRecentlyUsedPath()`，只记录不淘汰）| ✅ `d38b16a` |
| 2c | 引用计数存储与 API（`TextureEntry.refCount/lastUseTick` + `retainTexture`/`releaseTexture`/`getTextureRefCount`，越界 no-op、release 不下溢）| ✅ `080c2f5` |
| 2d | **接线 + 淘汰**：把 `retain/release` 接进 `Image` 的加载/换图/析构三处，实现 `trimTextureCache()`（候选 = `refCount==0` 的 LRU；淘汰时**同步删 `g_pathCache`** 并释放 GPU 纹理）| ⬜ **只剩这一步** |
| 3 | 背压：解码并发上限 + 每帧上传配额 + 优先级（协议见下）| ⬜ 待做 |

**为什么把 2 拆成 2a~2d**：每一块都是**纯加法、零行为变化、可独立测试与回滚**（`check.sh` 从 3591 -> 3599 项）。
到最后一步 2d 时，需要动的只有 `Image` 里三处成对的 `retain/release` 加一个 `trimTextureCache()`，
风险被压到最小 —— 这一步**必须在余量充足时做**，因为它决定了"纹理何时真正释放"，
写错会得到"引用漏减（永不释放）"或"淘汰了在用的纹理（use-after-free）"。

**为什么小步 1 只记账不淘汰**：淘汰的前提是"知道谁还在用" —— 没有引用计数就淘汰，
会把正在被 cell 使用的纹理释放掉（use-after-free）。所以先把账目与旋钮做出来（零风险、
可测试），再上引用计数。

### 11.5 多列网格的结论（截至第 26 轮）：容器一侧仍有未解缺陷

**已经修好并有截图证据的**：
- **帧高度被自身 flex 压塌**：探针实测网格帧 `h=10`（不是 300），而 `ScrollingFrame` 是
  `clipsToBounds` 的 -> 每个 140 高的 cell 只露出顶部一条，看起来就是"只有背景、没有内容"。
  修法：给**帧自己** `setHeight(300)` + `setShrink(0)`（`ScrollingFrame` 会设 `flex-basis:0`，
  百分比高度传不进来）。提交 `d5250af`，截图确认从"一条细线"变成三张完整高度的卡片。
- 多列可视范围接上 `columns`（此前 3 列只建 2 个 cell）：`0a6c14e`
- 多列内容高度按**视觉行**累加（此前按条目，滚动范围偏大 N 倍）：`9fab712`

**仍未解决**：网格里 cell 的**子节点（图片/标题）不绘制**，只看到卡片的背景。

**证据链（都是实测，不是推断）**：
| 观察 | 结论 |
|---|---|
| 探针打在 `layoutCell`：`row=0..2 cols=3 cellW=270 cellH=140 top=0` | 回收器的尺寸/位置/列数**正确** |
| 把网格的 cell 换成**已验证能渲染**的 `ListCell`，网格里仍只有背景 | 问题**不在 cell 类** |
| `setColumns(1)` 也一样 | **不是多列代码路径** |
| 同一个 `ListCell` 在**列表**里背景与内容都在；放进**网格**只出背景 | 问题在**网格容器这一侧** |
| 去掉中间包装层（子节点直接挂 cell）后连背景都没有 | 包装层的 `AppearanceView` 至少会画；裸 `Box` cell 在网格里连背景都不画 |

**下一步的排查方向**（留给后续）：
① 网格容器的 `clipsToBounds` / 绘制顺序（外层 Box -> 帧 -> 内容盒 -> cell）逐层对比列表那条路径
② `Box::draw` 对 `backgroundColor` 的绘制条件（为什么同一个 cell 在列表画、在网格不画）
③ 用设计器那套"手动驱动一棵不在 App 视图树里的视图"写**无窗口布局+绘制测试**，
   直接把 cell 的绘制调用序列打出来（比截图快得多）

### 11.6 流程教训（这几轮踩得最狠的）

一个**临时截图钩子被误提交进仓库**（`demo/app/demo_app.cpp` 的 TMP-SHOT），
它会在第 50 帧抢先截图到旧路径并退出 -> 我后来注入的新钩子根本没机会跑
-> **我至少有 4 轮的"实验"测的是旧二进制，结论无效**。已清理（`ef85cf0`）。

于是立了几条硬规矩（都吃过亏）：
1. **临时钩子绝不能进提交**；提交前 `grep -rn "TMP_SHOT\|TMP-PROBE" demo/ bklv/ designer/` 必须为空
2. **不用自己的 `/tmp/*.base` 备份** —— 备份会带旧钩子，恢复后重复定义导致构建失败；
   一律 `git checkout -- .` 回到已提交状态再改
3. **重建后必须看 `$?`** —— 构建失败的 run 会被当成有效实验
4. **截图后核对日志里的保存路径**是否等于本次注入的路径（这条一上线就抓到了问题）
5. **探针不要节流** —— 否则会把"没打日志"误读成"没被调用"（我真的这么误判过一次）

### 11.7 网格内容不显示：一个新推断（待验证）

把已有观察按"绘制递归深度"排一下，出现一个能解释大部分现象的假设：

| 场景 | 画出来的 | 没画出来的 |
|---|---|---|
| 网格 + 带包装层的 cell（AppearanceView 里放图/标题）| cell 的背景、包装层本身 | **包装层的子节点**（图、标题）|
| 网格 + 裸 cell（图/标题直接挂 cell）| 什么都没有 | cell 的背景与子节点 |
| 列表 + 裸 cell（`ListCell`：图/标题直接挂 cell）| cell 的背景**与子节点** | —— |

一个能解释前两行的说法：**网格里 cell 的绘制只往下走了一层**（cell 画了、它的直接子节点里的"容器/背景"画了，
再往下一层就不画了）。但第三行（同一个 `ListCell` 在列表里能画出直接子节点）又与之矛盾 ——
**除非**网格与列表走的是两条不同的绘制/裁剪路径。

**验证方法（下一轮，成本很低）**：
1. 在 `View::draw` / `Box::draw` / `ScrollingFrame::draw` 里打**层级计数探针**，
   分别看列表与网格里"每个 cell 及其子树的 draw 调用深度"，一次读数就能定性
2. 顺带对比两边的 `clipsToBounds`、`getContentOffsetY`、以及内容盒的尺寸
3. 若确认是绘制路径差异 -> 就是库里的一个真 bug（不是 demo 的问题），修完网格自然就出来了

### 11.8 网格内容不显示：已排除的 10 个假设（附实测证据，交接用）

按排查顺序，每条都有探针读数或代码依据：

| # | 假设 | 排除依据 |
|---|---|---|
| 1 | cell 的尺寸/位置算错 | 探针打在 `layoutCell`：网格 `row=0..2 cols=3 cellW=270 cellH=140 top=0` —— 全对 |
| 2 | 绑定没生效（老坑） | 探针：`GridCell::applyIndex` 被调用、段号/行号/图片路径都正确 |
| 3 | 是多列代码路径的问题 | `setColumns(1)` 也一样不显示 |
| 4 | 是 `GridCell` 这个类写错了 | 把网格的 cell 换成**已验证能渲染**的 `ListCell`，网格里依然只有背景 |
| 5 | 绘制递归"只走一层" | `Box::drawChildren` 探针：侧栏那条路径画到 **depth 8** |
| 6 | 回收器手工 draw、不走递归 | `ScrollingFrame::draw` 只画滚动条，内容走 `drawChildren` |
| 7 | 被 `drawChildren` 的"不相交剔除"跳过 | 在该分支打探针：**SKIP 0 条** |
| 8 | 裁剪矩形是缓存的旧值（h=10 那次） | `PushClipRect` 是每帧按当前 x/y/w/h 现算 |
| 9 | 绘制时 x/y/w/h 里有 NaN | `View::frame` 探针：全程只有 1 处（底栏 `HintItemView`），与网格无关 |
| 10 | 图片没有纹理 / 纹理是占位 | `Image::draw` 探针：网格与列表**每个字段都一样** —— `idx=1,2,3`（各自独立）、`texW=texH=0`（列表也是 0）、几何正确 |

**结论**：列表与网格在 `Image::draw` 处的**可测字段完全一致**（几何、纹理下标、是否有效），
差异只能在**渲染管线的更下游**（顶点变换、drawList 提交、后端裁剪/提交顺序）。

**后续建议**：这一项需要**调试器级别**的手段，而不是继续"读代码 + 打探针"：
1. 在 `Image::draw` 下断点，检查这一帧 `ImDrawList` 里该图片的 **cmd / 顶点范围**是否真的加入
2. 或抓一帧的 **draw call 序列**（GL 用 `apitrace`/`renderdoc`，VK 用 `renderdoc`），
   看网格那三个图片的 draw call 是否发出、被哪个 clip/scissor 剪掉
3. 顺带对比"列表页"与"列表页+网格"两种布局下，`ScrollingFrame` 推入的 clip 矩形与
   内容盒的 `contentOffsetY` 是否一致

### 11.9 另一个待办：页面级探针必须先切到受测页面

我在这项上白跑过**两次**（图鉴那次、网格这次）：demo 启动在第 1 页，
而受测的列表/图鉴页要切过去才会构建与绘制 -> 探针读到的是"没渲染"，
误判成"渲染有问题"。**规矩**：任何页面级探针，先把起始页切到受测页面（临时改
`pendingStartPage`，跑完 `git checkout` 还原，且绝不提交）。

#### 小步2 的实现设计（引用计数 + LRU 淘汰，定稿待实现）

现状：`graphics/render/graphics.cpp` 里 `std::vector<TextureEntry> g_textures`（`handle` + `alive`）+
`std::map<std::string,int> g_pathCache`（路径 -> 下标）。纹理**只在 `shutdownTextureStore()`
时统一释放**，平时只增不减（小步1 只加了账目与预算旋钮）。

**协议（按这个实现，不要临时发明）**：
1. **引用计数放在仓库侧，不放在 `TextureRef` 里** —— `TextureRef` 是个小值类型（`index/texture/width/height`），
   到处按值拷贝（`getTexture()` 返回 const 引用、`Image` 里存一份）。给它加计数会让拷贝语义变复杂、
   而且很容易漏减。改成：`TextureEntry` 加 `refCount` 与 `lastUseTick`，
   仓库提供 `retainTexture(index)` / `releaseTexture(index)`。
2. **谁加减**：
   - `Image::setImageFromFile()` / `updateTexture()` 成功后 `retainTexture(新)` + `releaseTexture(旧)`
   - `Image::~Image()` / 换图 / `releaseTexture()` 时 `releaseTexture(当前)`
   - `TextureRef` 的**其它使用者**（例如未来共享的缩略图缓存）同样成对调用
3. **淘汰规则**（只在超预算且有人请求时做，不做后台线程）：
   - 候选 = `refCount == 0` 且 `alive` 的条目
   - 按 `lastUseTick` 升序淘汰 LRU，直到 `getTextureCount() <= budget`
   - 淘汰 = 真正释放 GPU 纹理 + 从 `g_textures` 标记 `alive=false` + **从 `g_pathCache` 删掉该路径**
     （否则下次命中会拿到已释放的下标）
   - **`refCount > 0` 的绝不淘汰**（这是"正在被 cell 用着的不淘汰"的落点）
4. **调用时机**：`Image::updateTexture()` 之后、以及 `setTextureBudget()` 变小之后各调一次
   `trimTextureCache()`；不要每帧无条件遍历（缓存小，但没必要）
5. **测试**（无窗口可做的部分）：`getTextureCount()`/预算边界、`retainTexture`/`releaseTexture`
   的计数、`trimTextureCache()` 在 refCount>0 时不淘汰、`g_pathCache` 同步删除；
   真正"淘汰后 GPU 纹理被释放"需要窗口环境，用截图/日志验证（并如实标注）

#### 小步3 的实现设计（背压：解码并发上限 + 每帧上传配额 + 优先级，定稿待实现）

问题：一次滚出 20 个新 cell 就是 20 次解码 + 20 次纹理上传 -> 主线程掉帧（不是死锁）。
现状：`Image::setImageFromFileAsync()` 走 `task::runResult(...)` 后台解码，回调在主线程上传；
**没有任何并发上限、没有优先级、没有每帧配额**。

**协议**：
1. **可配置旋钮**（与 `RecyclerLoadConfig` 同风格，放 `graphics/render/graphics.hpp` 的纹理预算旁边，
   未来由 JSON 属性或 C++ 直接设）：
   - `maxConcurrentDecodes`（默认 4）：同时在飞的解码任务上限
   - `maxUploadsPerFrame`（默认 2）：主线程每帧最多把几个"解码完成"的结果上传成纹理
   - `0` 一律表示"不限"（保持现有行为，安全默认）
2. **解码队列**（`foundation/core/graphics.*` 或新建 `foundation/core/texture_queue.*`）：
   - `Image` 不再直接 `task::runResult`，而是 `enqueueDecode(path, priority, 完成回调)`
   - 队列在 `pumpMainThread()`（每帧）里：把"等待中"的任务按 `priority`（= 离视口距离，越小越优先）
     启动，直到在飞数达到 `maxConcurrentDecodes`
   - 解码完成的条目进入"待上传"队列
3. **上传配额**：每帧最多从"待上传"队列取 `maxUploadsPerFrame` 个执行上传
   （上传必须在主线程；超额的留到下一帧）
4. **取消与过期**：已有 `AliveFlag`（视图销毁）与 `loadGeneration`（换图）两道护栏继续用；
   队列里**取消**一个任务时（滚出视口）直接丢弃，不浪费上传预算
5. **优先级怎么来**：`RecyclerFrame` 在 `layoutCell()` 时知道每个 cell 的行号 ->
   可见区中心距离可作为 `priority`；接口上只需要 `enqueueDecode(path, priority, cb)`，
   **不必让 graphics 认识 recycler**（分层：graphics 只认数字）
6. **可测范围（无窗口）**：队列的调度逻辑本身是纯数据（先来/优先级/并发上限/每帧配额都能测）——
   把"启动解码"和"执行上传"做成可注入的函数指针/回调，测试里用假实现计数即可；
   真实解码与上传的耗时验证需要窗口环境（如实标注）

##### 2d 施工单（已定位到确切位置，照做即可）

**先看清了现有所有权模型**（`ui/widgets/basic/image.cpp` 的析构注释就写着）：

| 类型 | 纹理归谁 | 现有行为 |
|---|---|---|
| 动图（animated）| **`Image` 自己**（`ownsTexture`）| 析构时 `releaseOwnedTexture()` 释放真纹理 |
| 静态图（走路径缓存）| **`g_pathCache` 持有** | `Image` **不释放**（注释明确写"静态图是路径缓存持有的，不能动"）|

**结论**：2d 的引用计数**不是**为了"谁能释放"（那已经分好了），
而是为了让缓存知道"哪张静态图正被用着、能不能淘汰" -> `refCount > 0` 不许淘汰。

**接线位置（`ui/widgets/basic/image.cpp`）**：赋值 `this->texture = loaded;` 的四处 —— **270 / 291 / 328 / 341 行**。
- 每处：先 `releaseTexture(this->texture.index)`（换图时释放旧的；首次为无效下标 -> 安全 no-op），
  再 `this->texture = loaded;`，再 `retainTexture(loaded.index)`（**仅静态/非 owned 的路径**）
- `releaseOwnedTexture()` 只处理 owned（**不要**在这里 release 静态图）
- `Image::~Image()` 里：若**不是** owned 且下标有效 -> `releaseTexture(this->texture.index)`

**淘汰（`graphics/render/graphics.cpp`）**：`trimTextureCache()` —— 候选 = `refCount == 0 && alive` 的 LRU
（按 `lastUseTick` 升序），淘汰时**同步 `g_pathCache.erase(path)`** 并释放 GPU 纹理；
`refCount > 0` 绝不淘汰。调用时机：`Image::updateTexture()` 之后 + `setTextureBudget()` 变小后各一次。

**测试**：无窗口可测计数/边界/LRU 选择/路径同步删除（把"启动解码/执行上传"做成可注入回调也可测队列）；
"淘汰后 GPU 纹理真被释放"需窗口环境（用截图或日志，并如实标注）。

##### 小步3 的落地进度（背压）

| 块 | 内容 | 状态 |
|---|---|---|
| 3-1 | 旋钮与默认值：`setDecodeConcurrency`(默认 4) / `setMaxUploadsPerFrame`(默认 2)，`0 = 不限` | ✅ `215da06`（26 项测试）|
| 3-2 | **队列纯调度**：`foundation/core/decode_queue.*`（优先级出队 / 并发上限 / 每帧上传配额 / 去重 / 取消，回调注入，可无窗口测）| ✅ `a3d7500`（16 项测试）|
| 3-3 | **解码限并发接线**：`Image::setImageFromFileAsync` 的 `task::runResult` 包进 `enqueueDecode(resolved, 0, ...)`；`App::frame` 每帧 `pumpDecodeQueue()` + `pumpUploads()` | ✅ `4c87fec` |
| 3-4 | **上传改由 `pumpUploads()` 按每帧配额消费**（把上传体从解码回调里挪进 `notifyDecodeFinished(resolved, upload)`）| ⬜ 未做（见下）|

**为什么 3-3 已经拿到主要收益**：解码完成回调本身就是在 `task::pumpMainThread()` 里跑的，
而 3-3 之后**同时在飞的解码数被限制在 4**（`getDecodeConcurrency()`），
所以"一次滚出 20 个 cell 同时解 20 张、随后一帧内传 20 张纹理"这个**掉帧来源已经被削掉**。

3-4 的增量收益是"把上传再摊到多帧"（例如 20 张解码完成时，每帧只传 2 张），
代价是要把上传体整段搬进 `notifyDecodeFinished` 的回调里 —— 这属于**改上传时序**，
必须单独一轮做，并且要做窗口侧验证（图片仍正常显示 + 上传确实按配额分布）。
**在没做之前，不要声称"每帧上传配额已生效"。**

#### 3.5 的端到端验证（已做，有运行时证据）

**方法**：给 `trimTextureCache()` 的释放动作加一条 INFO 日志，临时把 demo 的纹理预算压到 **4**
（远低于当时在用的 8 张精灵图），跑 200 秒（含滚动），看两件事：
① 淘汰**真的发生**；② 画面**零损坏**（正在被用着的纹理绝不能被释放）。

**结果**：
```
淘汰次数: 3
截图：网格 3 张卡片（妙蛙种子/小火龙/杰尼龟）+ 列表 8 行，精灵图全部正常
```
-> 引用计数 -> 淘汰资格 -> 预算裁剪 -> 释放 GPU 纹理 这条链**在真实运行时跑通**，
而且**"refCount > 0 的绝不淘汰"**得到端到端验证（预算 4 < 在用 8，画面却零损坏）。

**另一条观察**：demo 只有 8 条不同的精灵图路径，所以 8 行都可见时"全员在用"，
`trimTextureCache()` 会**找不到候选并停止**（这正是设计要的"不硬淘汰"）；
只有滚动导致部分 cell 被回收、其 `Image` 析构 `releaseTexture` 之后，才会出现可淘汰的候选。
**验证完成后 demo 已移除那个临时预算**（回落库默认 `0 = 不限`），
产品里不保留只为验证存在的配置。

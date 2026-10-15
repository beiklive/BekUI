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
    代码里本来就有（`views/image.cpp`，发起时自增那行之下几行）。
  · ✅ **共享纹理缓存也已经有** —— `core/graphics.cpp` 里有 `std::map<std::string, int> g_pathCache`
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
`widgets/recycler_view.{hpp,cpp}`（两个索引入口）、`demo/app/pages/page_pokedex.cpp`（绑定函数）、
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

### 11.4 3.5 的现状（未开始）

- 已核对：代际护栏、销毁护栏、**路径纹理缓存**三者**代码里本来就有**（我先前两次凭印象说"缺"，均已更正）
- **真正缺的**：① 纹理**预算与淘汰**（`g_pathCache` 只增不减）② **背压**（解码并发上限 + 每帧上传配额 + 优先级）

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

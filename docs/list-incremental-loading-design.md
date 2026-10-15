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

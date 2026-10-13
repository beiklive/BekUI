# borealis 剩余可移植能力评估

已完成：`View`（Yoga 树 + 绘制模板 + 测量回调）、`Box`（节点管理 + Flexbox 属性）、
`Label`（换行 + 省略号 + 对齐 + 排版缓存）、`App`（SDL3 帧循环）、Style/Theme、Animatable。

下面按"移植收益 / 成本"排序，只列真正值得做的。

---

## 第一优先：列表与滚动

这是 borealis 里除了 View/Box 之外价值最高的部分，也是任何内容型应用的第一需求。

### 1. ScrollingFrame（`views/scrolling_frame.cpp` 618 行）

| 机制 | 说明 | 移植成本 |
|---|---|---|
| `contentOffsetY` 用 `Animatable` 做惯性滚动 | 拖动松手后按速度衰减，不是逐帧硬跟手 | 低（Animatable 已有） |
| `setTranslation()` 偏移内容视图 | View 已经预留了 translation 字段，直接可用 | 低 |
| `clipsToBounds` + 滚动指示条 | View 已有 clipsToBounds 与 PushClipRect | 低 |
| `onLayout()` 里把自身宽度同步给 contentView | 依赖已有的 onLayout 回调 | 低 |
| 焦点滚动：焦点变化时自动滚到可见区 | 依赖焦点系统（见第二优先） | 中 |

评价：**建议做**。它与现有 `View` 的契合度很高，`translation` 和 `clipsToBounds`
当初就是为它预留的。约 200~300 行。

### 2. RecyclerFrame（`views/recycler.cpp` 591 行）

| 机制 | 说明 |
|---|---|
| `registerCell(id, allocator)` + `dequeueReusableCell(id)` | 按复用标识维护对象池 |
| `prepareForReuse()` | 复用时重置内容 |
| `cellsRecyclingLoop()` | 每帧按可视区间回收/补齐 cell |
| `preFetchLine` | 视口外多准备几行，避免滚动时白块 |

评价：**建议做，但排在 ScrollingFrame 之后**。核心就是"对象池 + 可视区间计算"，
不复杂；难点是数据源刷新（`reloadData`/`notifyDataChanged`）与焦点保持。
约 300~400 行。

### 3. HScrollingFrame（`views/h_scrolling_frame.cpp` 122 行头文件）

横向滚动，逻辑与 ScrollingFrame 同构。做完纵向后基本是复制改轴，成本很低。

---

## 第二优先：输入与焦点

当前 `bklv` 完全没有输入抽象，`App` 只是把 SDL 事件喂给 ImGui 后端。
borealis 这一层是完整的一套：`View::registerAction` + `Box::hitTest` + `Box::getNextFocus`
+ `Application::currentFocus` + 底部提示栏。

| 能力 | API | 移植成本 |
|---|---|---|
| 命中测试（鼠标/触摸） | `View::hitTest(Point)` / `Box::hitTest` | 低（递归 + `Rect::contains` 已有） |
| 方向键/摇杆导航 | `Box::getNextFocus(FocusDirection, View*)` | 中（几何打分，按方向选最近邻） |
| 焦点状态 | `currentFocus` + `onFocusGained/Lost` | 低 |
| 动作注册与提示栏 | `registerAction(hint, Button, listener)` | 中 |
| 手势（长按/滑动） | `core/touch/` + `gesture.hpp` | 低 |

评价：**建议做**。这是"应用能不能用"的分水岭。特别是做手柄/键盘导航的项目，
ImGui 自带的导航和这套是两回事，别混用。约 400~600 行。

---

## 第三优先：内容控件

### 4. Image（`views/image.cpp` 454 行）

| 机制 | ImGui 对应 |
|---|---|
| `nvgCreateImage(Mem)` 纹理上传 | `glGenTextures` + `glTexImage2D` + `ImTextureRef` |
| `nvgImagePattern` 贴图 | `ImDrawList::AddImage` |
| 圆角裁剪 | `AddImageRounded`（已有，直接可用） |
| `ImageScalingType`（fit/fill/stretch/center） | 自己算 uv + 目标矩形 |
| `setImageFromMem` / 异步加载 | 不变 |

评价：**建议做**。绘制部分比 NanoVG 版更简单（`AddImageRounded` 直接支持圆角），
主要工作量在纹理生命周期与缩放模式。约 200 行。

### 5. Button / Rectangle / ProgressSpinner

- `Rectangle`：纯色块 + 圆角，等于 `Box` 加背景色，几乎零成本。
- `Button`：`Box` + 动作 + 焦点视觉。
- `ProgressSpinner`：旋转动画，`AddCircle` + 时间。

评价：**顺手做**，每个 50~100 行。

### 6. Slider / CheckBox / SelectorCell

带输入的控件，依赖焦点与动作注册。**等第二优先做完再说**。

---

## 第四优先：文本增强

`Label` 已能换行、截断、对齐，但 borealis 还有：

| 能力 | borealis 实现 | 说明 |
|---|---|---|
| 跑马灯 | `Label::resetScrollingAnimation` + Timer | 用于超长单行标题，Animated 已有 |
| 富文本 | `RichTextSpan` / `RichTextImage` / `RichTextBreak` | 图文混排，wiliwili 自己扩展的，工作量大 |
| 简繁转换 | OpenCC 集成 | 与 UI 框架无关，按需 |
| `onLayout` 里预烘焙截断 | 已有对应实现（我们自己算的） | 已完成 |

评价：跑马灯**建议做**（成本低、观感提升明显）；富文本**先不做**。

---

## 第五优先：调试与骨架

| 能力 | 说明 | 评价 |
|---|---|---|
| `DebugLayer`（`views/debug_layer.cpp`） | 显示日志流、当前焦点、帧耗时 | **已做**：日志面板是视图树里的 `views/debug_log_view.*`（读环形缓冲、ImDrawList 直接画，不为每条日志建视图）；线框层是 `views/debug_layer.cpp` |
| `setFlexWrap` | Yoga 的 flex-wrap，做网格流式布局 | 已有 setter，如需网格可打开 |
| `collapse/expand`（`Animatable` 驱动高度） | 折叠动画 | 成本低，按需 |
| `Padding` 视图 | 纯占位 | 可有可无 |

---

## 明确不建议移植

| 模块 | 原因 |
|---|---|
| `platforms/`（glfw/sdl/switch/ps4/psv/android/winrt） | 已有 SDL3 单平台方案，抽象层在这里是负担 |
| `Application` 的 Activity 栈与转场动画 | 你的项目还没有多页导航需求，等有了再说 |
| `core/video.hpp` + VideoContext | ImGui 后端已经接管窗口与 GL |
| `AudioPlayer` | 与 UI 框架无关 |
| `ImeManager` | ImGui 有自己的 IME 支持 |
| `NotificationManager` | 依赖 Activity 栈 |
| XML / tinyxml2 / `BRLS_BIND` | 你已明确不要 XML |
| `core/style.cpp` 的文件加载与主题叠加 | 现在是硬编码表，够用 |
| `core/i18n` | 现在没有多语言需求 |
| `core/task` / `thread` / `cache_helper` | 属于工具层，跟 UI 框架没关系 |
| `core/singleton` / `bind` | 和 XML 绑定耦合 |

---

## 建议的下一步顺序

1. **ScrollingFrame** —— 投入最小、结构最契合，先把列表滚动做出来
2. **RecyclerFrame** —— 数据量大了必须要有，依赖第 1 步
3. **输入与焦点** —— 决定应用能不能真正操作，也是第 1 步"焦点自动滚动"的前提
4. **Image** —— 内容展示的刚需
5. ~~**DebugLayer**~~ —— 已实现为视图树里的日志面板（`views/debug_log_view.hpp`，挂外壳调试层）

前四项做完，`bklv` 就从"能摆布局"变成"能写内容型应用"了。

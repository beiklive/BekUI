# 移植评估：把 borealis 的设计搬到 imgui_learn，绘制从 NanoVG 换成 ImGui

评估对象：`~/Code/C++/example/wiliwili/library/borealis`（Apache-2.0，下面引用的行号实测）
目标：保留 borealis 的「Yoga 树 = View 树」设计，把 `draw(NVGcontext*, ...)` 换成 ImGui 的 `ImDrawList`
工具链：macOS + SDL3 + OpenGL3 + Dear ImGui 1.93.0 WIP（imgui_learn 现有环境）

---

## 一、结论

**可行，属于中等规模重构，不是"小改一层绘制后端"。**

理由：NanoVG→ImGui 的替换**不会**传播到整个框架。占比最大的三块——
`View`/`Box` 树结构、Yoga 集成、布局失效机制——**一行都不用改**，
`View::draw()` 以下才是重写区，约 40% 代码量。

真正的工作量和风险都集中在 **文本**（换行/截断/省略号/中文字体）和 **裁剪/圆角/图片**，
不在布局上。

---

## 二、能不能不移植？先认清一件事

你现在的 demo_04 只做了 borealis 的 5%，缺的恰好是最贵的部分：

| borealis 的核心能力 | 你现在有 | 移植难度 |
|---|---|---|
| Yoga 树与 View 树同生命周期 | ✗（每帧重建） | 低 |
| `invalidate()` 冒泡 + 按需全树重算 | ✗ | 低 |
| `YGNodeSetMeasureFunc` 文本测量 | ✗ | 中 |
| `View::frame()` 绘制模板（背景/阴影/边框/高亮/裁剪） | ✗ | 中（依赖后端能力） |
| `Box` 递归绘制 + 多层剔除 | ✗ | 低 |
| XML 声明式布局 + `BRLS_BIND` | ✗ | 中（依赖 tinyxml2） |
| Style/Theme 样式表 | ✗ | 低（纯 C++） |
| 动画 `Animatable` | ✗ | **零**（borealis 内部就是用 tweeny，和你 demo_05 同一个库） |
| 输入/焦点/导航/底部提示 | ✗ | 中高（手柄导航要自己写） |

---

## 三、逐层评估：改什么、不改什么

### 3.1 完全不用动（约 60%）

| 模块 | 为什么不用动 |
|---|---|
| `View` 的 Yoga 样式透传（`setWidth/setGrow/setMargins/setPositionType/...`） | 纯 Yoga API 调用，与绘制无关 |
| `YGNodeSetContext` + `YGNodeGetContext` 反查 | 框架无关 |
| `View::invalidate()`（view.cpp:1212） | 唯一的 `YGNodeCalculateLayout`，与后端无关 |
| Yoga 全局事件 → `onLayout()`（application.cpp:148） | 框架无关 |
| `getX/getY` 相对父节点累加（view.cpp:1228） | 数学，与后端无关 |
| `Box::addView` 同步 `YGNodeInsertChild`（box.cpp:167） | 框架无关 |
| `Box::getCullingBounds` / 剔除算法（box.cpp:113） | 数学 |
| `Style` / `Theme`（style.cpp 435 行 + theme.cpp 193 行） | 纯 C++ 表 + `NVGcolor`，只需把 `NVGcolor` 换成 `ImU32`/`ImVec4` |
| `Animatable` / `Ticking`（animation.hpp / time.hpp） | 底层就是 tweeny，零改动 |
| XML 属性注册宏、`BRLS_BIND` | 与绘制无关，只依赖 tinyxml2 |

### 3.2 必须改的接口（约 15%，但改动面广）

```cpp
// 现在
virtual void draw(NVGcontext* vg, float x, float y,
                  float width, float height, Style style, FrameContext* ctx) = 0;

// 建议改成
virtual void draw(ImDrawList* dl, float x, float y,
                  float width, float height, Style style, FrameContext* ctx) = 0;
```

`FrameContext` 换掉 `vg`/`fontStash`，保留 `pixelRatio`、`theme`。

**改动面广但不是难点**：`draw()` 是纯虚函数，编译器的错误列表就是移植清单。
实际用到 NanoVG 的文件统计（实测）：

| 目录 | 依赖 NVG 的文件 | 总文件 |
|---|---|---|
| `include/borealis/core` | 8 | 34 |
| `include/borealis/views` | 15 | 28 |
| `lib/core` | 4 | 24 |
| `lib/views` | 15 | 28 |
| `lib/platforms` | 9 | 33 |

### 3.3 真正的重写区（约 25%）

#### (1) 绘制状态栈：`nvgSave/nvgRestore` → ImDrawList

NanoVG 是**有状态**的（当前填充色、字体、变换、裁剪都在 context 里），
ImDrawList 是**无状态命令流**。所有 `View::draw()` 里的 `nvgSave/nvgRestore` 模式都要重新组织。

| NanoVG | ImDrawList 替代 | 备注 |
|---|---|---|
| `nvgSave/nvgRestore` | 无直接对应，改为把状态当参数传 | 影响所有 draw 实现 |
| `nvgIntersectScissor` | `PushClipRect` / `PopClipRect` | 可直接映射 |
| `nvgTranslate/nvgScale/nvgRotate` | 自己算坐标；复杂变换用 `AddImageQuad` | 比较烦，但 borealis 里只用了 4 处 |
| `nvgBeginPath/rect/roundedRect/fill/stroke` | `AddRectFilled/AddRect/AddRectFilledMultiColor` | 直接映射 |
| `nvgFillColor` + `nvgFill` | 每个 `AddXxx` 直接传 `ImU32 col` | 简化 |
| `nvgLinear/Radial/BoxGradient` + `nvgFillPaint` | `AddRectFilledMultiColor`（仅线性近似） | **阴影/渐变会变差**，见风险 3 |
| `nvgImagePattern` + `nvgFillPaint` | `AddImage` / `AddImageRounded` | 直接映射 |
| `nvgCreateImage(Mem)` | 自己 `glTexImage2D` + `ImTextureID` | 需自己管纹理生命周期 |
| `nvgDeleteImage` | 自己 `glDeleteTextures` | 同上 |

好消息：**`lib/views/*` 里没有一处用到渐变**（实测仅 `lib/core` 用到
`nvgLinearGradient`/`nvgRadialGradient`/`nvgBoxGradient`，共 6 处，全部集中在
`view.cpp` 的背景/阴影/高亮里）。所以渐变改写面很小，只是视觉会有损失。

#### (2) 文本：最难的一块

`Label` 的文本处理全部建立在 NanoVG 文本 API 上（label.cpp 681 行）：

| 用途 | NanoVG | ImGui 对应 | 风险 |
|---|---|---|---|
| 测量 | `nvgTextBounds` | `ImFont::CalcTextSizeA` | 低 |
| 换行测量 | `nvgTextBoxBounds` | `CalcTextSizeA(..., wrap_width, ...)` | 低 |
| 绘制 | `nvgText` / `nvgTextBox` | `dl->AddText(font, size, pos, col, txt, end, wrap_width)` | 低 |
| **逐字形位置** | `nvgTextGlyphPositions` | **无对应 API** | **中高** |
| 光标绘制 | `nvgTextWithCursor` / `nvgTextBoxWithCursor` | 无，自己画竖线 | 低 |

`nvgTextGlyphPositions` 是关键：`Label::onLayout()`（label.cpp:583）用它逐个字形比较
`i.maxx + ellipsisWidth > width` 来算省略号截断位置。ImGui 没有逐字形查询，
只能二分/线性地用 `CalcTextSizeA` 试探子串边界——功能可以实现，性能是 **O(n) 次测量/帧**
（有 `onLayout` 缓存兜底，只有 resize 和换文本才重算，可接受）。

字体：`FontStash`（`unordered_map<string,int>`）→ `ImFont*`；`nvgCreateFontMem` →
`ImFontAtlas::AddFontFromMemoryTTF`。中文用 `GetGlyphRangesChineseFull()`。
borealis 的**字体回退机制**（regular + zh-Hans + zh-Hant + emoji + 图标字体按需 fallback）
ImGui 原生不支持，要在 `ImFontAtlas::ImFontConfig::MergeMode` 基础上自己包一层按字形查表的字体选择器。

#### (3) 平台层

| borealis | 你的项目 |
|---|---|
| `platforms/{glfw,sdl,switch,ps4,psv,android}` | **只保留 SDL3 一条**，其余全删 |
| `VideoContext`（窗口 + GL + nvg 后端 + clear/begin/end） | 已被 `demo_common.h` 的 SDL3+OpenGL3 取代 |
| `InputManager` + 键盘/鼠标/触摸/手柄 | ImGui IO 覆盖键鼠；**手柄导航要自己写**（SDL3 Gamepad → 焦点导航） |
| `FontLoader` | ImGui 字体加载 |
| `AudioPlayer` / `ImeManager` / `Notifications` | 学习项目先砍掉 |

#### (4) 输入/焦点

borealis 有一整套自己的焦点系统（`currentFocus`、`Box::hitTest`、`Box::getNextFocus`、
`registerAction` + `Hints` 底部提示）。这套和 ImGui 的 hover/active/focus 是**两套东西**。

选择：保留 borealis 的（ImGui 只当画笔，不注册 ImGui 窗口），
这样手柄方向键导航、高亮动画、提示栏的代码可以整块搬过来。**建议这么做**，
比硬拼 ImGui 的导航干净。

---

## 四、工作量估算

按"能跑起来"到"能写业务"分档（单人，不含美术/业务逻辑）：

| 阶段 | 内容 | 估算 |
|---|---|---|
| 0 | 抽出 `nvg_compat.h` 兼容层（把 40 个 nvg 调用映射到 ImDrawList）+ 空窗口跑通 | 3~5 天 |
| 1 | View/Box/Yoga 移植（含 `invalidate`、`onLayout`、剔除、`getX/Y`） | 3~4 天 |
| 2 | `Label` 全部文本能力（换行、截断、省略号、对齐、字体回退） | 5~8 天 |
| 3 | 基础视觉：背景、边框、圆角、阴影、高亮、点击动画、裁剪 | 4~6 天 |
| 4 | `Image`（纹理上传、圆角、缩放模式、异步加载） | 3~4 天 |
| 5 | Serial/Recycler 等容器控件 | 3~5 天 |
| 6 | XML 布局 + 绑定 + Style/Theme | 3~4 天 |
| 7 | 输入/焦点/导航/提示 | 4~6 天 |
| 8 | 打磨（性能、draw channel、DPI、边界情况） | 3~5 天 |
| **合计** | | **约 5~7 周** |

如果只做"框架骨架 + Label + Box + Image"（够写一个像样的小应用）：**2~3 周**。

---

## 五、五个主要风险

**1. 文本（最大风险）**
中文换行、省略号截断、字体回退。NanoVG 的逐字形 API 是 ImGui 没有的，
必须自己实现且要逐帧验证。这是唯一有可能让整个移植"卡住"的地方。
建议**先写 Label 垂直切片验证**，通过了再动其他。

**2. `nvgSave/nvgRestore` 语义丢失**
NanoVG 的状态栈让父类的绘制状态可以被子类继承（字体、颜色、裁剪、变换）。
ImDrawList 无状态，所有 draw 实现要改成显式传参。
`View::frame()` 的绘制模板（view.cpp:162）会明显变长。

**3. 阴影和高光会掉质量**
`nvgBoxGradient`（高质量的柔和阴影）在 ImDrawList 里没有等价物。
`AddRectFilledMultiColor` 只能做线性近似，阴影会有明显色带。
可接受则忽略，不接受就要自己写 9-slice 阴影图或多次叠加。

**4. 性能特性反转**
NanoVG 批量合并好、状态切换便宜；ImDrawList 每条命令立即生成顶点，
且 VtxOffset 是 16 位（每 channel 上限 65535 顶点）。
borealis 的"每层级一次 `nvgIntersectScissor`"在 ImGui 里会变成频繁的
`PushClipRect`，需要合理规划 ImDrawChannel，否则长列表会爆 channel 或掉帧。

**5. 许可证**
borealis 是 Apache-2.0，你的 imgui_learn 是学习项目，没问题（保留 NOTICE 即可）。
但要**整块抄代码**的话，注意 borealis 的 `View`/`Box` 与 wiliwili 的 GPL-3.0 业务代码是两回事——
只抄 `library/` 下的框架部分，别把 wiliwili 的 `include/view/*.hpp` 带进来。

---

## 六、两种做法对比

| 项目 | A. 写 `nvg_compat.h` 垫层 | B. 直接改 `draw()` 签名 |
|---|---|---|
| 做法 | 用 ImDrawList 实现一套 `nvgBeginPath/nvgRect/nvgFill/nvgText...`，borealis 源码基本不改 | 把 `NVGcontext*` 换成 `ImDrawList*`，逐个改 draw 实现 |
| 工作量 | 小（1 个文件，约 600~900 行） | 大（40+ 文件） |
| 可维护性 | **差**：垫层无法表达 ImGui 的纹理/字体/裁剪模型，会越写越歪 | 好：与 ImGui 惯例一致 |
| 适合 | 快速验证可行性 | 长期维护 |
| 风险 | 垫层里的 `save/restore` 要自己维护一套伪状态机，容易出诡异 bug | 编译错误即清单，进展可见 |

**建议：A 用来做 3 天的可行性验证（阶段 0），验证通过后转 B 重写。**
不要试图用 A 一路做到底——垫层的伪状态机会成为新的技术债。

---

## 七、建议的推进顺序

1. **阶段 0 垂直切片**（3~5 天）：只做一件事——
   空窗口 + 一个 `Box` + 一个 `Label`，用 Yoga 布局，用 ImDrawList 画出来，
   文本要支持中文换行 + 超长省略号。**这一步证明可行或不可行，再决定要不要继续。**
2. 通过后移植 `View`/`Box` 骨架 + `invalidate()`。
3. 再扩 `Image` → 容器控件 → XML/Style。
4. 输入/焦点放最后（它不阻塞前面的验证）。

**验收标准（阶段 0）**：
- [ ] 窗口内出现 Yoga 算出的矩形，改窗口大小会重排
- [ ] 中文文本正确换行，超宽时出现 `…` 截断
- [ ] 矩形圆角、边框正常
- [ ] 一个 `Image` 能显示并裁剪成圆角
- [ ] 帧率稳定 60（VSync 下不抖）

---

## 八、一句话回答"值不值得"

- 目的是**学架构**：值得，而且应该做——这是唯一能让你真正理解
  "布局树与渲染树如何合一"的方式，比读代码有效 10 倍。
- 目的是**做个能用的应用**：不值得，直接用 ImGui 的窗口系统，
  或者在 ImGui 之上只搬 borealis 的 `Box` 递归 + 剔除 + `invalidate` 三个机制即可。

如果只搬三个机制，**约 3 天**就能得到 80% 的收益。

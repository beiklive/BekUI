# 开发计划书：液态玻璃作为 bklv 的外观层（GL / GLES3-Switch / Vulkan）

> 依据：`docs/glass-layer-feasibility.md` 的调研结论 + 用户拍板的四条决定。
> 状态：**待审**（第八节是自查审查记录）。

## 一、目标与范围（按拍板结果）

| 项 | 决定 |
|---|---|
| 渲染路径 | **桌面 GL 3.2 Core（GLSL 150）+ Switch GLES3（`#version 300 es`）+ 桌面 Vulkan**，三条都做 |
| Switch | **支持**（更正上一轮"不支持"的结论，见第八节第 1 条） |
| 参数优先级 | **外观 JSON（每层）> Shell 的 `GlassTuning`（全局默认）** |
| 默认档位 | 半分辨率 + 均衡质量（`quality = 1`）+ `blurRadius = 32` |
| 归属 | 库内 `bklv/core/glass/`（思路层）+ `bklv/backends/*`（后端实现）；**所有者是 Shell** |

不在本次范围：玻璃的"新形状"（多边形/自定义轮廓，先只做矩形/圆角矩形/胶囊/圆）；玻璃上的动画；
Android/iOS（库里本来就没有这两个平台）。

## 二、架构（与焦点框同一套单向依赖）

```
外观层 op "glass"     →  控件只声明"我这一层是玻璃 + 形状(矩形/圆角半径)"
bklv/core/glass/*          →  纯数据：Style / Request / Queue / BlurPlan / Tuning / Capabilities
bklv/core/glass/glass_service.*  →  每帧收集请求 → 排模糊计划 → onPrepareDrawData 时交后端注入
bklv/backends/{gl,gles,vk}_glass.*  →  抓帧 / 模糊 / 合成（能力不足则整层关闭）
Shell                 →  所有者：全局开关 / 质量档位 / 默认参数 / 能力查询
```

- **抓帧时机**：`App::onPrepareDrawData`（`ImGui::Render()` 之后、`renderDrawData()` 之前）——
  库里已有这条钩子（当初就是为玻璃加的）。一帧最多抓一次背景。
- **只 opt-in**：只有声明了 `op: "glass"` 的层入队，不做"禁区"（设计器的 `excludeRect` 不需要）。
- **形状复用焦点框那段逻辑**：半径钳到短边一半 → 矩形/胶囊/正圆自动成立（可抽成 `core/` 公共函数）。

## 三、API 契约

```cpp
// ---- Shell：所有者 ----
void  Shell::setGlassEnabled(bool);                 // 全局硬开关（性能/降级用，JSON 不能覆盖它）
bool  Shell::isGlassEnabled() const;
void  Shell::setGlassQuality(glass::Quality);       // FAST(半分辨率, 默认) / SMOOTH(全分辨率)
glass::Quality Shell::getGlassQuality() const;
glass::GlassTuning& Shell::getGlassTuning();        // **全局默认值**（不是最终值）
glass::EffectCapabilities Shell::getGlassCapabilities() const;
bool  Shell::isGlassSupported() const;

// ---- 视图/外观：声明形状与每层覆盖 ----
// 外观 JSON：{ "op": "glass", "radius": …, "blurRadius": …, "tintA": …, "strength": … }
// 解析规则（**用户拍板的优先级**）：
//   1) 外观层 JSON 写了这个参数 → 用它
//   2) 没写 → 用 Shell::getGlassTuning() 里的全局默认
//   3) 例外：enabled / quality / capabilities 只由 Shell 决定（是"开关与性能"，不是样式）
virtual glass::GlassRequest View::getGlassBackdrop() const;   // 默认无效 = 不要玻璃
```

**回退契约**：能力不足（后端没实现 / 着色器编不过 / 尺寸非法）→ `isGlassSupported() == false`，
所有玻璃层回退成普通绘制（不影响可读性），并且**只警告一次**，不刷屏。

## 四、阶段拆解

### 阶段 0：思路层入库（零渲染风险）
| 任务 | 文件 |
|---|---|
| 把 Style / Request / Queue / BlurPlan / Tuning / Capabilities 搬进库（去 `designer::` 命名空间） | `bklv/core/glass/*.{hpp,cpp}` |
| 单元测试：队列顺序与去重、模糊计划（对数/降采样级数/上限）、形状（半径钳制/周长） | `tests/glass_core_test.cpp` |

**验收**：`check.sh` 全过；新测试覆盖纯逻辑；设计器仍能编（先把设计器的那份改成 include 库版本）。

### 阶段 1：桌面 GL 打通
| 任务 | 说明 |
|---|---|
| GL 玻璃后端 + GLSL 150 着色器 | 从设计器搬（着色器是内联常量字符串，**不引构建期工具**） |
| 补 GL 函数入口 | macOS/Linux 用系统头（不需要）；**Windows** 缺约 30 个 → 按 `gl_loader.cpp` 的做法补 `SDL_GL_GetProcAddress` 表 |
| 服务接线 + Shell API + `onPrepareDrawData` 注入 | |
| demo 页 + 验收脚本 | 一块玻璃面板；开/关各截一张 |

**验收**：① 双后端零警告；② 不该受影响的区域**逐像素一致**；③ 玻璃区域像素标准差明显 > 0
（历史实测：不透明面板 0.00 / 玻璃 3.65）；④ 一帧只抓一次背景（日志计数）。

### 阶段 2：Switch GLES3
| 任务 | 说明 |
|---|---|
| GLES 着色器变体（`#version 300 es` + `precision highp`） | 参考项目 `shaders/opengl/*` 就是这一套，已在 Switch 上跑通 |
| 复用桌面 GL 后端，条件编译着色器与少数差异（`glTexStorage2D` GS3 有 ✓） | 库里 Switch 后端本来就 `#include <GLES3/gl3.h>`，**FBO/blit/VAO 原型直接可用**（不需要额外 loader） |
| Switch 构建脚本接线 | `scripts/build_switch.sh` 收集 `core/views/widgets` 是目录级 → 新增 `glass/` 要确认它在收集范围内或显式加 |

**验收**：`./scripts/build_switch.sh` 通过、零警告；真机跑 demo 玻璃页（用户手上有 nro 流程）；
桌面 GL 与 Switch 截图观感一致（同参数）。

### 阶段 3：Vulkan（**唯一还需要你选的点**）
两条路线，二选一或分两步：

| 路线 | 做法 | 效果 | 代价 |
|---|---|---|---|
| **A. 设计器现有版**（推荐先做） | `vkCmdBlitImage` 顺 mip 链做 box 模糊；圆角/染色/描边由 ImGui 叠着画；复用库里已有的 `VulkanUiPassHook` | 能用，**背景是上一帧的**、**没有折射/色散** | ~500 行搬运 |
| **B. 参考项目版** | 移植 `shaders/vulkan/*`（`#version 450` → SPIR-V），做真正的当帧模糊 + 折射/色散 | 与 GL 观感一致 | ~1400 行 + **SPIR-V 必须先离线编译并以字节数组嵌进源码**（不许给构建加 glslc 依赖，见 AGENTS） |

**建议**：先 A 打底（VK 用户马上能用、风险最低），再按需升 B。
**验收（无论 A/B）**：VK 校验层静默（`BKLV_VULKAN_VALIDATION=1` 跑 20s）；双后端零警告；
截图对比；接口注释与文档写明 A 路线的两个取舍。

### 阶段 4：外观 op + 优先级 + 演示与文档
| 任务 | 说明 |
|---|---|
| 外观 JSON 解析 `op: "glass"` + 参数覆盖优先级（JSON > Shell） | 在 `core/appearance.*` 加一个 op（要动 `ALL_PROPS` 的话按 AGENTS 五处同步） |
| demo 玻璃页 / 在基础控件页加一块玻璃卡 | 展示"玻璃面板 + 不透明画布并排"，方便肉眼对比 |
| 文档 | `docs/glass-layer.md`（用法/参数/优先级/能力/各后端差异）+ 更新 `layers-and-focus.md` 的层表 |
| （可选）Shell 调参面板 | 设计器那套滑块搬到库里的调试层（`Shell::getDebugLayer()`），默认关 |

**验收**：`check.sh` 全过；命名检查；文档能照着写出一个玻璃面板。

## 五、全局验收清单（每个阶段都要跑）

1. `cmake --build build/gl`、`build/vk` → **看退出码**，零警告零错误
2. `./scripts/check.sh` 全过 + `python3 scripts/check_naming.py`
3. **像素断言**：开/关玻璃各截一张 → "必须不受影响"的区域逐像素一致；玻璃区域 std-dev > 0
4. **一帧只抓一次**、静态界面下不额外重排（`App::getLayoutCount()` 停住）
5. Switch：`./scripts/build_switch.sh` 通过 + 真机出图
6. 文档与 AGENTS 同步（新文件、新 op、优先级规则、后端差异）

## 六、风险清单与对策

| # | 风险 | 对策 |
|---|---|---|
| 1 | 抓帧时背景还没画 → 采到黑 | 强制"背景先画"：库侧规定玻璃层只有在 backdrop 层之后才入队；日志里打"抓帧发生在第 N 条命令"便于排查 |
| 2 | GL 状态污染（整屏四分之三变透明） | `FramebufferGuard` + 玻璃命令后插 `ResetRenderState`（设计器已踩过并修复） |
| 3 | Windows GL 缺入口 | 阶段 1 就补表，别等到 Windows 上才暴露；补表清单在实施时逐个 `GetProcAddress` |
| 4 | VK 一帧滞后/无折射被当成 bug | 接口注释 + 文档 + `EffectCapabilities` 里如实上报（`refraction=false`） |
| 5 | GLES3 精度差异（`highp`/色带） | 着色器统一 `precision highp float`；真机截图对比；必要时加轻微抖动 |
| 6 | 性能（全屏模糊 ×N 块） | 默认半分辨率 + 均衡档；每帧最多抓一次；`maxGlassRegions` 上限；能力位可关 |
| 7 | 玻璃糊到"必须原像素"的区域 | opt-in 设计从根上避免；再加一条：不透明画布区域所在的视图不声明玻璃 |
| 8 | 一次改太多、回归难查 | 严格按阶段提交，每阶段独立验收；阶段 0/1 不碰 VK/Switch |
| 9 | SPIR-V 嵌字节数组可读性差 | 只在选路线 B 时做：`.spv` 保留在 `bklv/core/glass/shaders/vulkan/`，附生成命令与校验（哈希）在 README |

## 七、时间/工作量估算

| 阶段 | 估算 |
|---|---|
| 0 思路层 + 单测 | ~300 行 + 150 行测试 |
| 1 桌面 GL | ~900 行（含 shader 与补表） |
| 2 Switch GLES3 | ~250 行（变体 + 构建接线） |
| 3 Vulkan A / B | ~500 / ~1400 行 |
| 4 op + 优先级 + demo + 文档 | ~700 行 |

合计 **桌面 GL+Switch ≈ 2150 行；VK 视路线再加 500–1400 行**。

## 八、审查记录（自查，含我改过的主意）

1. **更正**：上一轮我写"Switch 明确不支持"是**错的** —— 参考项目
   `~/Code/C++/developing/imgui-liquid-glass` 的 Switch 端口走的就是 `OpenGLGlassBackend`（`platform/switch/include/glass_switch/GlassOverlay.hpp`），
   用的是 EGL + GLES3，用户已经在真机上跑通 nro。而且 bklv 自己的 Switch 后端
   `bklv/backends/gles3_backend.cpp` 直接 `#include <GLES3/gl3.h>` → **FBO/blit/VAO 原型可用，不需要额外 loader**。
   本计划已把 Switch 列为**一等目标**（阶段 2）。
2. **优先级写成规则而不是口号**：`JSON > Shell` 只适用于**样式参数**；
   `enabled / quality / capabilities` 仍由 Shell 独占 —— 否则"性能开关"会被某个 JSON 层悄悄打开，
   这类问题在真机（Switch）上代价很高。这一条是本计划里我唯一主动"越权"加的约束，请确认。
3. **不做"禁区"机制**：设计器的 `excludeRect` 是因为它的面板是 ImGui 窗口、覆盖面积大；
   库侧 opt-in 更强（没声明就没有玻璃），少一套机制 = 少一类 bug。已从计划里删掉。
4. **Vulkan 路线没有替你拍板**：GL+VK 是明确的，但"VK 用哪套实现"会显著影响工作量和观感，
   计划里给成 A/B 两条并建议分两步（先 A 后 B）——**这是唯一需要你再点头的点**。
5. **阶段 0 的收益被低估过**：一开始我想"思路层直接搬，不用测试"；复查后加了单测
   （队列顺序/模糊计划/形状钳制都是纯函数，最便宜的那部分保障），并把设计器改成 include 库版本 ——
   否则会出现"设计器与库两份同样的代码"这种最容易腐烂的状态。
6. **性能预算没写数值**：我只写了"半分辨率 + 均衡档"，但**没有实测帧率**。
   阶段 1 验收时补一条实测（玻璃面板数 × 分辨率 → 帧时间），再决定默认档位是否需要下调。
7. **没有排"多块玻璃互相采样"**：设计器有 `PREVIOUS_CONTENT` 这种后备来源（面板互相覆盖时用）。
   本计划先用"一帧一次抓帧"，也就是第二块玻璃采到的是**含第一块玻璃的结果** ——
   观感上通常更好（有层叠感），但要写进文档，避免被当成 bug。

## 九、明确不做

- 把玻璃塞进 Shell 的层栈（它是对已画像素的后处理，不是一层视图）
- 用 `RenderBackend::readPixels()` 做 CPU 抓帧（截图用的同步回读，每帧做会把帧率打穿）
- 引入 glslc / SPIR-V 构建期步骤（要 SPIR-V 就离线编译 + 嵌字节数组）
- 玻璃的新形状（多边形/自定义轮廓）与玻璃动画


---

## 阶段 1 完成记录（2026-10 实测）

桌面 GL 链路已端到端跑通，验收数据：

- **opt-in 保证**：demo 玻璃页开/关各截一张，不透明卡区差异 **0 / 161600 像素**（逐像素一致）
- **玻璃生效**：玻璃卡区差异 99.7%；卡内"无文字区"横向梯度 **0.07** vs 壁纸 **3.43**（模糊把条纹抹平）
- **内容在玻璃之上**：标题亮像素 2236（修好前是 0 —— 被玻璃盖住）
- **模糊贴图本身**：像素回读实测"进模糊前 2.18 → 出模糊后 0.28"
- **设计器回归**：画布内容区仍 **0 像素差**，整屏抽样差异 42 万（玻璃在画）
- 构建：GL / VK 退出码 0、零警告；`check.sh` 2975 项全过；命名检查 165 文件

两个关键坑都写进了 `AGENTS.md`（4b 守卫作用域、4c 注入偏移与命令合并）。


---

## 阶段 2 完成记录（Switch / GLES3）

玻璃的 GL 后端与桌面**共用一份正文**，差异收敛到三个点：

1. **着色器版本头**：新增 `glass::shaders::kGlslHeader` —— 桌面 `#version 150`，
   Switch `#version 300 es` + `precision highp float/sampler2D`。
   `#version` 必须是源码第一行，所以由 `linkProgram()` 拼在正文之前（正文一份，头一份）。
2. **GL 加载器**：`gl_glass_procs.cpp` 在 `BKLV_GLES3` 下**直接取函数地址**
   （`procs.x = &glx`，符号链接期就解析好了），桌面仍走 `SDL_GL_GetProcAddress` 表。
   `gl_glass_procs.hpp` 里那份"自己定义 GL 类型与枚举"的兜底在 ES3 下让位给 `<GLES3/gl3.h>`，
   避免重复定义。
3. **构建接线**：`scripts/build_switch.sh` 的 backends 清单加上
   `gl_glass.cpp` / `gl_glass_procs.cpp` / `gl_glass_shaders.cpp`（这几个目录是手列的，不是按目录收集）。

验收
- `./scripts/build_switch.sh` → **退出码 0**，`dist/switch/demo.nro` 9.8M，
  `build/switch/bklv/backends/{gl_glass,gl_glass_procs,gl_glass_shaders}.o` 三个目标产物都在
- 构建日志里 48 条 warning **全部来自 libnx 系统头**，本仓代码零警告
- 桌面回归：GL 构建退出码 0；设计器玻璃仍正常（玻璃面板内横向梯度 **0.08** = 模糊生效）

仍未做：Switch **真机画面**未验（需要实机 + nxlink）；这条要等你有机器时跑一次
（`nxlink -s dist/switch/demo.nro`，开/关玻璃各看一次）。


---

## 阶段 4 完成记录（外观层 `op: "glass"`）

- `appearance`：新增 `Op::GLASS`，解析 `"op": "glass"`
- `AppearanceView` 的 GLASS 分支：按该层矩形调用 `GlassService::submitBackdrop()`
  （内含"先切命令再入队"，所以内容不会被玻璃盖住）
- `GlassLayerOptions`：外观覆盖的样式字段（`fill` 染色 / `opacity`），
  实现"**JSON 优先于 `makeGlassStyle(Shell 的 tuning)` 默认值**"
- 主题新增 `resources/theme/default/base/glass_panel.json`（并进 `theme.json` 的 `files[]`）
- demo 玻璃页改用该外观：**视图里不再有任何玻璃代码**，只给标题文字 + 关掉时的回退底色
- 文档：新增 [`glass-layer.md`](glass-layer.md)（使用说明 + 验收数据 + 文件地图），
  `json-syntax.md` 补 op 表与层序规则，`README.md` 索引，`AGENTS.md` 补两条清单坑

验收（走 op 这条路，GL）
- 卡内"无文字区"横向梯度 **0.07** vs 壁纸 **3.43**（模糊生效）
- 标题亮像素 **607**（内容压在玻璃之上）
- 不透明卡区差异 **0 / 161600**（opt-in 保证）
- GL / VK 构建退出码 0、零警告；`check.sh` 全过；命名检查全过

至此阶段 0–4 全部落地。唯一还挂着的验收是 **Switch 真机画面**（需要实机 + nxlink）：
`nxlink -s dist/switch/demo.nro`，开/关玻璃各看一次。

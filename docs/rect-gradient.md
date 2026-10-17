# `rect` 层加渐变 —— 可行性调研与方案

**状态：已实现**（第一步 + 第二步都落地，见文末「已实现」一节）。

结论先给：**可行，而且不用碰任何后端**。渐变做成 `ImDrawList` 的**顶点色网格**，
GL / Switch-GLES3 / Vulkan 三条后端自动都支持（现在那些自绘几何就是这么工作的）。
建议分两步做，第一步约 1 个阶段的工作量。

---

## 1. 现状（读代码得到的事实）

| 位置 | 现状 |
|---|---|
| `bklv/core/appearance.hpp` 的 `AppearanceLayer` | `fill` / `stroke` 都是**单个 `ImU32`**；另有 `strokeWidth` / `radius` / `opacity` / `inset` / `width` / `height` / `align` / `valign` |
| `bklv/core/appearance.cpp` | 解析表里 `{ "fill", &layer.fill }`、`{ "stroke", &layer.stroke }`、`radius` 走长度缩放 |
| `bklv/widgets/appearance_view.cpp` 的 `Op::RECT` | `AddRectFilled(p_min, p_max, scaleAlpha(fill), radius)` + `AddRect(...)` 描边 —— **单色，没有渐变** |
| 现有"渐变"用法 | 只有 demo 壁纸与设计器边框带用 `AddRectFilledMultiColor`（4 角双线性，**不支持圆角**） |
| 自绘几何的先例 | 焦点框用 LUT 贴图 + `AddImageQuad`（纹理路线）；`shadow` 用 `PathRect`+`PathStroke` 描一圈（路径路线） |

ImGui（1.92.9b）能用的手段：

- `AddRectFilledMultiColor(p_min, p_max, 四角色)` —— 直角四角渐变，最简单，但**不能给圆角**。
- `PrimReserve / PrimWriteVtx / PrimWriteIdx / PrimVtx` —— **自己写顶点**（任意网格、每顶点颜色）。
- `ImGui::GetFontTexUvWhitePixel()` —— 自绘形状时用的白色 UV（官方注释就写了这个用途）。
- `AntiAliasedFill` 只作用于 ImGui **自己的**填充图元；手写网格**不会**自动有羽边。

## 2. 三条路线对比

| 路线 | 做法 | 优点 | 缺点 |
|---|---|---|---|
| **A. 顶点色网格（推荐）** | 自己算顶点位置 + 每个顶点的颜色，`PrimReserve` 写进去 | **零纹理、零后端改动**（GL/GLES3/VK 通吃）；线性渐变精确；能带圆角 | 圆角处的抗锯齿要自己补羽边（约 40 行）；多停靠点是分段线性近似（可接受） |
| B. 1D LUT 贴图 + UV | 生成 1×N 的颜色 LUT 纹理，顶点 UV 沿渐变轴映射（和焦点框同一套思路） | 色带最平滑、天然抗锯齿 | 每条渐变一份纹理（或共享一张）、要管纹理生命周期；圆角仍需网格 |
| C. 只用 `AddRectFilledMultiColor` | 直接调现成 API | 一行代码 | **不支持圆角**（而我们的外观几乎都带 `radius`）；任意角度会失真 |

**推荐 A**：它和项目现有"绘制期自绘几何"的经验一致，且**天然跨后端**——
这是选择它最重要的理由（B 要额外管纹理，C 满足不了圆角）。

## 3. JSON 语法草案（尽量贴着现有键的写法）

```json
{
  "id": "bg",
  "op": "rect",
  "radius": 10,
  "gradient": {
    "type": "linear",
    "angle": 90,
    "stops": [
      { "at": 0.0, "color": "#3A60C8" },
      { "at": 1.0, "color": "@theme/color.accent.base" }
    ]
  }
}
```

规则（都按项目既有约定）：

- `gradient` 存在时**以它为准**；`fill` 仍然是"单色底"，状态覆盖（focused 等）继续用 `fill`。
- `type`：`linear`（默认）/ `radial`。枚举走 `enumMatches`，拼错 → **报错**（不静默降级）。
- `angle`：**度**，0 = 从左到右、90 = 从上到下（顺时针），只对 `linear` 有意义。
  无量纲，**不按 uiScale 缩放**。
- `center`：`[x, y]`，0..1 相对层矩形，只对 `radial` 有意义（默认 `[0.5, 0.5]`）。
- `stops`：2~**4** 个（固定容量，层里不额外堆分配）。每个 `{ "at": 0..1, "color": ... }`。
  - `at` 必须**严格递增**且在 `[0,1]` 内，否则报错（带是哪个 stop 的提示）。
  - `color` 支持 `@theme/...`（解析期解析，和 `fill` 同一条路）。
- 与 `import` 合并：`gradient` 是对象 → 逐字段合并；`stops` 是数组 → **整体替换**（和现有数组规则一致）。
- `stroke` 的渐变**这轮不做**（描边要沿路径逐段采样，另算一阶段）。

## 4. 数据模型

```cpp
struct GradientStop { float at = 0.0f; ImU32 color = 0; };

struct Gradient
{
    enum class Type : uint8_t { LINEAR, RADIAL };
    Type  type        = Type::LINEAR;
    float angle       = 90.0f;      // 度
    float center[2]   = { 0.5f, 0.5f };
    GradientStop stops[4];
    int   stopCount   = 0;          // < 2 表示"没有渐变"
};
```

放在 `AppearanceLayer` 里（**定长**，不给每层加堆分配）。**不进 `ALL_PROPS`** ——
渐变不是标量通道，**不做补间**（见第 6 节）。

## 5. 绘制实现（三后端零改动）

新增一个纯函数与一个绘制助手（放在 `core/appearance.cpp` / `widgets/appearance_view.cpp`，
或单独开 `core/gradient.{hpp,cpp}` 便于单测）：

```cpp
// 纯函数：t 处采样（分段线性，t 先钳到 [0,1]）—— 可无窗口单测
ImU32 sampleGradient(const Gradient& gradient, float t);

// 把渐变算成 t：linear 用投影，radial 用归一化距离
float gradientParameter(const Gradient& gradient, const ImVec2& p, const ImVec2& min, const ImVec2& max);
```

绘制分两种情形：

1. **直角（`radius <= 0.5`）**
   - 轴对齐角度（0/90/180/270）：4 个顶点 + 2 个三角形，每顶点采样 → **精确**（线性插值正好等于渐变）。
   - 斜角：切成 **8×8** 网格逐顶点采样 → 误差可忽略（也可以直接调 `AddRectFilledMultiColor` 做轴对齐的快捷路径）。
2. **圆角（`radius > 0.5`）**
   - 沿圆角矩形边界取样（每个圆角 8 段，共约 40 个边界顶点），再加一个中心顶点 → **三角扇**
     （凸多边形，扇形填充）——每顶点按位置采样。
   - 线性渐变在扇形上是**精确**的（颜色是位置的线性函数，线性插值不变）；径向是分段近似，
     分段数取够即可（8/角 已看不出差别）。
3. **抗锯齿**（第二步做）
   - 手写网格没有羽边。按 ImGui `_AddConvexPolyFilled` 的做法，沿边界再生成一圈**外扩一个羽边宽度、
     alpha=0** 的顶点（羽边宽度 = `1.0 * _FringeScale`），与内圈顶点连成三角形带即可。
   - 第一步可以先不做：圆角处会略有锯齿（直角 + 轴对齐完全不需要 AA）。

依赖只有三个公开 API：`GetFontTexUvWhitePixel()`、`PrimReserve`、`PrimWriteVtx/PrimWriteIdx`。

## 6. 与状态动效、主题的关系（要提前说清的取舍）

- **渐变是静态样式，不做补间**：`states` 目前只能改 `AppearanceProp` 里的标量/颜色通道，
  换不了"整个 gradient 对象"。所以：
  - 状态想变淡 → 用 `opacity`（已有）✓
  - 状态想换一种渐变 → **这轮不支持**（要么后续给 states 加"对象字段覆盖"，
    要么把想要的两种渐变做成两个 layers 切换）——写进文档，不装作支持。
- **与玻璃共存**：`glass` 层由解析期提到最前，渐变层照常排在它后面 ✓ 互不影响。
- **两个清单坑照旧**：新增主题文件要写进 `theme.json` 的 `files[]`（带 `.json` 后缀），
  新增键要补进 `docs/json-syntax.md` 与 `theme_test` 的遍历校验。

## 7. 分两步、工作量与验收

**第一步（直角 + 任意角网格 + 解析 + 单测 + 文档）——约 250 行**
- `gradient.{hpp,cpp}`：`GradientStop/Gradient` + `sampleGradient` + `gradientParameter`
- `appearance.*`：解析 `gradient`（含 stops 校验与报错）
- `appearance_view.cpp`：直角情形的网格绘制
- `tests/gradient_test.cpp`：采样端点/中点/钳制/多段、解析校验（stops 数、递增、越界报错）
- `docs/json-syntax.md` + `rect-gradient` 小节；`check.sh` 项数随之上涨

**第二步（圆角 + AA 羽边 + 主题示例 + 像素验收）——约 200 行**
- 圆角三角扇 + 羽边；demo 里加一份内联的渐变卡外观（`base/rectangle.card.gradient` 已按要求从主题撤出）
- **像素验收**（GL，沿用现有套路）：90° 渐变卡上端像素 ≈ stop0、下端 ≈ stop1（容差 ≤2/255）、
  中间单调过渡；圆角处无锯齿（人眼+边缘像素检查）
- Switch 顺带看一眼（同一份顶点，理论上不需要额外代码）

**明确不做（本轮）**：`stroke` 渐变、锥形/角度渐变、超过 4 个停靠点、渐变补间动画。

## 8. 风险

| 风险 | 说明 | 对策 |
|---|---|---|
| 圆角锯齿 | 手写网格没有羽边 | 第二步补羽边；第一步先用 1px 描边过渡 |
| 径向近似 | 扇形内线性插值 | 分段数取够（8/角）；必要时加"环"细分 |
| 顶点数增加 | 每层几百个顶点 | 可忽略；先不做缓存（过度设计） |
| 与 `fill` 语义冲突 | 两种底色并存 | 明确规则：写了 `gradient` 以它为准，`fill` 仍服务状态覆盖 |


---

## 9. 已实现（本次落地）

| 文件 | 内容 |
|---|---|
| `bklv/core/gradient.{hpp,cpp}` | `GradientStop` / `Gradient`、`sampleGradient`、`gradientParameter`、`gradientTypeFromName`、网格生成 `addGradientRectMesh` |
| `bklv/core/appearance.{hpp,cpp}` | 层里加 `Gradient gradient`；解析 `gradient`（`type`/`angle`/`center`/`stops`，五类错误全部报错） |
| `bklv/widgets/appearance_view.cpp` | `Op::RECT`：有渐变就走网格，没写仍是原来的单色 `AddRectFilled` |
| `demo/app/pages/page_base_widgets.cpp` | 示例外观**内联在页面里**（蓝→紫 90° 渐变卡，`applyAppearanceJson`）：默认主题是扁平风格，能力示例不占随机发布的主题资源 |
| `tests/gradient_test.cpp` | **55 项**：采样（端点/中点/多段/钳制/无效）、参数（0°/90°/45°/径向）、枚举名、解析正例与五类反例、网格（顶点数/角点颜色/圆角/羽边/整体 alpha） |

实测：GL / VK / Switch 三个构建退出码 0、本仓零警告；`check.sh` **3108** 项全过（+61）；
命名检查 169 文件。

过程中单测抓到一个真错：线性渐变的投影范围**必须按矩形四个角的投影取 min/max** ——
最初用 `width*dirX` 与 `height*dirY` 分开算，45° 时 t 会跑到 1.5。

## 10. 仍然不做

- `stroke` 的渐变（要沿路径逐段采样）
- 锥形/角度渐变、超过 4 个停靠点
- 渐变的补间动画（`states` 只能改标量通道；想换渐变就做两个 layer，或等后续给 states 加对象覆盖）

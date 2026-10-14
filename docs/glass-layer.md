# 液态玻璃（外观层 `op: "glass"`）

一句话：**视图声明"这块要玻璃"，玻璃画在它下面，它自己的内容压在玻璃上面。**
视图不写任何玻璃代码，样式由外观 JSON 决定，开关与帧级参数由 `Shell` 决定。

配套阅读：[`glass-layer-plan.md`](glass-layer-plan.md)（计划书与验收记录）、
[`glass-layer-feasibility.md`](glass-layer-feasibility.md)（调研与取舍）。

---

## 1. 三个层次：谁决定什么

| 层次 | 位置 | 决定 |
|---|---|---|
| **开关 / 档位 / 帧级参数** | `Shell`（转发到 `glass::GlassService`） | `setGlassEnabled` / `setGlassQuality` / `getGlassTuning`（blurRadius、折射、色散、染色、光照）；§4 的能力查询 |
| **样式**（每块玻璃可以不同） | 外观 JSON 的 `op: "glass"` 层 | `fill`（染色，带 alpha）/ `opacity` / `radius`（圆角）/ `inset` 与 `width`/`height`（几何） |
| **要不要玻璃** | 视图所引用的外观 | 没写 `op: "glass"` 层就没有玻璃（**opt-in**，一个像素都不会被影响） |

**优先级：外观 JSON 写了就用 JSON，没写才用 `Shell` 的 `GlassTuning`。**
`enabled`、`quality`、`blurRadius` 是**帧级**的（两条后端的"这一帧的背景模糊"是整帧共用一份），
所以它们只由 `Shell` 决定 —— 逐层改模糊半径在当前架构下做不到，不装作支持。

## 1b. demo 里怎么看（基础控件页）

**不另开页面。** 基础控件页（demo 的「基础」页）里，**每个控件都有一行液态玻璃版本**：

- 有"自己的面"的控件（按钮一族 / 开关 / 下拉触发器 / 选择器）→ 用它自己的 `glass` 变体
  （主题里 `<外观>.glass`，如 `widget/button.text.glass`），底换成玻璃，文字/图标不变；
- 没有面的控件（文字 / 图片 / 滑块 / 进度圈）→ 放在一块**玻璃板**上（`base/glass_plate`），
  读作"这个控件在液态玻璃里"。

两种写法的共同点：变体文件只需 `import` 基外观 + 覆盖底色 + 加一层 `op: "glass"`：

```json
{ "import": "widget/button.text",
  "layers": [ { "id": "bg", "fill": "#00000000", "stroke": "#00000000" },
              { "id": "glass", "op": "glass", "fill": "#10203A6E" } ] }
```

页面上还有两个"为了看得见玻璃"的布置（都是 demo 自己的选择，不是库的要求）：
小节卡片用半透明玻璃卡（`base/rectangle.card.glass`）；壁纸铺在**页面**的第一层
（Shell 的内容区是不透明的，铺在背景层看不见）。玻璃采的是"这一帧已经画好的内容"，
所以壁纸必须先画、且要有高频细节（斜条纹）。

## 2. 怎么写

主题里加一份外观（`resources/theme/<主题>/base/glass_panel.json`）：

```json
{
  "box": { "radius": "@theme/metric.component.radius", "padding": [14, 16, 14, 16] },
  "layers": [
    { "id": "glass", "op": "glass", "radius": "@theme/metric.component.radius",
      "fill": "#0A0F1E57" },
    { "id": "title", "op": "text", "source": "title",
      "color": "@theme/color.text.primary", "fontSize": "@theme/metric.font.size.sm",
      "align": "left", "valign": "top" }
  ]
}
```

视图这边只要给文字：

```cpp
class GlassPanelView : public AppearanceView
{
  public:
    GlassPanelView() : AppearanceView("base/glass_panel", false) { }
  protected:
    std::string getLayerText(const std::string& source) override
    {
        return source == "title" ? this->title : std::string();
    }
};
```

三条硬要求：

1. **`glass` 层永远画在最前**（解析期就把 `Op::GLASS` 提到队首）——所以主题里用
   `import` 基外观 + 追加一层 `glass` 的写法是安全的，不用关心书写位置；
   玻璃画在这一层的位置上，后面的内容层自然压在它上面。
2. **背景要先画**：玻璃采的是"这一帧到此刻已经画好的内容"，背景在后面 = 采到一片空。
   背景还必须有**高频细节**（斜条纹之类）与足够亮度：纯渐变 + 深色底上，玻璃几乎看不出来。
3. **关掉玻璃时**玻璃层什么都不画（不退回自算的颜色）—— 想让面板在这种时候仍可读，
   在视图的 `draw()` 里补一块回退底色（`demo/app/pages/page_glass.cpp` 就是这么做的）。

## 3. 后端矩阵

| 后端 | 实现 | 取舍 |
|---|---|---|
| 桌面 GL 3.2 / GLSL 150 | 抓帧 → 半分辨率可分离高斯（双线性配对采样）→ 折射/色散/染色合成 | 功能最全；每帧抓一次、当帧所有玻璃面共用 |
| Switch GLES3 / GLSL ES 3.0 | **同一份实现与正文着色器**，只有版本头不同（`#version 300 es` + `precision`）；GL 入口直接取地址 | 与桌面一致 |
| Vulkan（路线 A） | UI 通道前后两个钩子：`vkCmdBlitImage` 顺 mip 链下采样出模糊底，圆角/染色/高光由 ImGui 叠上去 | 背景是**上一帧**（滞后一帧，静态界面看不出）、**没有折射/色散**；能力查询里 `refraction` / `dispersion` 如实报 false |

拿不到 GL 入口 / 着色器编不过 / 尺寸非法 → 整体关闭玻璃（`isGlassSupported()` 为 false），
上层按"没有玻璃"处理。

## 4. 接口

```cpp
// 所有者（宿主用这个）
shell.setGlassEnabled(true);
shell.setGlassQuality(glass::GlassQuality::BALANCED);
shell.getGlassTuning().blurRadius = 32.0f;
if (shell.isGlassSupported()) { /* 才把开关暴露给用户 */ }

// 视图想自己声明（等价于 op: glass，给非外观路径用）
glass::GlassService& service = glass::getGlassService();
service.submitBackdrop(drawList, pixelBounds, radii, shape, density, &options);

// 外观层内部用的就是这一条 —— 别自己填 drawCommandOffset（见 §5）
```

## 5. 三个坑（都踩过，写在 `AGENTS.md` 里了）

1. **注入点的偏移不是"真边界"**：ImGui 会把连续图元合并进同一条命令，
   直接记 `CmdBuffer.Size` 会让调用方自己的文字被并进前一条命令，玻璃插在后面把它盖住。
   → 用 `GlassService::submitBackdrop()`，它先 `AddDrawCmd()` 切一条新命令，偏移取 `Size - 1`。
2. **GL 状态守卫要包"一次操作"，不能包"整段回调"**：`FramebufferGuard` 包住整个绘制回调时，
   抓帧 + 模糊留下的离屏 FBO 一直绑着，`composite` 被画进离屏 FBO，屏幕上什么都不出现。
   → 守卫只包 `prepareBackdrop`，合成在 FBO 恢复之后执行。
3. **壁纸必须在主绘制列表的最前面**（不是 `GetBackgroundDrawList()`）：抓帧那一刻帧缓冲里
   只有菜单栏、没有壁纸 → 玻璃采到一片黑。

## 6. 验收方式与实测数据（阶段 1，GL）

开/关玻璃各截一张（`BKLV_DESIGNER_GLASS=0` 或临时关开关），逐像素比对：

| 项目 | 结果 |
|---|---|
| 玻璃卡区差异（开 vs 关） | **99.7%**（161156 / 161600） |
| 卡内"无文字区"横向梯度 vs 壁纸 | **0.07 vs 3.43**（模糊把条纹抹平） |
| 标题亮像素 | **2236**（内容压在玻璃之上；修好前是 0 = 被盖住） |
| 不透明卡区差异 | **0 / 161600**（opt-in 保证：没声明就不受影响） |
| 模糊贴图本身（像素回读） | 进模糊前 2.18 → 出模糊后 **0.28** |
| 设计器画布内容区 | **0 像素差**（回归） |

外观层 `op: "glass"` 这条路径单独复测过：卡内 **0.07** vs 壁纸 **3.43**、标题亮像素 **607**、
不透明卡 **0 / 161600**。

VK 那条路没有 `App::requestScreenshot`（截图是 GL 侧实现的），所以 VK 的验收是
"构建 + 真跑 + 日志 + 无 validation 报错"，像素断言仍在 GL 侧（着色与合成逻辑共用）。

## 7. 文件地图

| 文件 | 作用 |
|---|---|
| `bklv/glass/glass_style.hpp` / `glass_request.hpp` / `glass_blur_plan.*` / `glass_tuning.*` / `glass_geometry.*` | 思路层：参数、请求、模糊计划、形状解析（纯逻辑，无平台依赖） |
| `bklv/glass/glass_queue.*` | 一帧的请求队列（只收 opt-in 的声明，不做裁剪） |
| `bklv/glass/glass_service.*` | 服务：帧首/提交/注入/帧末；`submitBackdrop`；能力与开关 |
| `bklv/backends/gl_glass.{hpp,cpp}` + `gl_glass_procs.*` + `gl_glass_shaders.*` | GL / GLES3 后端（抓帧 + 高斯 + 合成） |
| `bklv/backends/vk_glass.{hpp,cpp}` | Vulkan 路线 A（mip 链模糊 + ImGui 叠染色） |
| `bklv/core/focus_ring.*` | **无关**，但形状表达（圆角矩形 + 半径钳制）与玻璃共用同一套思路 |
| `resources/theme/default/base/glass_panel.json` | 玻璃面板外观（`op: "glass"` 的示例） |

# 液态玻璃移植评估（只用于设计器界面）

对象：`developing/imgui-liquid-glass`（ImGui + Android，Vulkan / OpenGL ES 双后端）
目标：把它的玻璃效果与效果思路搬进本仓库的 `designer/`，**只服务设计器自己的界面外壳**，不进 `bklv/` 库。

**边界（硬要求）**：**预览区不加玻璃、也不受玻璃影响** —— 那里渲染的是主题 JSON 的真实效果，
是用户要看的东西，不能被模糊/染色/高光/色散改变一个像素。玻璃只作用于外壳面板：
文件列表、节点树、属性表、顶栏、菜单栏。

## 一、结论

**可行**，建议只做 **GL 路径**（设计器在 macOS 上就是 `--preset gl`），VK 路径不接、自动降级成现在的实心面板。

理由：真正与平台绑定的只有两个后端目录（GL ~800 行 / VK ~1400 行），
思路层（Style / Request / Queue / BlurPolicy，约 200 行）零依赖可直接拿；
而 bklv 的 ImGui GL 路径就是 `ImGui_ImplOpenGL3_RenderDrawData`，
玻璃靠 `ImDrawCmd::UserCallback` 注入 —— **不用改 bklv 的任何后端**。

## 二、移植什么 / 不移植什么

| 部分 | 行数 | 处置 |
|---|---|---|
| `effects/glass/`（Style / Request / Queue / BlurPolicy / EffectCapabilities） | ~200 | 原样搬进 `designer/glass/` |
| `effects/glass/backend/opengl/` | ~800 | 改造后搬（见下） |
| `effects/glass/shaders/opengl/`（4 个 GLSL） | — | 改版本号后搬 |
| `effects/glass/backend/vulkan/` + vulkan shaders | ~1400 | **不搬**（需要动 bklv VK 后端，单独排期） |
| `ui/widget/`（控件层） | 7561 | **不搬**：绑 Android 的 `hridhi`/`toucher` + EGL + 自成一套布局/弹簧动画，与 bklv 视图系统重复；设计器界面本来就是 ImGui 控件 |

## 三、具体改动清单

1. **思路层**：`GlassStyle` / `GlassRequest` / `GlassRequestQueue` / `GlassBlurPolicy` /
   `EffectCapabilities` 原样复制（无平台调用）。

2. **GL 后端改造**（`OpenGLGlassBackend/Resources/BlurPass/DrawCallbacks`）
   - `ensure(EGLContext, w, h)` → 改成**单上下文**（设计器只有一个窗口/GL 上下文），
     删掉按 EGLContext 键的资源表与 `glGetCurrentContext()`（桌面 GL 没有这个调用）。
   - 着色器 `#version 300 es` + `precision highp float;` → `#version 150`（macOS Core 3.2 上限）。
   - `glTexStorage2D`（GL 4.2）→ `glTexImage2D`（macOS 只到 4.1）。
   - 保留 `glBlitFramebuffer` + `glReadBuffer(GL_BACK)` 抓背景（GL 3.0+ 可用）。
   - 状态保存/恢复沿用它的做法（`glScissor`/`glViewport`/`glBlendEquation`/`glBlendFuncSeparate`/
     `glColorMask`），避免污染 ImGui 的 GL 状态。

3. **GL 函数加载（必须补的一块）**
   bklv 用的是 ImGui 自带精简 loader（`bklv/3rd/imgui/backends/imgui_impl_opengl3_loader.h`），
   只覆盖 ImGui 自己要用的入口 —— **没有** `glGenFramebuffers` / `glBindFramebuffer` /
   `glFramebufferTexture2D` / `glBlitFramebuffer` / `glCheckFramebufferStatus` /
   `glGenVertexArrays` / `glDrawArrays` / `glUniform*` / `glGetUniformLocation` /
   `glGetProgramiv` / `glGetShaderiv` 等约 30 个。
   → 需要按现有 `bklv/backends/gl_loader.cpp` 的做法新增一个桌面 GL 加载器（设计器内部用）。

4. **设计器接入**（`designer/ui/designer_window.cpp`）
   - 外壳面板（文件列表 / 节点树 / 属性表 / 顶栏 / 菜单栏 / 弹窗）：绘制前把矩形 + 圆角
     塞进 `GlassRequestQueue`，带 `drawCommandOffset` 记录注入位置。
   - **预览区不入队**，而且每个请求矩形都要与预览矩形求交后**裁掉重叠部分**（`ImVec4` 的
     clipRect 也一并裁）—— 面板贴边、悬浮或改布局时都不会糊到预览上。
   - `ImGui::Render()` 之后照抄 `OpenGLGlassDrawCallbacks::inject(drawData, queue, backend)`。
   - 接入点在 `App::onDrawWindows`（设计器已经把界面画在那里）。

5. **回退与开关**
   - `EffectCapabilities` 原样：拿不到 FBO / 着色器编译失败 → 玻璃全部不生效，面板回到现在的样子。
   - 设计器自己的开关（环境变量，符合"库与产品程序不留调试开关"的约定 —— 设计器是开发工具）。

## 四、工作量估计

| 项 | 估计 |
|---|---|
| 思路层搬运 | 0.5 天 |
| GL 后端改造（shader 版本 / 纹理 API / 单上下文） | 2–3 天 |
| GL 函数加载器扩展 | 0.5–1 天 |
| 设计器接入 + 回退 | 1 天 |
| 调参（模糊档位、染色、折射、色散、边缘高光、密度） | 1–2 天 |
| **合计** | **约 5–8 天** |

## 五、风险与坑（按严重度）

1. **采样时序 + 预览禁区**：玻璃采的是"已经画好的内容"。设计器里 bklv 预览与 ImGui 面板同帧绘制，
   面板还会互相重叠 → 必须固定绘制顺序；`GlassBackdropSource::PreviousContent` 只能看到先画的内容。
   另外**预览区的像素必须完全不被玻璃影响**，落地时：
   ① 预览矩形从所有玻璃请求里裁掉（含 clipRect）；② 玻璃面板不要覆盖预览；
   ③ 验收做成断言：开/关玻璃各截图一次，**预览区逐像素一致**（可以直接用设计器自己的
   `designer/core/image_diff.*` 比）。
2. **Retina / 密度**：`density` 必须传 framebuffer scale（Retina 是 2x），否则模糊半径与
   折射偏移差一倍（本仓库在截图/设计器上踩过同类坑）。
3. **macOS GL 上限 4.1**：4.2 的接口一律不能用（`glTexStorage2D` 已列）。
4. **无 EGL**：删掉上下文键反而简化资源管理；但要自己保证 GL 上下文在玻璃调用时是当前的。
5. **许可证**：着色器注明 "Adapted from AndroidLiquidGlassView, MIT" —— MIT 可移植，
   保留版权声明即可商用。
6. **边界**：这套东西**只放 designer/**。库要保持零新依赖 + Switch 可编，
   玻璃（模糊/FBO/多趟 pass）也不适合塞进 `bklv` 的绘制层。

### 附：玻璃要有东西"可采"才看得出来

设计器当前面板底是深灰纯色，纯色背景上玻璃≈看不出效果。要在外壳上体现材质，
需要给设计器**最底层铺一张背景**（渐变、噪点或一张壁纸图），玻璃面板浮在它上面；
这张背景**只在面板区域可见**，与预览区无关。

## 六、落地进度

### 第 1 步已完成（GL 路径）

已实现：底图壁纸 + 菜单栏 / 状态栏 / 左栏 / 右栏玻璃，预览区为禁区。

```
designer/glass/glass_style.hpp        玻璃参数（面/模糊/光照/染色）
designer/glass/glass_request.hpp      一次玻璃请求 + EffectCapabilities
designer/glass/glass_queue.{hpp,cpp}  一帧的请求队列 + 禁区裁剪
designer/glass/glass_blur_plan.{hpp,cpp} 双线性配对可分离高斯计划（半分辨率）
designer/glass/gl/gl_procs.{hpp,cpp}  桌面 GL 入口（SDL_GL_GetProcAddress，约 45 个）
designer/glass/gl/gl_shaders.{hpp,cpp} GLSL 150 源码（公共模块内联，无构建期工具）
designer/glass/gl/gl_backend.{hpp,cpp} 抓背景 / 模糊 / 合成 / 注入 draw data
designer/ui/designer_glass.{hpp,cpp}  设计器门面（开关、能力、注入钩子）
```

落地顺序（原计划）与现状：
1. ~~顶栏 + 菜单栏玻璃~~ ✅ 顺带把左栏 / 右栏也做了（面板底改透明，玻璃才看得见）
2. 逐个面板细调（圆角/染色/折射强度、滚动与重叠时的表现）→ 待做
3. 玻璃调参面板（模糊档位/染色/折射/色散/高光）→ 待做
4. VK 路径 → 待排期（要动 bklv 的 VK 后端）

### 验收数据（GL，2560×1640 帧缓冲 / 1280×820 逻辑）

- `BKLV_DESIGNER_SHOT` 开关玻璃各截一张，逐像素比对：
  · **预览面板差异 0 像素**、**预览画布差异 0 像素**（硬要求达标）
  · 左栏 / 右栏 / 顶栏按预期整块变化（玻璃生效）
- 玻璃后端日志：`[玻璃] 后端就绪：2560x1640（模糊 1280x820）`、`[玻璃] 已启用（32 块以内）`

### 第 2 步（本轮）：调参面板 + 外壳统一

- `designer/glass/glass_tuning.hpp`：半径/质量档/染色/折射/深度/色散/对比/四项光照/
  两类圆角/横条外阴影开关 —— 唯一数据源，界面上滑块直接改它
- 工具菜单「玻璃调参」+ `F4`；面板自己也是玻璃；参数随 `designer_state.json` 存
- 日志浮窗也上玻璃；浮窗底给 0.35 alpha（玻璃在预览区会被裁掉，那块要有底色）
- 抽查：**调参面板压住画布时，画布区域逐像素差异仍是 0**（禁区裁剪对浮窗同样生效）

### 仍然待办（下一步）

- **Vulkan 路径**：要动 bklv 的 VK 后端（捕获交换链 → 模糊 pass → 合成 pass +
  命令缓冲/屏障/描述符），按项目纪律单独排期；GL 之外先整体降级（已实现）
- 面板逐个细调（滚动、重叠、拖拽时的表现）
- 玻璃参数预设（几套命名观感，一键切）与"导出到主题 JSON"
3. 玻璃调参面板（模糊档位/染色/折射/色散/高光）做成设计器自己的 ImGui 面板，方便迭代审美
4. 最后再单独评估 VK 路径（要动 bklv 的 VK 后端：捕获交换链 → 模糊 → 合成）

## 七、还没有确认的

- 没有真机/实机渲染验证（本评估基于读代码 + 结构判断）
- VK 路径只做了代码结构判断，没有实测；接它需要改 bklv VK 后端（AGENTS 里明确那条路径
  "故意不共用、动它风险更大"）

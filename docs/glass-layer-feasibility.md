# 调研：把液态玻璃做成 bklv 的一层，用 Shell 接口配置

问题：设计器里的液态玻璃效果，能不能作为 bklv 的一层加进库里，通过 `Shell::` 接口设属性？

**结论：可行，而且架构上很合适** —— 但"一层"要理解成**外观层（appearance layer）**，不是
Shell 层栈里的一层。下面是把设计器那套搬进库需要动的东西、API 草案、六个坑与分阶段计划。

---

## 一、现状盘点（设计器里已经有的家底）

| 部分 | 位置 | 行数 | 有没有平台依赖 |
|---|---|---|---|
| 思路层：Style / Request / Queue / BlurPlan / Tuning / Capabilities | `designer/glass/*.hpp|*.cpp` | 736 | **零依赖**（只用 ImGui 类型） |
| GL 后端：抓帧 + 可分离高斯 + 折射/色散合成 + 自带的 GL 函数加载 | `designer/glass/gl/` | ~1873（含 VK） | GL 3.2 Core + GLSL 150 |
| VK 后端：`vkCmdBlitImage` 顺 mip 链模糊 + 两个 `VulkanUiPassHook` | `designer/glass/vk/` | 同上 | Vulkan |
| 门面：开关 / 调参 / `addBar` / `addPanel` / `excludeRect` / `endFrame` | `designer/ui/designer_glass.cpp` | 278 | 薄 |

关键事实（决定了可行性的上限）：

1. **接入点库里已经有了**：`App::onPrepareDrawData`（`ImGui::Render()` 之后、`renderDrawData()` 之前），
   设计器就是在这里注入玻璃命令的 —— 这条钩子当初就是为玻璃加的，库里没有任何玻璃代码。
2. **GL 路径不用改库后端**：玻璃靠 `ImDrawCmd::UserCallback` 插进 `ImDrawData`
   （设计器那份移植评估里的原话："不用改 bklv 的任何后端"）。
3. **VK 路径也不用改库后端**：它走库里已经有的 `VulkanUiPassHook`（UI 通道前铺底+模糊+合成、
   通道后把成品留给下一帧），通道用 `LOAD_OP_LOAD`。
4. **能力位现成**：`EffectCapabilities{ backdropCapture, blur, refraction, dispersion, maxGlassRegions }`
   —— 拿不到就整体回退成实心面板，不会让程序不能用。
5. **形状已经是"矩形+圆角"那一套**：`GlassRequest` 有 `minMax + CornerRadii + GlassShapeKind`
   （圆角矩形 / 胶囊 / 圆）—— 和焦点框同一套表达，可以共用"半径钳到短边一半"那段逻辑。

## 二、"作为一层"是什么意思：两种 layer，只有一个合适

| 方案 | 合适吗 | 为什么 |
|---|---|---|
| 塞进 **Shell 层栈**（backdrop / content / overlay / hud / debug） | **不合适** | 玻璃不是"画自己内容的视图"，它是**对已经画好的像素做后处理**（要抓帧+模糊）。塞成一层解决不了"采样背景"，还会带来层内顺序/裁剪的新问题 |
| 做成**外观层 op**（`layers: [{ "op": "glass", ... }]`） | **合适** | 一块"玻璃面板"就是某个控件外观里的一层，和 `rect` / `shadow` / `custom` 平级；"哪些面板是玻璃、圆角多少"由 JSON 声明，"模糊/染色/折射参数"由 Shell 统一给 —— 与"装饰层走 JSON"的既有约定一致 |

于是所有者与职责划分**和刚做完的焦点框一模一样**（同一套单向依赖）：

| 角色 | 职责 |
|---|---|
| 控件（外观层） | 只声明"我这一层是玻璃 + 我的形状"（矩形/圆角半径，圆形控件自动变圆） |
| 库的服务（`bklv/glass/`） | 每帧收集请求 → 排模糊计划 → 交给后端注入（一帧只抓一次背景） |
| 顶层外壳（Shell） | **所有者**：全局开关 / 性能档位 / 参数 / 能力查询 |
| 后端（GL / VK） | 抓帧、模糊、合成；能力不足就整层关闭 |

## 三、要动库的哪几处（迁移为主）

| 要做的 | 内容 | 估算 |
|---|---|---|
| ① 思路层搬进 `bklv/glass/` | Style / Request / Queue / BlurPlan / Tuning / Capabilities 原样搬（零依赖）+ 无窗口单测（队列、模糊计划、圆角形状） | ~250 行 + 测试 |
| ② GL 后端搬进 `bklv/backends/` | `gl_glass.*` + 着色器（GLSL 150 常量字符串，不引构建期工具）+ **补 GL 函数入口**（见坑 5） | ~800 行 |
| ③ VK 后端搬进 `bklv/backends/` | `vk_glass.*`，复用现有 `VulkanUiPassHook`；保留"背景是上一帧"的既有取舍 | ~600–1400 行 |
| ④ 库侧服务 + 接线 | `GlassService`：收集请求 → `onPrepareDrawData` 时调后端注入；帧尺寸/密度的取用 | ~200 行 |
| ⑤ Shell API + 视图声明 + op 解析 | 见下面草案；`AppearanceView` 解析 `op: "glass"` 并入队 | ~250 行 |
| ⑥ demo 页 + 验收 | 一个玻璃面板页；开/关各截一张做像素断言 | ~200 行 |

**合计约 2300–3100 行**（大头是搬运 + 去掉设计器特有假设 + 改名）。

## 四、Shell API 草案

```cpp
// ---- 全局：所有者是外壳，和焦点框一致 ----
void  Shell::setGlassEnabled(bool);              // 一键关（退回实心面板）
bool  Shell::isGlassEnabled() const;
void  Shell::setGlassQuality(glass::Quality);    // FAST(半分辨率) / SMOOTH(全分辨率)
glass::GlassTuning& Shell::getGlassTuning();     // 模糊/染色/折射/光照/圆角（唯一数据源）
glass::EffectCapabilities Shell::getGlassCapabilities() const;  // 后端能力，UI 据此灰掉开关
bool  Shell::isGlassSupported() const;

// ---- 视图侧：只声明形状，不认识玻璃的实现 ----
// View::getGlassBackdrop() 默认返回"无效"（= 不要玻璃），子类/外观覆盖才有
virtual glass::GlassRequest View::getGlassBackdrop() const;
```

外观写法（形状与强度按层声明，参数由 Shell 统一给）：

```json
{ "op": "glass", "id": "glass",
  "radius": "@theme/metric.component.radius",
  "strength": 1.0 }
```

## 五、六个必须提前想清楚的坑（都有实测依据）

1. **采样时机**：玻璃采的是"这一帧到此刻为止**已经画好**的内容" → 背景/壁纸必须先画。
   库侧的天然落点就是 Shell 的 **backdrop 层**（它本来就在最前面）。
   ⚠️ 设计器踩过：抓帧那一刻如果背景还没画，采到一片黑，面板就是黑的（排查花了两轮）。
2. **不能糊到"必须是原像素"的区域**：设计器是 `excludeRect`（因为它的面板是 ImGui 窗口，覆盖面积大）。
   库侧**改用 opt-in**：只有声明了玻璃的视图才入队 → 不需要"禁区"概念，也不会误伤
   （比如 demo 里渲染主题 JSON 的画布区，必须逐像素不受影响）。
3. **VK 有一帧滞后**：VK 那条路没有自定义 shader（靠 mip 链 blit），背景是**上一帧**的画面，
   而且**没有折射/色散**（只有 GL 有）。这条要写进接口注释与文档，否则会被当成 bug 报。
4. **GL 状态必须还回去**：抓帧/模糊画在离屏 FBO 上，framebuffer 绑定是全局状态 ——
   不恢复的话后面所有 ImGui 命令都画进离屏 FBO（设计器踩过：整屏四分之三变透明）。
   设计器用 `FramebufferGuard` + 玻璃命令后插一条 `DrawCommand_ResetRenderState` 兜住。
5. **GL 函数入口**：macOS/Linux 用系统头就有 GL 3.2 Core 原型（不需要 loader）；
   **Windows** 只有 GL 1.1，走 imgl3w，而它**缺约 30 个入口**（`glGenFramebuffers` /
   `glBlitFramebuffer` / `glDrawArrays` / `glUniform*`…）→ 库侧要按
   `bklv/backends/gl_loader.cpp` 的做法补一张 `SDL_GL_GetProcAddress` 表。
6. **Switch / GLES3**：Switch 走的是 `gles3_backend`（`BKLV_GLES3` / `BKLV_SWITCH`），
   GLSL 150 的着色器在 GLES3 上编不过（要 `#version 300 es` 变体 + GLES3 的加载器）。
   → 建议**先明确"Switch 不支持，自动降级成实心面板"**，别为它拖住整个特性。

## 六、分阶段计划

| 阶段 | 内容 | 验收 |
|---|---|---|
| 1 | 思路层搬进 `bklv/glass/` + 单测（队列/模糊计划/形状） | `check.sh` 全过，新测试覆盖纯逻辑 |
| 2 | GL 路径打通（GL 后端 + 补加载器 + 服务 + Shell API + op 解析） | demo 出一块玻璃面板；开/关各截一张：**不该受影响区域逐像素一致**；玻璃区域像素标准差明显 > 0（之前实测：不透明面板 0.00、玻璃 3.65） |
| 3 | VK 路径（复用 `VulkanUiPassHook`），文档写明一帧滞后 | 双后端零警告 + 校验层静默 + 截图对比 |
| 4（可选） | GLES3/Switch 变体、更多形状（多边形/自定义轮廓） | 真机验证 |

**建议先做 1+2**：GL 覆盖桌面三平台的主要使用场景，风险最低；VK 与 Switch 各自单独排期。

## 七、需要拍板的四件事

1. **先只做 GL**（推荐）还是 GL+VK 一起接？
2. **Switch 明确不支持**（自动降级）可以吗？
3. 参数放哪：**Shell 的 `GlassTuning` 统一给 + 外观层只写"是不是玻璃/圆角/强度"**（推荐，同设计器）
   还是每层都能写全套参数？
4. 默认档位：**半分辨率 + 均衡质量**（推荐，性能稳）还是全分辨率？

## 八、明确不建议的三件事

1. **别把玻璃塞进 Shell 的层栈**：它是对已画像素的后处理，不是"一层视图"。
2. **别用 `RenderBackend::readPixels()` 做 CPU 抓帧**：那是截图用的同步回读，每帧做会把帧率打穿。
3. **别一次把三条后端（GL/VK/GLES3）都做完**：GLES3 的着色器变体与加载器是纯新增工作量，
   而且 Switch 真机验证链路长，会把特性拖住。

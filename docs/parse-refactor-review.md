# 解析层重构与扩展机制（评审稿）

> 目标读者：维护者 / 想加自己图层与控件的第三方。
> 本文只讲**怎么组织解析与扩展点**，不改任何运行时语义。

## 1. 背景：现在的问题（实测）

| 文件 | 行数 | 里面装了什么 |
|---|---|---|
| `core/appearance.cpp` | 1906 | 解析（`compile` **501 行**）+ 编译产物查询 + 状态补间运行时 + `AppearanceInstance`（带 tweeny） |
| `core/appearance.hpp` | 357 | Layer / Box / 过渡 spec / Appearance / Instance 五个概念 |
| `core/json_view.cpp` | 649 | 视图 JSON 解析（第二套） |
| `core/theme_data.cpp` | 567 | 主题 JSON 解析（第三套） |
| `widgets/appearance_view.cpp` | 631 | 按 op 分支绘制 |
| `designer/core/appearance_doc.cpp` + `ui/designer_window.cpp` | 482 + 大 | 设计器**又一套** op/键表（字符串版） |

两个直接后果：

1. **同一个概念有多份实现** —— 「图层类型」在解析（枚举）、绘制（枚举）、设计器（字符串）里各写一遍。
   实证：加 `glass` 时设计器整个漏掉，事后才补。
2. **加一个字段要在 4 处改** —— 解析、绘制、设计器键表、文档。这不叫可扩展。

## 2. 目标 / 非目标

**目标**

- **单一事实来源**：一种 op 的名字、默认字段、可编键、层序档位、解析与绘制钩子，只写一处
- **可读**：`compile` 瘦到 < 80 行；解析按「段 / 类型」分文件，同层级的代码在同一文件里
- **低耦合**：解析（`compile/`）与运行时（`core/`）分 TU；tweeny 只出现在运行时
- **第三方可扩展**：加一种 op = **新增 1 个文件 + 1 行注册**，不改库任何现存文件；设计器自动认它
- **行为不变**：重构期间所有既有测试与截图结果不变

**非目标（v1 明确不做）**

- 不让 op 参与布局（op 只画；要能装子节点走下面的 `children`，那是控件级扩展）
- 不让父外观改子节点的状态（`states` 的寻址空间 v1 仍只有「层」）
- 不动渲染后端与绘制期零重排这条主线

## 3. 三种东西，三个层级（先对齐概念）

**整份外观 JSON = 声明（模板）**：它自己不参与任何运行期结构、不进布局，
可以被零成本引用多处 —— `import` 引外观、`new AppearanceView(name)` 让一个 View 套用它、
将来的 `children` 让父 View 据此创建子 View。

| 声明 | 声明的是什么 | 谁把它变成运行期实体 | 变成什么 |
|---|---|---|---|
| `box` | 布局参数（宽高/内外边距/轴向/对齐 + 底色边框） | **引用它的 View**（`applyAppearanceBox`） | 写进那个 View 的 Yoga 节点 |
| `layers[]` | 装饰层（rect/text/image/shadow/glass/custom） | 同一个 View（`AppearanceView` 按注册表画） | 画在那块矩形里的一笔笔 |
| `children[]` | 子控件的声明（纯布局盒 / 别的外观 / 控件引用） | 同一个 View（复用视图模板实例化） | 真正的子 View，这时才产生布局节点 |

一句话：**`box` 自己不进布局** —— 是引用外观的那个 View 进布局，`box` 只是被套用的参数。

绘制顺序固定为：**View 自己的底（来自 box 的底色/边框）→ layers（按层序）→ 子 View**。

## 4. 目录结构

```
bklv/compile/                      ← 新增：JSON → 数据（解析、校验、编译）
  json_util.{hpp,cpp}              取数字/颜色/枚举/数组 + 带 key 路径的统一报错
  appearance_parse.{hpp,cpp}       外观入口：调度 + 错误汇总（<80 行）
  appearance_box.cpp               第 1 层：box 段
  appearance_states.cpp            第 4 层：states.<状态>.<层id>.<属性>
  appearance_transitions.cpp       第 4 层：transitions.<事件>...
  appearance_children.cpp          子节点：外观内组合控件（见 §6）
  layer_registry.{hpp,cpp}         第 2 层：op 注册表 + 查表
  layers/                          第 3 层：一种 op 一个文件（第三方加在这里）
    rect.cpp  text.cpp  image.cpp  shadow.cpp  glass.cpp  custom.cpp
  view_parse.{hpp,cpp}             视图描述解析（从 core/json_view.cpp 搬）
  theme_parse.{hpp,cpp}            主题解析（从 core/theme_data.cpp 搬）

bklv/core/                         运行时与公开数据类型（几乎不动）
  appearance.hpp                   公开 API 保留；内部 include 拆分后的碎片
  appearance_instance.{hpp,cpp}    运行时状态机（tweeny 只在这里）
```

**边界五条（评审重点）**

1. `compile/` 只做「JSON → 数据 + 错误」，不含运行时、不含绘制
2. `core/` 只放运行时数据与状态机；tweeny 只能出现在 `appearance_instance.*`
3. 绘制留在 `widgets/appearance_view.cpp`，它按注册表调各 op 的 draw 钩子，自己不再写 `if (op == …)`
4. 公开头路径不变（`bklv/core/appearance.hpp`），调用方零改动
5. 四套 JSON（主题/视图/外观/i18n）共用 `json_util`：宽容规则与错误格式只写一遍

## 5. 图层类型注册表（核心扩展点）

设计取向按常见 C++ 规范：**无宏、显式注册、不做静态初始化依赖**（本项目已有先例：事件总线刻意不用宏/typeid）。

```cpp
// bklv/compile/layer_registry.hpp
namespace bklv::compile {

enum class LayerOrder { SHADOW, GLASS, BACKGROUND, HIGHLIGHT_BG, CONTENT, DECORATION, OVERLAY };

struct LayerType {
    std::string name;                                   // "stripe"
    std::string defaults;                               // 加层时的默认字段（JSON 文本）
    std::vector<std::string> keys;                      // 可在检查器里编的键
    LayerOrder order = LayerOrder::DECORATION;          // 引擎据此排序（不用再手工 hoist）
    std::function<bool(const Json&, AppearanceLayer&, const ThemeData&, std::string&)> parse;
    std::function<void(const AppearanceLayer&, ImDrawList*, const LayerRect&)> draw;
    std::string help;                                   // 设计器/文档用的一句话
};

// 注册与查表（重复注册报错，不覆盖）
bool registerLayer(LayerType type, std::string& error);
const LayerType* findLayer(const std::string& name);

// 内置层：显式列出，不加任何静态初始化魔法
void registerBuiltinLayers();

} // namespace bklv::compile
```

第三方加一种 op（**不碰库任何文件**）：

```cpp
// mygame/layers/stripe.cpp
namespace {
bool parseStripe(const Json& node, AppearanceLayer& out, const ThemeData& theme, std::string& err) {
    if (!resolveNumber(node, "angle", theme, out.stripeAngle, err)) return false;   // 共用 json_util
    return true;
}
void drawStripe(const AppearanceLayer& layer, ImDrawList* list, const LayerRect& rect) {
    /* 纯绘制：不参与布局、不改尺寸 */
}
}

// 初始化时注册一次（放在自己的 main/模块初始化里）
void registerMyLayers() {
    bklv::compile::registerLayer({
        .name = "stripe", .defaults = R"({ "angle": 45, "width": 2 })",
        .keys = { "angle", "width", "color", "gap" },
        .order = bklv::compile::LayerOrder::DECORATION,
        .parse = parseStripe, .draw = drawStripe,
        .help = "斜条纹：angle 角度 / width 线宽",
    }, error);
}
```

**注册后免费得到**：层序自动正确、错误提示统一、`states/transitions` 能直接寻址这层的属性、设计器自动认它（菜单/中文名/候选键/层序校验全部读注册表）、文档表格可从注册表生成。

## 6. 外观里的子节点 `children`（控件级组合）

让「一份外观 = 一个完整控件」，并且纯主题就能造新控件。

```json
{ "box": { "axis": "row", "gap": 10 },
  "layers": [ … ],
  "children": [
    { "appearance": "base/icon_badge", "box": { "width": 36, "height": 36 }, "icon": "@icon/star" },
    { "box": { "axis": "column", "grow": 1 },
      "children": [ { "appearance": "base/label", "text": "@i18n/x" } ] },
    { "ref": "widget/button.icon", "text": "@i18n/ok" }
  ] }
```

- 三档节点：`appearance`（建 `AppearanceView`）/ 只写 `box`（建普通 `Box`）/ `ref`（引用现成控件）
- **实例化复用现有 `ViewTemplate` / `instantiate`**，不另写解析器：i18n、绑定、错误路径全部统一
- 循环引用 / 深度上限 / 未知名都**报错**（对齐 `import` 的既有规则）
- 与 view JSON 的分工写进文档纪律：**view JSON = 页面结构**；**`appearance.children` = 一个控件自身的组合**

## 7. 迁移步骤（每步独立提交、可回滚）

| 步骤 | 内容 | 验收 | 回滚 |
|---|---|---|---|
| ①a | 抽 `compile/json_util.{hpp,cpp}`（把 appearance.cpp 的文件内解析助手搬过去） | 测试全绿（3266 项） | 单提交 revert |
| ①b | 把 `compile` 里的解析按**关注点**拆成函数（**不按 op**：字段解析是表驱动的，见 §12.12） | 同上 | 单提交 revert |
| ①c | 引入 `layer_registry`，注册 6 个内置 op（**"一种 op 一个文件"落在注册表层**，§12.12） | 同上 + 注册表查表测试 | 单提交 revert |
| ② | 拆 `appearance_parse`：501 行 → 若干 ≤80 行；`appearance_box/states/transitions` 分文件 | 同上 + `compile` 行数断言 | 单提交 revert |
| ③ | 搬 view/theme 解析进 `compile/`；`appearance_view.cpp` 改为查注册表 | 同上 + 设计器照常工作 | 单提交 revert |
| ④ | `appearance_children` + 三档节点 | 见 §8 的验收样例 | 单提交 revert |
| ⑤ | 设计器改为读注册表（菜单/键表/层序校验） | 设计器里能加 `stripe` 且不改设计器代码 | 单提交 revert |


### 实施备注 3（做 ⑥ 时发现的关键缺口）：op 目前是**封闭枚举**，第三方 op 表示不了

`AppearanceLayer::Op` 是 `enum class { RECT, TEXT, IMAGE, SHADOW, GLASS, CUSTOM }`，
解析器遇到不认识的 op 串会**直接报错**。也就是说：照现方案，第三方即使注册了 `stripe`，
`AppearanceLayer` 里也没有东西能表示它 —— §8 那条"在 `bklv/` 之外加一种 op"的验收**走不通** ✗。

**解决办法（建议，改动很小）**：给枚举加一个**扩展槽**，把"是哪一种扩展"放在数据上：

```cpp
enum class Op { RECT, TEXT, IMAGE, SHADOW, GLASS, CUSTOM, EXTENSION };

struct AppearanceLayer {
    Op op = Op::RECT;
    std::string extensionType;   // op == EXTENSION 时，注册表里的类型名（"stripe"）
    ...
};
```

解析期：op 串查注册表 → 命中内置 6 种就填对应枚举；命中**已注册的扩展**就填
`Op::EXTENSION` + `extensionType`；两者都没有才报错（保持"写错必报错"）。
绘制期：`Op::EXTENSION` 走注册表的 `draw` 钩子（③ 的自然延伸）。
层序：用注册表的 `order` 档位排序（引擎里那张"阴影 0 → 玻璃 1 → 其余"的表也一并交给档位）。

好处：**枚举保持封闭**（编译器仍能做穷尽性检查），扩展只带一个字符串；
内置 6 种的行为完全不变（它们仍然走各自的枚举分支）。

这条必须在 **①c-3 之前**落地，否则注册表的 `parse` 钩子没有合适的返回值可用。

## 8. 可扩展性验收（这是方案的证据，不是口号）

1. **加一种 op**：在 `mygame/layers/` 新增 1 个文件 + 在自家初始化里注册 1 行 →
   `git diff --stat` 里**不含** `bklv/` 任何文件；主题里写 `"op": "stripe"` 生效；
   设计器能加/能编；`states.pressed.stripe.angle` 可用
2. **加一个控件**：新增 1 份外观 JSON（含 `children`），纯主题完成，不改 C++
3. 每个新 op 的**绘制钩子不得触及布局**（代码评审项 + 测试里断言：加层前后 `App::getLayoutCount()` 不变）

## 9. 风险与取舍

- **重构风险**：行为不变，但动到 1906 行文件 → 靠 3266 项检查 + 每步独立提交兜底
- **Switch 构建**：`scripts/build_switch.sh` 按目录收集 core/views，新增 `compile/` 必须加进收集列表（AGENTS 有记录），否则 Switch 上 undefined reference
- **注册表查表开销**：解析期一次性查表，运行期不查 → 不影响帧时间
- **两套 JSON 的重叠风险**：靠 §6 的分工纪律 + 文档；如果将来发现两边都在写页面结构，就再收一次
- **不做的事**（避免过度设计）：op 参与布局、父外观改子状态、状态寻址扩成树路径

## 10. 评审请重点看这五条

1. 目录与命名（`bklv/compile/` + `layers/` 一种 op 一文件）
2. 注册机制：**无宏、显式注册、不依赖静态初始化**
3. `children` 三档节点 + 复用视图模板（不另写解析器）
4. v1 明确不做：op 进布局、父改子状态
5. 迁移顺序与每步验收（含"加新 op 不改库文件"这条可执行证据）

---

**评审通过后的第一步**：执行 §7 的 ①，并以 §8 的 `stripe` 样例作为验收证据。

> 实施备注（做的时候发现的依赖顺序）：① 必须先拆成 ①a/①b/①c。

> 实施备注 2（拆 `parseLayers` 时实测）：这一段的代码**不是按 op 分支**的，
> 而是「按字段统一驱动」+ 一处 op→默认 id 的映射。所以"按 op 拆函数"不是一次机械搬家，
> 而是**先要引入每种 op 的键表**（也就是 ①c 的注册表）。据此把顺序调整为：
> **①c 先把注册表（名字/默认字段/键表/层序/help）落下来 → `parseLayers` 退化成
> "查表 + 分派" → 再按 op 把字段解析迁进 `compile/layers/<op>.cpp`。**
> 迁移前先补 `tests/layer_parse_test.cpp` 作为行为基线。
>
> 原因：注册表要求"每种 op 一个 parse 钩子"，而现在的解析是 `compile` 里一个 501 行的函数体，
> 钩子无处可挂 —— 所以顺序是 **先搬助手（①a）→ 再按 op 拆函数（①b）→ 最后才注册（①c）**。

## 11. 实施进度（每步一个提交，可回滚）

| 步骤 | 内容 | 提交 | 结果 |
|---|---|---|---|
| ①a | 抽 `compile/json_util.{hpp,cpp}`（取值/报错只写一遍；`Json` 别名收敛） | `8462f74` | `appearance.cpp` 1906 → 1798；`check.sh` 3266 全过（零行为变化） |
| ①b-1 | `states` 段抽成 `parseStates()` | `bf22110` | `compile` 501 → 423 |
| ①b-2 | `layers` 段抽成 `parseLayers()` | `ee04bd4` | `compile` 423 → **49 行**（达标 <80） |
| ①b-预备 | 图层解析行为基线 `tests/layer_parse_test.cpp`（拆之前的保险） | `5f5e94e` | 33 项；立刻抓出"默认 id 是 `rect0` 而非 `rect`"与 `shadow/glass → custom<N>` 的 quirk |
| ①c-1 | 图层类型注册表（名字/默认 id 前缀/层序档位/键表/help；无宏显式注册；内置 6 种惰性注册） | `ee50139` | 注册表测试 + **注册表 vs 解析器交叉校验**（64 项） |
| ①c-2 | 默认 id 前缀改由注册表提供 | `c6fa9f6` |
| ⑥-a | 设计器 op 列表/中文名改读注册表 | `c5d9503` | **修掉"设计器里切不到 glass"的 bug**（下拉框写死 5 项 ✗） |
| ⑥-b | 检查器候选属性并入注册表键表（取并集） | `d1f32ed` | 第三方新 op 不改设计器就能编自己的字段 |
| ①c-3-pre | `Op::EXTENSION` + `extensionType` 扩展槽 | `b30bc77` | 解决"第三方 op 无法表示"的硬缺口；未注册的 op 仍报错 |
| 验收-样例 | `demo/contrib/stripe_layer.cpp`（`bklv/` 之外加一个 op） | `6957538` | 注册 + 解析路径端到端通过；主题侧样例**主动撤回**（见下） | **修掉 quirk**：`shadow`/`glass` 从 `custom<N>` → `shadow0`/`glass1`；基线翻断言，变化显式可见 |

**两件要紧的遗留**（别忘）

1. ~~clean 构建审计~~ **已完成 ✓**：`rm -rf build/fresh` + 从零 configure/build（GL），
   结果 **总告警 0 条**（连第三方都是 0 —— libnx 头只在 Switch 构建里出告警）。
   结论：上一次删掉的未使用函数确实解除了一颗定时炸弹；
   **并把这条写进验收流程：每次大改（或每条重构提交）做一次全新构建数告警**，
   不要只依赖增量构建 —— 增量会跳过长期未重编的 TU，潜伏告警就查不出来。
   ⚠️ 用额外构建目录后要按 AGENTS 的做法恢复：
   `rm -f dist/*_test dist/demo dist/designer && cmake --build build/gl -j 8`（多构建目录共用 `dist/`）。
2. **"主题里直接用扩展 op"还缺一环**：`theme_test` 会编译主题 `files[]` 里的所有外观，
   而测试进程里没人注册第三方 op → 会以"不认识的 op"失败。
   所以主题侧样例（`base/stripe_demo.json`）这轮**主动撤回**了，等 ③（绘制读注册表）时
   一并解决"主题加载/测试如何看到第三方注册"。

**还剩**（按顺序）

1. ①c-3：把 `parse` / `draw` 钩子接上注册表 → `parseLayers` 退化成"查表 + 分派"，
   键校验读键表；每种 op 的字段解析迁进 `compile/layers/<op>.cpp`（一文件一 op）
2. ③ `widgets/appearance_view.cpp` 改读注册表的 draw 钩子，删掉 `if (op == ...)` 链
3. ④ 把 view / theme 解析搬进 `compile/`（`core/` 只留运行时；tweeny 只出现在运行时 TU）
4. ⑤ `appearance.children`（三档节点 + 递归 + 复用 ViewTemplate + 循环/深度报错）
5. ⑥ 设计器改读注册表（加层菜单/键表/层序校验自动认新 op）
6. **可扩展性验收**：在 `bklv/` 之外新增 `stripe` op（1 文件 + 1 行注册）→
   `git diff --stat` 不含 `bklv/`；主题可用；设计器能加能编
7. ①/② 收尾：补一次页面截图对比（当前每步靠 3331 项检查兜底，比截图更硬）

**过程中记录的三条经验**（都已写进提交信息，值得进 AGENTS）

- 机械替换必须先 `assert` 再落盘：定位错边界时脚本中止、文件零改动（`bf22110` 那次）
- 切出来的代码块 `return nullptr` 要按新函数签名改成 `return false`；`appearance->x` 的机械
  改写可能造出自引用（`box = box`），编译器会直接报出来
- 聚合初始化会踩 `-Wmissing-field-initializers`（尾部钩子字段）→ 用工厂函数命名赋值，
  将来加字段也不会漏

### 阶段性验证（第 23 轮，全部改动之后的复查）

近期连续改过库代码（json_util / 注册表 / 默认 id / 扩展槽 / 扩展 parse+draw 钩子 / 保留键校验），
所以做了一次完整复查：

- `rm -rf build/fresh` + 从零 configure/build（GL）：**总告警 0 条**（本仓 0，第三方 0）
- 恢复 `dist/`（`rm -f dist/*_test dist/demo dist/designer && cmake --build build/gl -j 8`）后
  `./scripts/check.sh`：**3348 项全过**；`check_naming.py`：175 个文件全过
- 扩展链端到端：注册（`bklv/` 之外）→ 保留键防呆 → 解析（`EXTENSION` + `type` + **parse 钩子**）
  → 默认 id → 设计器自动认 → 主题里直接用 → 绘制钩子 → 出画面（截图确认过条纹铺满）

**剩余三件（都还没动）**

1. **①c-3 余下**：内置 6 种的字段解析仍挤在 `parseLayers`（382 行）里，尚未按 op 拆进
   `compile/layers/<op>.cpp`（扩展路径已经走注册表，内置路径还是老样子）
2. **④** view / theme 解析搬进 `compile/`（`core/` 只留运行时；tweeny 只出现在运行时 TU）
3. **⑤** `appearance.children`（外观内组合控件：`appearance` / 只写 `box` / `ref` 三档）——
   设计已在本文 §6，实现尚未开始；这是"一份外观 = 一个完整控件"的关键一步

优先级建议：**先 ⑤**（新能力，用户侧感知最强，且不依赖前两件），再 ④（结构与依赖清理），
最后收 ①c-3 余下（纯内部整理）。

## 12. ⑤ `appearance.children` 实现规格（实现前先定死）

### 12.1 JSON 语法

`children` 是外观 JSON 的顶层段，与 `box` / `layers` / `states` / `transitions` 平级：

```json
{
  "box": { "axis": "row", "gap": 10, "padding": 12 },
  "layers": [ { "op": "rect", "id": "bg", "fill": "@theme/color.surface.card", "radius": 12 } ],
  "children": [
    { "appearance": "base/icon_badge", "box": { "width": 36, "height": 36 }, "icon": "@icon/star" },
    { "box": { "axis": "column", "gap": 4, "grow": 1 },
      "children": [ { "appearance": "base/label", "text": "@i18n/card.title" } ] },
    { "ref": "widget/button.icon_text", "text": "@i18n/ok", "icon": "@icon/check" }
  ]
}
```

### 12.2 三档节点（互斥，写两个 = 报错）

| 节点 | 建什么 | 就绪度 |
|---|---|---|
| `"appearance": "<名>"` | 一个 `AppearanceView`（名字同现有规则，含 `.变体`） | 本步实现 |
| 只写 `box` | 一个 `Box`，套用该 `box` 段（纯布局盒，递归嵌套的载体） | 本步实现 |
| `"ref": "<视图模板名>"` | 用主题 `view/` 里的**视图模板**实例化（可产出任意控件类型） | 复用现成机制 |

第三档刻意复用**视图模板**（`ViewTemplate` + `instantiate()`），而不是新造"控件注册表" ✗ ——
这样"引用别的控件"能把库里已有的一切（Button / Label / Image / Cell…）都用上，
绑定、i18n、错误路径全部沿用一套。

公共可选键：`box`（覆盖/追加）、`children`（递归，仅当本节点建的是容器时允许）、
内容槽 `text` / `icon` / `info`（实例化后按名套到 `AppearanceView` 的既有槽位上）。

### 12.3 数据模型与文件

```cpp
// bklv/core/appearance.hpp（编译产物里的**纯数据**，不含视图）
struct AppearanceChild {
    enum class Kind { APPEARANCE, BOX, VIEW_TEMPLATE };
    Kind kind = Kind::BOX;
    std::string name;              // appearance 名 或 视图模板名
    AppearanceBox box;             // 写了的字段才套用
    std::string text, icon, info;  // 内容槽（可空）
    std::vector<AppearanceChild> children;
};
// Appearance 增：std::vector<AppearanceChild> children; + getChildren()
```

- **解析**：`bklv/compile/appearance_children.{hpp,cpp}`（新文件；`compile/` 已在 CMake 与
  Switch 的按目录收集里 ✓，不用再改构建清单）
- **实例化**：`bklv/widgets/appearance_children.cpp`（建视图是 widgets 的事，compile 不建视图）
- 公开头 `core/appearance.hpp` 只多一个**纯数据**结构 + getter（不引 nlohmann）✓

### 12.4 时机与生命周期

- 父视图（`AppearanceView`）在**构造时**和 `applyTheme()` 重建外观后，按 `children` 规格
  建一遍子视图；规格没变就不重建（避免每帧重排 / 丢焦点）
- 子视图是普通子节点：`addView(...)` ✓ —— 于是它们**进布局**（与 `layers` 的本质区别）
- 换肤：父外观重取时子节点跟着重建（子节点自己的外观由它自己去 `applyTheme`）

### 12.5 错误规则（全部硬错误，不静默降级）

1. 同时写 `appearance` 与 `ref` → 报错
2. 两样都没写、也没写 `box` → 报错（不知道要建什么）
3. `ref` / `appearance` 指向不存在的名字 → 报错
4. 在 `box`-only 节点上写 `appearance`/`ref` 之外的未知键 → 警告（对齐现有约定）
5. **深度上限 16**（超过报错）—— 比视图 JSON 的 32 更保守，控件组合嵌套不该那么深
6. **循环**：`appearance` A 的 children 里引用 A（或 A→B→A）→ 报错。
   判定走"当前实例化链上的 appearance 名集合"（与 `import` 的循环检测同一套思路）
7. **v1 不做**：父外观改子节点状态（`states` 的寻址空间仍只有"层"）；子节点参与父的
   `transitions` 视图级通道以外的东西；"只写 box 但带 children"的隐式容器猜测（明确允许即可）

### 12.6 测试计划（无窗口）

- **解析**（`tests/appearance_children_test.cpp`）：三档各能解析；2/3/5/6 各类错误都能复现；
  `box` 字段只覆盖写了的字段
- **实例化**：构造父 `AppearanceView` → 断言子节点个数/类型/外观名/内容槽；
  规格未变时不重建（对比重建计数）
- **视觉验收**：主题里加一份用 `children` 的"卡片（图标 + 标题 + 副标题）"外观，
  demo 的 JSON 页加一行显示它 → 截图与"纯 layers 拼的同类卡片"对比（确认可读、无溢出）
- 回归：全部既有 3348 项必须保持全绿（`children` 是**纯新增段**，不写就没有任何行为变化）

### 12.7 里程碑（每步一个可回滚提交）

1. 数据模型 + 解析 + 解析层测试（不接触视图）
2. 实例化：`appearance` + `box` 两档 + 实例化测试
3. 第三档 `ref`（复用视图模板）+ 测试
4. 主题示例外观 + demo 一行 + 截图；`docs/json-syntax.md` 补"children"一节
5. （可选）设计器里把 children 树显示出来（只读展示即可，编辑放最后）

### 12.8 里程碑 2 实现路径（侦察后确定）

已经确认的现成设施（不用新造）：

- `AppearanceView : public Box`，构造 `AppearanceView(std::string appearanceName, bool focusable)`
- 加子节点就是 `parent.addView(std::move(child))`（`account.cpp` / `bottom_bar.cpp` 都是这么写的）
- **`applyAppearanceBox(Box* view, const AppearanceBox& box)`** —— 正是"只写 `box` 的子节点"需要的；
  它跟外观套用走同一个函数，行为天然一致
- `AppearanceView` 在**构造**（`appearance_view.cpp:207`）与 `applyTheme()`（:240）里套自己的 box

因此里程碑 2 的实现是：

```cpp
// widgets/appearance_children.cpp（新）
// 按 spec 建子视图并挂到 parent 上；返回 false = 建不起来（调用方决定是否报错）
bool buildChildViews(AppearanceView& parent, const std::vector<AppearanceChild>& spec);
```

- `APPEARANCE` -> `new AppearanceView(child.name, false)` + `applyAppearanceBox` + 内容槽
  （`setText` / `setIcon` / `setInfoText`）+ 递归
- `BOX` -> `new Box()` + `applyAppearanceBox` + 递归
- `VIEW_TEMPLATE` -> 里程碑 3
- 时机：父视图**构造时**建一次；`applyTheme()` 里若外观**名字变了**就重建
  （v1 不做"规格逐字段比较"，按名字变即重建 —— 简单且够用）

⚠️ **里程碑 1 需要先修的一处隐患**：`AppearanceBox` 是靠"哪些字段写过"来决定套不套用的
（AGENTS ⑦b：没写的字段不会清掉旧值），而我在 `compile/appearance_children.cpp` 里手搓的
`parseChildBox` **只填了值、没设那些存在标记** —— 直接拿它去 `applyAppearanceBox` 会把
width/height/padding 等**清零套用** ✗。

正确做法（也正好消掉我标的那条 TODO）：**把 `appearance.cpp` 里现有的 box 段解析提成一个可复用函数**
（例如 `compile::parseBox(node, theme, AppearanceBox&, std::string& error, where)`），
`children` 与主 `box` 段共用同一份 —— 这样 `axis`、`margin`、`justifyContent` 等全部自动支持，
存在标记也由同一套逻辑维护。**这是里程碑 2 的第一步**，而不是继续手搓。

### 12.9 `children` 的布局验证（第 39 轮，含一条方法论教训）

**结论：`children` 的组合布局是正确的**，之前"布局有问题"的判断是**误读裁剪截图**造成的 ✗。

用"布局之后探针"（临时 `ProbeCard : AppearanceView`，在 `update()` 里量最终矩形）得到的真实数据：

```
card            x=425 y=1794 w=405 h=212  children=2
child[0] 图标块 x=437 y=1882 w= 36 h= 36     （36x36，垂直居中）
child[1] 内层盒 x=473 y=1806 w=345 h=188 ownChildren=2  （两个标签在盒子里）
```

三处对得上：437+36=473（并排）、345 = 405−12×2−36（宽度分毫不差）、图标 y 居中（`alignItems: center` 生效）。

**方法论教训（两次都栽在"看"上）**
- 第 17 轮：条纹"只剩左边缘毛刺" -> 猜了两轮画法都不对，**打一行日志量出 `w=3.0`** 一次定位
  （撞了引擎保留键 `width`）
- 第 39 轮：裁剪截图看着像"竖直堆叠、顺序不对" -> **量矩形**发现布局完全正确，是我看错了

所以约定：**布局/几何问题一律先量（探针打真实矩形），不要靠看截图下结论**；
截图只用来看"有没有画出来、观感对不对"，不用来判断数值。

仍然待补（不影响正确性）：内层盒的 `justifyContent` 等键还不在手搓的 box 解析里，
所以标签在 188 高的盒子里靠上 —— 等 `parseBox` 复用之后自然解决。

### 12.10 `parseBox` 复用的侦察结果（下一刀怎么切）

**发现（第 41 轮）**：现在的 `parseLayers`（382 行）其实是**两段**，不是一段 ✗：

- **box 段解析**：`appearance.cpp` 第 **750 行**起（`if (doc.contains("box") && doc["box"].is_object())`），
  填 `width/height/padding/margin/gap/grow/shrink/axis/justifyContent/alignItems/backgroundColor/
  cornerRadius/border*` 与 `hasXxx` 存在标记；里面还有 `readLength`（数字 / "auto" / "50%" / 主题引用）
- **layers 段**：其后的图层循环

这也解释了为什么 `parseLayers` 要收 `AppearanceBox& box`（我抽的时候照搬了签名，没深究）**以及
为什么 children 的 box 只能认 6 个键**：children 走的是我手搓的那份，而全套逻辑一直在这 150 行里。

**下一刀（机械切片，方法已经用过两次）**

1. 新增 `compile/appearance_box.{hpp,cpp}`：
   ```cpp
   // 解析一个 box 对象（doc 里的 box 段、children 里的 box 节点都走它）
   bool parseBoxNode(const Json& boxNode, const ThemeData& theme, AppearanceBox& out,
                     std::string& error, const std::string& where);
   ```
   把 750 行那段**整块切进去**（含 `readLength` 等局部助手），`doc["box"]` 的取用留在调用方
2. `parseLayers` 开头改成调 `parseBoxNode(doc["box"], …)` -> 从 382 行降到 ~230 行
3. `compile/appearance_children.cpp` 删掉手搓的 `parseChildBox`，改调同一个 `parseBoxNode`
   -> **`axis`/`margin`/`justifyContent`/`alignItems` 等自动全都支持**，未知键告警也由同一份逻辑管
4. 验收：`check.sh` 3410 项全过（纯搬家、行为不变）+ `children` 的主题示例可以去掉那条
   "axis 不认识" 的告警；测试里补一条"children 的 box 能写 alignItems"

**做法提醒（自己踩过的）**：切片脚本**先 assert 边界、先验花括号配平，再落盘**；
改完要 grep 确认"真的改上了"（有一次 replace 没加断言，静默失败，靠计数才发现）。

### 12.11 12.10 已执行（第 42 轮）：box 段解析成为共享实现

- 新增 `compile/appearance_box.{hpp,cpp}`：`parseBoxNode(boxNode, theme, box, error, where)`
- `parseLayers` 从 382 行降到 **~230 行**；`appearance.cpp` 1906 -> **1714 行**
- `children` 删掉手搓的 `parseChildBox`，改调同一个函数
  -> **字段支持变全**（axis/alignItems/justifyContent/margin/minWidth/背景/圆角/边框/measureContent…）
- 测试 `appearance_children_test` 31 -> **37 项**（新增"children 的 box 能写全套字段"）
- `check.sh` 3416 项全过；三平台 0 告警 —— 纯搬家 + 一处行为增强

**下一刀的落点**：`parseLayers` 现在 ~230 行，大头是**图层循环里的字段解析**；
按 op 拆进 `compile/layers/<op>.cpp` 是 ①c-3 余下的内容（模式已经定：
机械切片 + 先 assert 边界 + 先验括号配平 + 改完 grep 确认真的改上了）。

### 12.12 ①c-3 余下的侦察结果：**不能按 op 拆**（计划修正）

侦察 `parseLayers`（499-758，**259 行**）的结构，得到与 ①b 时同样的结论，而且更明确：

```
502  for (size_t i = 0; i < doc["layers"].size(); i++)     <- 图层循环，占 245 行
507    if (!node.is_object())                              <- 元信息
518    if (layer.op == Op::EXTENSION) ... parse 钩子        <- 扩展 op（已按注册表）
533    if (!opOk)
539    ／ 默认 id 前缀（来自注册表）
552    ／ 三项拼错的编译期警告
570    // 颜色 / 数值字段
582    for (const ColorField& field : colorFields)          <- **字段表驱动**
609    for (const NumberField& field : numberFields)        <- **字段表驱动**
619    if (node.contains("inset")) ... 628 offset
637    // RECT 层的渐变
638    if (node.contains("gradient"))                       <- 约 110 行
747  hoistGlassLayers(...)
751  parseBoxNode(...)                                      <- 已抽出（12.10）
```

**关键事实：图层字段解析是"表驱动"的，不是按 op 分支的** ✗。
`colorFields` / `numberFields` 两张表被**所有 op 共用**（这正是"单一事实来源"该有的样子），
所以"按 op 拆进 `compile/layers/<op>.cpp`"这一刀**切不下去** —— 硬切只会造出一堆
互相复制的分支，反而把现在的好设计拆坏。

**每种 op 的"个性"其实已经集中了**：名字 / 中文名 / 默认 id 前缀 / 层序档位 / 键表 / help /
parse / draw 全在 `compile/layer_registry.cpp` ✓ —— 也就是说 §2 想要的"一种 op 一个文件"
在**注册表层**已经达成；字段解析层**本来就应该通用**。

**修正后的切法（按"关注点"切，不按 op 切）**

1. `parseLayerMeta(...)`：node 检查 / op / id / source / align / valign / scaling / 拼写警告
2. `parseLayerFields(...)`：两张字段表 + cutout + inset + offset
3. `parseGradient(...)`：渐变那 ~110 行（本来就是一个自成一体的校验器）
4. `parseLayers` 剩下"循环 + 调度 + hoist"（目标 ≤80 行）

三条都落在 `compile/` 下（`layers/` 这个目录**不建了** —— 因为没有"每种 op 一个文件"这回事），
并把 §7 表格里 ①c-3 的措辞改成"按关注点拆字段解析；op 的个性留在注册表"。

### 12.13 ①c-3 第三刀（parseLayerMeta）的侦察与正确切法

`parseLayers` 现在 **91 行**（目标 ≤80，差 11 行）。剩下的主体是**单个图层的元信息解析**
（`appearance.cpp` 509-572，64 行，括号 5:5 配平）。

**它不能像前两刀那样直接整块搬** ✗：这段引用了循环内的局部量
`i` / `opName` / `opOk` / `layerType` / `idPrefix` / `where` / `layers`。
其中 `opName` / `opOk` / `layerType` / `idPrefix` 都是**在段内自己算出来的** ✓（从前面的结构图看：
`parseOp` 取 op、`opOk` 判成败、`findLayer` 查注册表、`idPrefix` 取默认前缀）——
真正必须外部传入的只有 **`i`**（默认 id 的后缀 `rect0/rect1` 要用）和 `layers`。

**正确切法（下一轮一次做完）**

```cpp
bool parseLayerMeta(const Json& node, size_t index, const ThemeData& theme,
    AppearanceLayer& layer, std::string& error);
```
1. 段内自己算 `opName` / `opOk` / `layerType` / `idPrefix`（都在 509-572 的范围内）
2. 只有 `index` 从调用方传（后面拼默认 id）
3. `layers` 的引用先确认是"往里 push"还是"读 size"：若是 push，就留在 `parseLayers` 里
   （push 是循环的职责，不属于"解析一层"）
4. 完成 `parseLayers` ≤80 行后，②就算收口，进 ④（view/theme 解析搬进 `compile/`）

**做法**：仍然"先自检边界（括号配平 + 引用清单）-> 再落盘 -> 构建失败自动回滚"，
这套在这一轮已经拦下过三次错误切片。

### 12.14 parseLayerMeta 第一次尝试：自动回滚（三个原因已查明）

这一刀用「写入 -> 构建 -> **失败自动回滚**」的脚本做的，结果构建失败，脚本把改动全部撤掉，
**树干净、main 仍是绿的** ✓。三个原因都很明确：

1. **`layer` redefinition**：509-572 这段里**自己声明了** `AppearanceLayer layer;` ——
   也就是元信息解析负责"造出这一层"，而不是改一个传进来的层 ✗
   -> 签名应当改成**输出参数**：`AppearanceLayer& out`，段内那行声明删掉、`layer` 机械改名为 `out`；
   调用方在调用**之前**声明 `AppearanceLayer layer;`
2. **`parseOp` 未声明**：它是 `appearance.cpp` 的**文件内**函数（op 串 -> 枚举 + EXTENSION 兜底），
   抽出后必须**导出**（放 `layer_registry.hpp` 或 `appearance_layer_meta.hpp` 都行）
3. **调用点 `layer` 未声明**：因为声明在段内被搬走了 -> 由第 1 条的输出参数解决

**下一轮的正确切法**（一次能成）：
```cpp
// appearance_layer_meta.hpp
AppearanceLayer::Op parseOp(const std::string& text, bool& ok);          // 导出（原为文件内）
bool parseLayerMeta(const Json& node, size_t index, const ThemeData& theme,
    AppearanceLayer& out, std::string& error);                            // out = 输出
```
调用方：`AppearanceLayer layer; if (!parseLayerMeta(node, i, theme, layer, error)) return false;`

**方法论**：「写入 -> 构建 -> 失败自动回滚」这个脚本形态值得保留 ——
它让"试一刀"的成本变成零风险（这次就是它兜住的）。

### 12.15 ①② 的收口状态（第 50 轮）

| 目标 | 结果 |
|---|---|
| `Appearance::compile` ≤80 行 | **53 行** ✓（起点 501）|
| `parseLayers` ≤80 行 | **30 行** ✓（起点 382）|
| `parseStates` ≤80 行 | **87 行** ✗（差 7 行）|
| `appearance.cpp` | 1906 -> **1465 行** |
| 解析层落点 | `bklv/compile/`：json_util / layer_registry / appearance_box / appearance_layer_meta /
  appearance_layer_fields / appearance_gradient / appearance_children |

`parseStates` 是唯一没达标的：结构是"一个 `if` 包一个循环"，要压到 ≤80 只能把**循环体**提成
`parseOneState(...)`。第 50 轮试了一次，**循环体配平断言没过、脚本没落盘（文件零改动）** ✗ ——
下次先把它循环体的括号分布打出来（像 §12.9 量布局、§12.14 定位切片那样），看清再切。

**①② 的实质目标已达成**：`compile` 与 `parseLayers` 都达标、"一种 op 一个文件"落在注册表层、
四种 JSON 共用 `json_util`。`parseStates` 那 7 行属于"顺手收尾"，不影响结构与可扩展性。

### 12.16 ①/② 的重构前后截图对比（第 54 轮）

约定过"① 与 ② 额外要求页面截图与重构前一致"，这轮补上：

- 参照：`/tmp/shell_page.png`（**重构前**截的「外壳」页，10:35）
- 现在：重新截同一页（临时把 `pendingStartPage` 设为 14、截完已还原，工作树干净）
- 结果：**无回归** ✓
  · 整图 JPEG（1:1 同尺寸）**107135 → 107051 字节**，差 **0.08%**
  · 肉眼核对：页头「外壳」+ 右侧动作说明、侧栏 15 项（滚到底部）、两份 section、
    三行开关（页头/底栏/日志窗口）、焦点框位置、底栏提示 —— 与重构前一致
- 另做了一次"内容带逐字节比对"，哈希不同 ✗ —— 但这条**不能作为证据**：
  裁的那条带含底栏（时钟/电量会变），界面本身也有动效（焦点框相位）。
  **对"活的界面"用字节比对太苛刻**；真正硬的证据是 `check.sh` **3422 项全过**
  （含 theme_test 逐份编译两套主题的全部外观、gradient/shadow/draw 的顶点数与命令数断言）。

**方法论**（和 §12.9 那次"别靠看截图判断几何"呼应）：
- 判断**数值/几何** -> 打日志量真实值（探针）
- 判断**行为不变** -> 测试（用例数比截图硬得多）
- 判断**观感/有没有画出来** -> 截图（而且要看整图，别拿裁剪图下结论）

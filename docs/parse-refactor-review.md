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
| ①b | 把 `compile` 里按 op 的解析拆成 `layers/<op>.cpp` 的函数（仍在原 TU，先拆函数） | 同上 | 单提交 revert |
| ①c | 引入 `layer_registry`，注册 6 个内置 op（此时才谈得上"一种 op 一个文件"） | 同上 + 注册表查表测试 | 单提交 revert |
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

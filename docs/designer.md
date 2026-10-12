# 外观设计器（designer）

读 / 改 / 存主题的外观 JSON：**界面本体用 ImGui 控件画，预览用 bklv 的真实视图渲染**。
和 demo 是两个程序，互不依赖。

## 跑起来

```bash
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl && cmake --build build/gl
./dist/designer
```

启动时用仓库自带的 `default` 主题（**只读**：基准主题不许就地改）先给用户浏览；
要动手改就点「新建项目」：

| 模式 | 行为 |
|---|---|
| 模板新建 | 复制模板主题成新主题 + 在目标仓库的 `themes.json` 里登记；原主题字节不变 |
| 直接编辑 | 写回该主题（`default` / `ocean` 被拒绝，必须走模板新建） |

项目里存的是**主题目录的绝对路径**，所以主题在别的仓库（自己的项目）也能编辑。

## 界面

```
左上 文件列表（主题外观 / 页面视图 / 主题表）   中 预览区（选中什么渲染什么）   右 属性表
左   节点树（文件 → box → 每一层 → 每层字段 → states → 每状态 → 每通道）
左下 节点数 / 当前路径
```

- **预览区**：选「文件」渲染整个控件外观（真实控件类）；选「某一层」只画这一层
  （棋盘格底，方便看 shadow / 半透明）；选「某个状态」把该状态目标值归零时长后立刻到位；
  打开 `views/*.json` 时直接渲染**整页**（真实实例化那棵视图树）。
- **属性表**：按节点类型给表单（box 固定字段 / 层按 op 给字段 / states 的 to·duration·easing）；
  改任何一项立刻重建预览。数字和颜色都能写 `@theme/...` 引用。
- **保存**：先给 diff（哪个键从什么变成什么）→ 编译校验（不通过不写盘）→ 原子写 + `.bak` → 热重载。

## 快捷键与几个界面约定

| 键 | 作用 |
|---|---|
| Ctrl+S | 保存（先弹 diff） |
| Ctrl+Z / Ctrl+Y | 撤销 / 重做（帧级快照，最多 64 步） |
| Ctrl+D | 复制当前层 |
| Alt+↑ / Alt+↓ | 上移 / 下移当前层 |
| F1 | 校验面板（整主题校验 + 层序提醒） |
| F2 | 预览最大化 |
| 滚轮 / 中键（或 Shift+左键） | 画布缩放 / 平移 |

- **多文件同时改**：每份打开的文件的编辑状态都留着（草稿），切文件会拦截未保存改动
  （保存并切换 / 丢弃 / 取消）；「改动总览」列出所有脏文件并可一键全部保存
- **外部改动**：打开时记文件指纹，保存前发现磁盘变了会拦（覆盖 / 重新载入 / 取消）
- 外观文件必须在 `theme.json` 的 `widgets[]` 里登记 —— 界面上的「新建外观」会自动登记，
  写盘前还会先编译一遍（模板引用了不存在的主题 key 会当场报错，不落坏文件）
- 主题表 `colors.json` / `metrics.json` 也能改（颜色用取色器、数值可拖），保存后热重载
- 两处没做到位的：整页预览里**覆盖高亮**选中节点（需要库导出 JSON↔视图节点映射）、
  保存仍是整文件重写（键序与 `$` 元信息保住，写前 diff + 冲突检测 + `.bak`）

## 截图基线对比（界面回归）

```bash
# 1. 存一张基线
BKLV_DESIGNER_VIEW=page_json.json BKLV_DESIGNER_SHOT=/tmp/base.png ./dist/designer

# 2. 之后每次改动都跟基线比：差异 > 2% 像素就返回 1（差异图写到 <shot>.diff.png）
BKLV_DESIGNER_VIEW=page_json.json BKLV_DESIGNER_SHOT=/tmp/now.png \
  BKLV_DESIGNER_BASELINE=/tmp/base.png ./dist/designer
```

实测：同一页连拍两次平均差 0.00/255（渲染是确定性的）；换成别的文件是 19.45/255、
38.96% 像素不同、退出码 1，差异图里"变亮标红、变暗标蓝"。

## 自查开关（脚本 / CI 用）

```bash
BKLV_DESIGNER_PROJECT=my-skin          # 直接打开仓库里的某个主题
BKLV_DESIGNER_FORK=my-skin             # 先按模板新建一份再打开
BKLV_DESIGNER_FILE=button.primary      # 打开某个外观
BKLV_DESIGNER_VIEW=page_json.json      # 打开某个页面视图
BKLV_DESIGNER_TABLE=colors.json        # 打开主题表
BKLV_DESIGNER_NEW_APPEARANCE=my_widget # 新建一个外观文件（会登记进 theme.json）
BKLV_DESIGNER_SELECT=layer0            # 选中节点：file|box|layer<N>|state:<名>|view:/children/0
BKLV_DESIGNER_FOCUS=1                  # 预览里给焦点顺序打号
BKLV_DESIGNER_LOG=1                    # 开日志面板
BKLV_DESIGNER_FORCE_SAVE=1             # 立刻触发一次保存（验证只读拦截）
BKLV_DESIGNER_BASELINE=/tmp/base.png   # 与基线截图对比（界面回归）
BKLV_DESIGNER_SHOT=/tmp/a.png          # 渲染若干帧后离屏截图并退出
```

## 结构

```
designer/core/     逻辑层（**不含界面代码**，所以能无窗口测）
  theme_files.*      主题登记 / 外观清单 / 原子的写盘 + .bak / JSON 语法校验
  appearance_doc.*   一份外观的可编辑模型（保序 JSON，读→改→写不丢字段）
  diff.*             保存前的差异（按叶子，路径能指到 layers[2].fill）
  project.*          模板新建 / 直接编辑 / 项目记录
designer/ui/       界面层：ImGui 面板 + bklv 预览渲染
designer/main.cpp  入口
designer/mockup/   动手前对齐理解的 HTML 样板房（不是程序）
tests/designer_core_test.cpp   逻辑层的无窗口测试（62 项）
```

加预览模式 / 表单的注意点：预览是"手动驱动一棵不在 App 视图树里的视图"——
`YGNodeCalculateLayout` → `notifyLayout` → `update` → `setTranslation(画布原点)` → `frame(ctx)`，
并按画布矩形 `PushClipRect`。

## 预览里的"对着看"

- **选中树节点 → 预览里框出来**：库里 `createFromJSONString(json, &map)` 会导出
  "JSON Pointer ↔ 视图指针"的映射，设计器拿它读该节点的 `getFrame()` 画框（带路径角标）
- **点预览 → 反选节点**：在映射里找"包含点击位置、面积最小"的那个节点，选中它并展开左树
- **焦点顺序**：按视图树顺序列出可聚焦控件，可在预览里打 1 2 3…（导航顺序大致就是这个）
- **对比度检查**：文字层颜色与控件底色的对比度 < 4.5:1 时进校验面板提醒
- **日志面板**：直接复用库里的 `DebugWindow`（读日志环形缓冲），顶栏「日志」开关

## 第 10 轮补的

- **界面状态持久化搬进 core**（`designer/core/ui_state.*`）：以前 load/save 直接写在 UI 文件里，
  等于没有回归保护；现在有 13 项测试（往返、缺文件、坏 JSON 不覆盖已有值，
  默认值与 UI 保持一致、缩放默认改成 100%）
- **记住上次在看的文件**：`lastAppearance` / `lastViewFile` 存进状态，启动时自动打开
  （不像以前每次都回到第一个控件）
- **预览对齐网格**：10px 网格 + 中线开关（对间距、看居中最有用），状态里持久化
- 自审/自查抓到的三点：
  1. `setProject` 会打开第一个控件并**覆盖"上次在看"的名字**，导致启动恢复永远恢复不到
     → 用独立的 `startupAppearance` / `startupViewFile` 字段
  2. 缩放的默认值 UI 与 core 不一致（75% vs 100%）→ 统一成 100%
  3. 工具链教训：python `replace` 的锚点没匹配上时是**静默 no-op** ——
     `restoreLastFileIfAny()` 的调用因此压根没插进 main.cpp。现在每次替换都加断言

## 第 9 轮补的

- **颜色字段悬停**显示这个 key 被多少外观引用（改之前先知道影响面）
- **主题表保存后的影响清单**：这次动到的 key 影响了哪些外观，写进状态行与日志
- **编辑主题表时旁边保留预览**：主题表没有自己的预览，就用"最近打开的那个外观"
  （保存后主题热重载，再建一次就能看到新颜色）
- 自审/自查钩子抓到的四个缺陷：
  1. **页面视图的文本改了没点「应用」→ 保存时静默丢掉**。两条保存路径
     （`requestSave` / `doSave`）现在都会先自动应用一次，校验不过就不写
  2. **页面视图保存后草稿脏标记没清** → 文件列表一直挂着 `●`、改动总览里也留着
  3. **只读基准主题把页面视图也挡住了**：视图在 `resources/views/` 下，不属于主题，
     应该照常可改（只读只保护主题文件）
  4. 新建/删除外观后没刷新 `themeUsage`（引用关系会过期）
- 自查钩子补 `BKLV_DESIGNER_EDIT_TEXT=<JSON>`：改页面视图的**文本缓冲但不点应用**，
  用来验证上面第 1 条真的生效（实测：保存后文件里是新内容，且增量写盘只替换了变化的段）

## 第 8 轮补的

- **主题 key 的反向引用**（主题表里）：每一行显示"· N 处引用 / 没被引用"，
  点「谁在用」列出引用它的外观、点一下切过去。改配色前先知道会动到谁；
  "没被引用"的键是死配置，一眼可见
- 自审修的两个问题：
  1. **切换文件后没清绑定/映射**：焦点顺序面板会留着上一页的 `bindings`，
     那些 `View*` 在预览重建后就悬垂了（use-after-free）→ 换文件时清掉
  2. 「套用示例数据」以前**直接碰视图指针**：改成置标记、等"建完预览之后"再套，
     这条路径上的指针一定是新的；示例数据也按文件各自保存，不串页
- 截图抓到的 bug：主题表里的键是 `radius.md`（文件里没有 `metric.` 外层），
  而引用是 `@theme/metric.radius.md` —— 查表时没补前缀，导致**全部显示"没被引用"**

## 第 7 轮补的

- **数据绑定示例数据**：页面有 `bind` 时，预览里那些字段本来没有数据源（列表/文字全空）。
  现在打开页面会自动生成一份示例数据（`字段 → 示例 xxx`），面板里可编辑 +「套用示例数据」
- **切主题也会拦未保存改动**：切主题要清草稿，之前是静默丢；现在走同一个拦截弹层
  （保存并切换 / 丢弃 / 取消）
- 库侧新增 `applyViewAttribute(view, name, value)`：用字符串值泛化套一个属性
  （复用实例化 JSON 那条路：类型转换、`@theme` / `@i18n`、枚举宽容都在里面）；
  未知属性返回 false（`NOT_HANDLED` 不等于 APPLIED）

## 第 6 轮补的

- **主题切换器**：顶栏下拉直接切 `themes.json` 里的主题（基准主题只读浏览）；「设为默认」
  写 `themes.json` 的 `default`（只读主题不给改）
- **`include` 在树里展开**：`[include] <路径>` 节点展开成被包含文件的树（递归有上限）
- **数据绑定列表**：焦点顺序面板里列出"哪个节点绑了哪个字段"（预览里没有数据源，
  绑定出来的内容是空的 —— 面板里写着）
- **缩放提示更准**：画布不是 1280 宽时给「按真机 720p 画布」一键切换（主题尺寸都按 1280×720 写）
- 树里的角标换成 ASCII（`[bind x]` / `[include x]`）—— `⟲`/`⤵` 这类字形字体里没有，会画成 `?`

## 这一轮补的

- **枚举字段给下拉**：层的 `align` / `valign` / `scaling` / `axis` / `direction` 不再手打；
  `fontSize` 列出主题里的字号 key，`source` 列出可用槽位（也都能手写）
- **按约定重排层序**：层序提醒旁边给一个按钮（shadow → 底色 → hlBg → 内容 → hlGlow/hlBorder），
  只动顺序不改字段，Ctrl+Z 可撤
- **新建页面视图**：空页面 / 整页（页头+内容+底栏）/ 卡片，写盘前先试着实例化
- **`bind` / `include` 看得见**：树里节点带 `⟲ bind` / `⤵ include` 角标；页面有绑定时
  预览栏提醒"预览里没有数据源，绑定出来的内容是空的"
- **JSON 页面能用控件类型**（见 `docs/json-syntax.md`）：`Button` / `Rectangle` / `AppletFrame`
  / `Header` / `BottomBar` / Cell 家族等已注册，App 启动时自动注册

## 写在明面上的"没做到位"

- 保存是**整文件重写**（键序保住、缩进沿用原文件、内容没变就跳过、写前 diff + 冲突检测 +
  `.bak`），但不是真正的"只改那两行"
- 焦点顺序按**视图树顺序**近似（真机导航还受可聚焦性与模态影响），不是逐步模拟出来的

## 两条踩过的坑

- **ImGui 默认字体必须显式设**（`bklv/core/font.cpp` 的 `setDefaultFont` 会设
  `io.FontDefault`）。不设的话 ImGui 用字体表里的第 0 个，而那可能是先加载的
  **图标字体**（只有私用区几个字形）→ `ImGui::Text` 什么都画不出来，
  而 bklv 的文本没事（它自己传字体）。日志窗口/设计器都靠这条。
- **截图按帧缓冲读取**（`App::saveScreenshot` 用 `io.DisplayFramebufferScale`）：
  Retina 上帧缓冲是逻辑尺寸的 2 倍，按逻辑尺寸读只会拿到左下角那块，
  截出来的图是被放大的局部。

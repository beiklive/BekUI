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

## 自查开关（脚本 / CI 用）

```bash
BKLV_DESIGNER_PROJECT=my-skin          # 直接打开仓库里的某个主题
BKLV_DESIGNER_FORK=my-skin             # 先按模板新建一份再打开
BKLV_DESIGNER_FILE=button.primary      # 打开某个外观
BKLV_DESIGNER_VIEW=page_json.json      # 打开某个页面视图
BKLV_DESIGNER_TABLE=colors.json        # 打开主题表
BKLV_DESIGNER_NEW_APPEARANCE=my_widget # 新建一个外观文件（会登记进 theme.json）
BKLV_DESIGNER_SELECT=layer0            # 选中节点：file|box|layer<N>|state:<名>
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

# 继续 bklv 外壳重构 —— 新会话启动提示词

把下面整段复制给新会话即可（自包含，不依赖旧对话历史）。

---

## 任务：继续 bklv 外壳重构（第 4 步尾部 + 第 5~7 步）

仓库：/Users/beiklive/Code/C++/learn/imgui_learn（C++17 的 UI 库 bklv + demo + designer）

### 唯一权威依据（先完整读一遍）
- `docs/shell-implementation-plan.md` —— 正文 + 附录 A（分层）/ B（已核对的 borealis 焦点规格）/
  C（第 3 步清单）/ D（第 5 步清单）+ v9~v16（纪律、裁决、每个坑的行号与对策）。**以它为准**。
- `docs/layers-and-focus.md`、`docs/json-syntax.md`

### 当前基线（可信、全绿）
- 代码与文档全部入库、工作区干净；`./scripts/check.sh` → 全部检查通过（2013 项），GL/VK 零警告
- `python3 scripts/check_naming.py` → 136 个文件全部符合约定
- 新增测试：`tests/layout_primitive_test.cpp`(53)、`tests/shell_test.cpp`(46)、`tests/focus_test.cpp`(82)

### 已完成
1. 布局原语：`position` + `top/left/right/bottom` 本来就齐；补 `EdgeInsets`（`core/geometry.hpp`）+ 文档 + 测试
2. `Shell` 四层（`bklv/widgets/shell.{hpp,cpp}`）：Backdrop/Content/Overlay/Hud + `setHeaderView/setFooterView`
   + `getHeaderInset/getFooterInset`（内容避让**显式**）+ 安全区 + `addToCorner/addToEdge/fillLayer`
3. 焦点管理（扩展 `bklv/core/input.*`，**没有**新建 FocusManager 类；配套 `box.*`、`view.*`、
   `views/{h,}scrolling_frame.*`、`widgets/{appearance_view.cpp,button.hpp,cell.hpp}`）：
   顺序索引扫描 ±1 + 父链冒泡、默认焦点三级、`canFocus` 四条件、`CrossBoxPolicy` 三档、
   **默认不循环** + `setNavigationWrap(View*,bool)`、`setOnFocusEdgeHit(dir,current)` + 0.12s 节流、
   抖动 `View::shakeHighlight(dir)`、`remember/loadFocus/rememberFor/loadFocusFor`（存路径+hint，
   不存裸 View*）、`ensureFocusVisible`
4. 第 4 步四小块：`DialogOptions{dim,closeOnDimClick,closeOnBack,dimColor}`、
   `Shell::pushDialog/popDialog`（模态栈 + `removeView` + F3 顺序）、
   `InputManager::setSystemAction/clearSystemAction/hasSystemAction`（`handleNavigation()` 顶部，
   优先级高于模态）、dim 遮罩（插在弹窗之前入树、与弹窗同生共死）

### 先做这 5 件（第 4 步剩余，对策见 v14/v15）
1. `closeOnDimClick`：按 **F1(b)** 加"模态内空点回调"（`hitTest` 返 nullptr 时 Shell 判点是否在
   弹窗 `getFrame()` 内）——**不要**动 `hitTest`/`focusFromPointer` 的焦点语义
2. `closeOnBack` 的**读取点**（`input.cpp:384` 只写不读）+ **B 键限定子树派发**（**F2**）
3. `enterLayer(Box*)` / `leaveLayer()`（**F4：必须 `void leaveLayer()`**）
4. 焦点记忆从**单槽改栈**（**F5**：`input.hpp:325`）
5. 系统键与 `handleActions` 同帧双响应

### 再做第 5~7 步
- **第 5 步（最大）**：按**附录 D**。`bklv/widgets/pager.{hpp,cpp}`：`setPageFactory(id,fn)`、
  `show(id)`（**先挂新页 → 给新页焦点 → 再 delete 旧页**）、可取消 `preload/cancelPreload`、
  失败/loading、页面级记忆（焦点 + 滚动位置）。`ScrollingFrame` **已有**
  `setContentOffsetY/getContentOffsetY`，直接复用。然后**迁移 demo 14 页**（`AppletFrame` → `Shell`+`Pager`），
  逐页截图复核
- **第 6 步**：按页背景 `setBackdropFor(pageId, view)` + `TransitionHost`（跨层飞行元素**不要**挂 HudLayer）
- **第 7 步**：删旧 `AppletFrame`，但**必须保留 JSON 类型名 `"AppletFrame"`**；设计器回归 + 文档/缓存收尾

### 硬性纪律（踩出来的）
1. **每改一处立刻构建**（`cmake --build build/gl -j 8`），0 error 再改下一处；绝不攒改后一次构建
2. 精确锚点 + 单处替换；删块用**括号配对器**（同时数 `()` 与 `{}`）；**禁止**正则删块、**禁止**找 `});`
3. 有并发写者时**绝不用 `git add -A`**；不在写者活跃时 `reset/stash` 它的文件；回退只认已验证提交号
4. 新 setter **必须在 public 段**；命名要过 `check_naming.py`
   （**不过的前缀：`restore`、`leave`、`wrap`** → 用 `loadFocus/loadFocusFor`、`void leaveLayer()`）
5. 每步收尾：`check.sh`（只增不减、GL/VK 零警告）+ `check_naming.py` + 截图 + **提交**
6. 语义不确定就**停下来问用户**，不要猜；不把不可编译状态留在工作区

### 命令
```bash
cd /Users/beiklive/Code/C++/learn/imgui_learn
cmake -S . -B build/gl -DBKLV_GRAPHICS=gl -DBKLV_BUILD_TESTS=ON && cmake --build build/gl -j 8
./dist/shell_test && ./dist/focus_test && ./dist/layout_primitive_test
./scripts/check.sh
python3 scripts/check_naming.py
```
注意：**demo 里已经没有环境变量调试开关**（`BKLV_DEMO_PAGE`/`BKLV_SCREENSHOT` 等已删除）；
要验收写 `tests/` 无窗口测试，要截图在自己的临时程序里调 `App::requestScreenshot()`。
（设计器仍支持 `BKLV_DESIGNER_FILE` / `BKLV_DESIGNER_SHOT` / `BKLV_DESIGNER_WINDOW` / `BKLV_DESIGNER_INJECT_MOUSE`。）

### 其它必须知道的既有约定
- `ScrollingFrame::setContentView` **只摘不删**（所有权在调用方）
- 模态是 `InputManager` 的**模态栈**（`pushModal/popModal` + `isInsideModal`），别另立一套；
  `popModal()` 用**裸指针**恢复焦点、只校验 `savedFocus->getParent()`（F3）
- `View::hitTest`（`view.cpp:818`）对**非可聚焦**直接返回 nullptr
- 视图 JSON 的数字**不**按 uiScale 缩放；SIZE 属性用 `@style/metric.a.b`（**不是** `@theme/metric.`）
- Yoga 绝对定位子节点的偏移基准是父**外框**（padding **不**叠加）
- 异步回调必须带 `AliveFlag` 护栏（切页会 `delete` 旧页）
- 转场已抽到 `core/transition.hpp`（`ViewTransition`，入场/出场串行、绘制期变换、零重排）

### 交付要求
每完成一小块：构建 → 跑相关测试 → `check.sh` 全绿 → 提交（信息里写"做了什么/验证原文/仍未做"）。
若用子代理：范围 ≤6 个文件、每改一处构建、交付必须贴 `check.sh` 与 `check_naming.py` 原文、失败也要回报；
否则不要用（本项目多次因并发写者返工）。

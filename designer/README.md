# 设计器（Designer）

这里就是**设计器这个程序的项目根**：它的代码、资源、状态文件都放这个目录，
不再散到主仓库的 `bklv/`、`resources/`、`dist/` 里。

> 设计方案与实施细案见 [`DESIGN.md`](DESIGN.md)（研究结论 / 三方案对比 / 范围 / 交互 / 架构 / 分阶段验收）。
>
> 当前状态：**上一版已整体移除**（44 文件 / ~11.8k 行，见 git 历史 `3688411`），
> 目录里暂时只有这份约定和需求梳理（[`requirements.md`](requirements.md)）。
> 重做时按下面的规则来。

## 三条已定的边界（2026-10，用户拍板）

| # | 约定 | 具体做法 |
|---|---|---|
| 1 | **子目录**，不独立成仓库 | 项目根 = `designer/`；它自己的源码清单写在 `designer/CMakeLists.txt` 里，不把几十个文件名塞进根 CMakeLists |
| 2 | **绑定 bklv 库** | 同一工程里 `target_link_libraries(... bklv::bklv)`，可以直接 include 库的头、用 `Appearance` / `ThemeData` / `ViewTemplate` / `GlassService` / 图层注册表。**它是一方工具，不是第三方消费者**，所以不需要为了它把 API 磨成"对外可用"的样子 |
| 3 | **资源走原路径** | 主题、字体、文案仍读 `resources/`（相对资源目录解析，只读）。**不复制一份、不改资源路径**。设计器自己的东西（窗口状态、预设、导出模板）写在 `designer/` 下，或由调用方用参数指定 |

## 四条容易重犯的错（上一版都踩过）

1. **不往库内塞设计器专用分支**。库需要的能力就做成通用 API（例：`GlassService::getCapabilities()`、
   `layer_registry` 的中文名与参数模式、`ThemeData` 按绝对目录装载），不要写
   `if (designer)`。判断标准：这段代码删掉设计器之后还说得通吗？
2. **不往 `dist/` 写运行时产物**。上一版留下了 `dist/designer_state.json`、`dist/designer_project.json`，
   跟着 `check.sh` 一起被扫到。状态文件属于这个目录。
3. **文档模型与界面分离**。上一版的 `core/` 里既有 JSON 文档模型又有 diff/undo，
   结果改模型要动 UI、改 UI 要动模型。重做时：文档模型（读/改/写回、保序、来源追踪）
   先做成**无窗口可测**的库（`tests/` 能直接跑），UI 只做展示与输入。
4. **先有"源文档 + 有效值"双视图**。`import` / `include` / `vars` 让"一份文件"≠"一个完整对象"，
   只显示源文档会让人以为样式丢了；只显示有效值又没法改。两条都要，
   完整缺口清单见 [`../docs/json-audit.md`](../docs/json-audit.md)。

## 构建（代码落地时）

根 `CMakeLists.txt` 只加一行：

```cmake
set(BKLV_TARGET_LIST
    "demo|demo/main.cpp|app"
    "designer|designer/main.cpp|designer"   # 加这一行
)
```

并在 `designer/CMakeLists.txt` 里列自己的源文件。输出目录跟 demo 一样进 `dist/`（VK 进 `dist/vk/`），
**但状态文件不写在 `dist/`**。

`scripts/check.sh` 会把它一起编、一起跑测试（所以设计器代码同样要零警告）；
`scripts/check_naming.py` 只扫 `bklv/`，库的 API 命名规则不管设计器，但风格建议保持一致。

## 运行

```bash
scripts/check.sh                                   # 构建 + 全量检查（GL/VK）
./dist/designer                                     # 打开默认主题
./dist/designer --theme <主题目录> --file view/page_json
./dist/designer --palette --screenshot /tmp/d.png   # 无人值守验证：开浮层、渲染 N 帧后截图退出
```

命令行：`--theme` / `--theme-name` / `--file` / `--source` / `--palette` / `--zoom` /
`--click x,y` / `--screenshot <png>` / `--frames <n>`。设计与验收对照见 [`DESIGN.md`](DESIGN.md)。

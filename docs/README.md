# 文档索引

速查与说明文档。项目介绍、如何拉取编译、如何用到自己的项目里，见根目录的
[`README.md`](../README.md)。

## 速查

| 文档 | 内容 |
|---|---|
| [uml.html](uml.html) | bklv 库结构与 UML 图（自包含 HTML，离线可看；由 `scripts/gen_uml.py` 生成） |
| [architecture-review.html](architecture-review.html) | **架构评审**：分层与依赖图（含实测的 5 条反向依赖边）、帧循环时序、外观数据流、后端/平台抽象边界；组件清单、进程级单例清单、优点与风险（逐条附 `file:line`）与改进顺序 |
| [`bklv-api.md`](bklv-api.md) | bklv 移植层速查：文件职责表、Box 布局调用、交互与内容控件、imgui.ini 开关、日志约定、已知的坑、分层约定 |
| [`icon-fonts.md`](icon-fonts.md) | **图标字体规则**：两套字体（Material / switch_icons）、码位必须与字体配对、按层指定与运行时切换、三种典型写错方式 |
| [`json-syntax.md`](json-syntax.md) | **JSON 语法速查**：视图描述 / 控件外观 / 主题 / 文案 四类 JSON 的完整键表、取值形式、枚举拼写与报错行为 |
| [`rect-gradient.md`](rect-gradient.md) | **`rect` 层加渐变的调研与方案**：三条路线对比（顶点色网格 / LUT 贴图 / 现成 API）、JSON 语法草案、数据模型、绘制与抗锯齿实现、与状态动效的取舍、两步工作量与像素验收 |
| [`glass-layer.md`](glass-layer.md) | **液态玻璃使用说明**：三层职责（Shell 管开关/帧级参数、外观 JSON 管样式、视图只声明）、`op: "glass"` 写法与三条硬要求、后端矩阵与取舍、接口、三个坑、验收数据与文件地图 |
| [`glass-layer-plan.md`](glass-layer-plan.md) | **开发计划书**：液态玻璃做成外观层（GL / GLES3-Switch / Vulkan）—— 范围、API 契约与优先级、四个阶段与验收、风险清单、审查记录 |
| [`glass-layer-feasibility.md`](glass-layer-feasibility.md) | **调研**：液态玻璃能不能做成 bklv 的一层、用 Shell 接口配置 —— 现状盘点、两种"层"的取舍、要动的地方与行数、API 草案、六个坑、分阶段计划 |
| [`dependencies.md`](dependencies.md) | **依赖清单**（可直接转发）：9 个 vendored 子模块及版本/许可证、各平台要装的东西、随包资源体积 |

## 规划

| 文档 | 内容 |
|---|---|
| [`../TODO.md`](../TODO.md) | 开发计划：库化、控件补全、技术债、与 borealis 的取舍 |

## 怎么做

| 文档 | 内容 |
|---|---|
| [`../notes/adding-render-backend.md`](../notes/adding-render-backend.md) | 加一个渲染后端的完整步骤，以及两个后端都踩过的坑 |
| [`../AGENTS.md`](../AGENTS.md) | 开发约定：双后端要求、构建入口、平台与 Vulkan 的注意事项、代码规范 |

## 移植评估笔记

这几份是动手前的调研记录，保留作为决策依据，不代表当前实现状态：

| 文档 | 内容 |
|---|---|
| [`../notes/wiliwili-yoga-architecture.md`](../notes/wiliwili-yoga-architecture.md) | wiliwili 里 borealis + Yoga 的架构分析 |
| [`../notes/borealis-to-imgui-port-assessment.md`](../notes/borealis-to-imgui-port-assessment.md) | 从 borealis 移植到 ImGui 的可行性评估 |
| [`../notes/borealis-port-priorities.md`](../notes/borealis-port-priorities.md) | 移植的优先级与取舍 |

- [基础控件页对照表](base-widgets-map.md)：基础控件页里每个控件的代码 / 类文件 / 外观名 / JSON 路径
- [解析层重构与扩展机制（评审稿）](parse-refactor-review.md) —— `bklv/compile/` 分层、图层类型注册表、外观内 `children`、迁移与验收
- [解析层重构交付说明与示例](parse-refactor-handoff.md) —— 目录结构、可编译的示例外观、第三方加图层

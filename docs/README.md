# 文档索引

速查与说明文档。项目介绍、如何拉取编译、如何用到自己的项目里，见根目录的
[`README.md`](../README.md)。

## 速查

| 文档 | 内容 |
|---|---|
| [uml.html](uml.html) | bklv 库结构与 UML 图（自包含 HTML，离线可看；由 `scripts/gen_uml.py` 生成） |
| [`bklv-api.md`](bklv-api.md) | bklv 移植层速查：文件职责表、Box 布局调用、交互与内容控件、imgui.ini 开关、日志约定、已知的坑、分层约定 |
| [`json-syntax.md`](json-syntax.md) | **JSON 语法速查**：视图描述 / 控件外观 / 主题 / 文案 四类 JSON 的完整键表、取值形式、枚举拼写与报错行为 |
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

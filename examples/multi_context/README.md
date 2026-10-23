# 双 Context 示例

同一个宿主 ImGui 循环里运行两个 bklv Context：

- Context 1：default 主题；
- Context 2：glass 主题；
- 各自独立的 ThemeData / InputManager / EventBus / App；
- 共用宿主的窗口、ImGui Context 与纹理后端。

```bash
cmake --preset gl
cmake --build --preset gl --target multi_context
./dist/multi_context
```

运行日志应能看到两次 default 装载和一次 glass 装载；两棵页面同帧 step/draw 不崩溃。

当前仍共享：纹理仓库、字体图集（随 ImGui Context）、扩展注册表、布局静态量。

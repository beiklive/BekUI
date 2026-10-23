# 嵌入示例（SDL3 + OpenGL + ImGui）

宿主自己拥有窗口、GL 上下文、ImGui 上下文和主循环；bklv 只做 UI 树、布局、输入和绘制命令生成。

## 运行

```bash
cmake --preset gl && cmake --build --preset gl --target embed_imgui_gl
./dist/embed_imgui_gl
```

窗口里会同时出现 bklv 的页面和宿主自己的 `宿主窗口`。

## 帧顺序（必须一致）

```cpp
ImGui_ImplSDL3_NewFrame();
ImGui_ImplOpenGL3_NewFrame();
ImGui::NewFrame();

app.step(dt);    // 更新、布局、输入；不 NewFrame/Render
app.draw();      // 把 bklv 画进当前 ImGui 帧；不 Render

// 宿主自己的 ImGui 内容可以继续画
ImGui::Render();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
```

## 契约

- `attachHost()` 必须在宿主第一次 `ImGui::NewFrame()` **之前**调用（字体会加进当前图集）；
- bklv 不创建/销毁 ImGui Context，也不创建窗口或图形设备；
- 图片/动图需要宿主实现 `bklv::TextureBackend`（示例用 GL 实现）；
- `detachHost()` 只释放 bklv 的纹理引用，不碰宿主设备；
- 每帧调用 `setHostViewport()` 报告绘制区域；`step()` 与 `draw()` 成对调用；
- 玻璃效果在嵌入模式下暂不可用，会退回半透明底。

## 服务注入（M1）

宿主可在创建 `App` **之前**注入服务（App 构造期会读 i18n/资源）：

```cpp
bklv::Services services;
services.audio     = &myAudio;      // AudioService：接管 play/playFile
services.resources = &myResources;  // ResourceProvider：解析 resources/... 路径
bklv::setServices(services);

bklv::App app; // 之后按正常流程 attachHost/step/draw
...
bklv::clearServices(); // App 销毁前
```

- `AudioService`：不设置时用内置混音器；设置后所有 UI 音效（含 sound.json/外观覆盖）都走宿主。
- `ResourceProvider`：不设置时用可执行文件旁的 `resources/`；设置后可换成自定义根、pak 或内存资源。
- 服务由宿主拥有，bklv 只借用；必须在服务销毁前 `clearServices()`。

## 当前限制（M1）

- 主题、焦点、输入仍为全局单例；多实例需要后续 Context 阶段；
- 字体只能追加到宿主现有图集；
- 玻璃在嵌入模式下退回半透明；
- CMake install/find_package 尚未提供。

- 主题、焦点、输入、音频仍是全局单例：一个进程只建议一个 bklv App 实例；
- 字体只能追加到宿主现有图集，暂不支持图集重建后的重新合并；
- 服务注入（音频/输入/资源/字体/IME）、CMake 安装包、多实例在后续阶段实现。

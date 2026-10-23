# 嵌入 bklv（宿主驱动的 UI）

bklv 支持两种运行方式：

- **独立运行**：`App::run()` 创建窗口、ImGui Context、渲染后端与主循环；
- **宿主嵌入**：宿主拥有窗口、ImGui Context、GPU 设备和循环，bklv 只负责 View 树、布局、输入和绘制命令。

## 1. 所有权

| 资源 | 嵌入模式 | 独立运行 |
|---|---|---|
| 窗口 / 事件循环 | 宿主 | bklv 平台层 |
| ImGui Context | 宿主（默认借用） | bklv 创建 |
| GPU 设备 / ImGui 绘制后端 | 宿主 | bklv 平台层 |
| 图片纹理上传 | 宿主实现 `TextureBackend` | `RenderBackend` 适配 |
| View 树 | `Context` | `Context`（由 App 包装） |
| 主题 / 焦点 / 模态 / 输入 / 事件总线 | 每个 `Context` 独立 | 每个 `Context` 独立 |

`Context` 不创建 ImGui Context，也不调用 `ImGui::Render()`。

## 2. 帧顺序

```cpp
ImGui_ImplSDL3_NewFrame();
ImGui_ImplOpenGL3_NewFrame();
ImGui::NewFrame();

ui.step(dt);   // 更新 / 布局 / 输入
ui.draw();     // 画进当前 ImGui 帧

ImGui::Render();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
```

`attachHost()` 必须在宿主第一次 `NewFrame()` 之前调用（字体会加入当前图集）。

## 3. Context 与服务

```cpp
bklv::Context ui(services);   // 先装服务，再创建底层 App
ui.setRootView(root);
ui.attachHost(attach);
```

服务（[`bklv/core/services.hpp`](../bklv/core/services.hpp)）：

| 服务 | 用途 |
|---|---|
| `AudioService` | 接管 `play/playFile` |
| `ResourceProvider` | `resolve/read/hasResource`，自定义资源根/pak/内存 |
| `FontProvider` | `readBodyFont`（字节优先）/ `resolveBodyFont` |
| `ImeManager` | 系统软键盘 |
| `InputService` | 方向按住 / 确认 / 返回 |

服务由宿主拥有；`Context` 析构时 `clearServices()`。

## 4. 纹理

宿主实现 `TextureBackend`：

```cpp
class HostTextures : public bklv::TextureBackend {
    const char* getName() const override;
    bool isReady() const override;
    bklv::TextureHandle createTexture(const unsigned char*, int, int) override;
    void destroyTexture(bklv::TextureHandle&) override;
};
```

图片、动图、头像都走它；字体纹理由宿主 ImGui 后端负责。

## 5. 多实例

```cpp
bklv::Context ui1(services);
ui1.makeCurrent();      // 构建属于 ui1 的视图/主题前
ui1.loadTheme("default");

bklv::Context ui2(services);
ui2.makeCurrent();
ui2.loadTheme("glass");
```

每个 Context 独立：`ThemeData`、`InputManager`、`EventBus`、App、JSON 视图类型注册表。
共享：纹理仓库、字体图集（随 ImGui Context）、布局静态量。

## 6. 构建与安装

```bash
# 只编库
cmake -S . -B /tmp/bklv_node -DBKLV_BUILD_DEMO=OFF -DBKLV_BUILD_EXAMPLES=OFF
cmake --build /tmp/bklv_node

# 安装（可选解码器已随包导出）
cmake -S . -B /tmp/bklv_inst \
  -DBKLV_GRAPHICS=gl -DBKLV_BUILD_DEMO=OFF -DBKLV_BUILD_EXAMPLES=OFF \
  -DBKLV_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/tmp/bklv_prefix
cmake --build /tmp/bklv_inst -j8
cmake --install /tmp/bklv_inst

# 消费
cmake -S examples/cmake_consumer -B /tmp/bklv_cons \
  -DCMAKE_PREFIX_PATH=/tmp/bklv_prefix
cmake --build /tmp/bklv_cons
```

## 7. 示例

- [`examples/embed_imgui_gl`](../examples/embed_imgui_gl/README.md)：宿主 SDL3+GL+ImGui；
- [`examples/multi_context`](../examples/multi_context/README.md)：两个 Context、不同主题、同一 ImGui 帧；
- [`examples/cmake_consumer`](../examples/cmake_consumer/README.md)：安装后 `find_package`。

## 8. 已知限制

- 纹理仓库/字体图集/布局静态量仍全进程共享；
- 玻璃在嵌入模式下退回半透明（没有 GlassBackend 时）；
- 未提供 `BKLV_WITH_SDL=OFF` 的纯核心构建。

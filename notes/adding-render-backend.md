# 加一个渲染后端（以 Vulkan 为例）

现在的分层：

```
src/bklv/views/*        Image / Label / ScrollingFrame ...   ← 只认 ImDrawList 和 ImTextureID
src/bklv/core/graphics  RenderBackend 接口 + 纹理管理        ← 唯一的分界点
src/bklv/backends/*     OpenGLBackend / GLES3Backend / NullBackend  ← 图形 API 代码全在这里
src/bklv/platforms/*    PlatformSDL3 / PlatformSwitch        ← 窗口 + 事件循环 + 创建后端
src/bklv/core/app.cpp   ImGui 上下文 + 帧调度                 ← 不碰图形 API
```

**后端是编译期选择的**，没有运行时注册表。加 Vulkan 时的接法就是在对应平台的
`createBackend()` 里换一个实现；链接库与 shader 版本随之改变，这属于构建层面的选择。

`Image`、`Label`、`Box`、`ScrollingFrame`、`RecyclerFrame` 里**没有任何图形 API 调用**，
所以加 Vulkan 时上层一行都不用改。全部改动集中在 3 个地方。

---

## 第 1 步：写 VulkanBackend

新建 `src/bklv/backends/vulkan_backend.hpp/.cpp`，实现 `RenderBackend` 的 10 个方法：

| 方法 | Vulkan 下要做的事 |
|---|---|
| `getWindowFlags()` | 返回 `SDL_WINDOW_VULKAN` |
| `initContext(window)` | 创建 VkInstance（`SDL_Vulkan_GetInstanceExtensions` 取扩展）、VkSurfaceKHR（`SDL_Vulkan_CreateSurface`）、选物理设备与队列、建逻辑设备、建 swapchain + render pass + framebuffer + command buffer、建栅栏与信号量 |
| `initImGuiRenderer()` | `ImGui_ImplVulkan_Init()`，并上传字体纹理 |
| `shutdownImGuiRenderer()` | `ImGui_ImplVulkan_Shutdown()` |
| `newFrame()` | `ImGui_ImplVulkan_NewFrame()`；另外要处理 ImGui 的 `io.Fonts->TexList` 纹理请求（见第 3 步） |
| `clear(rgba)` | `vkCmdBeginRenderPass` + `vkCmdClearAttachments`（或 render pass 的 clearValue） |
| `renderDrawData(data)` | `ImGui_ImplVulkan_RenderDrawData(data, cmdBuffer)` |
| `present()` | `vkQueueSubmit` + `vkQueuePresentKHR`，然后重建帧同步对象 |
| `createTexture(pixels,w,h)` | 建 VkImage + 显存、`vkCmdCopyBufferToImage`、建 VkImageView、分配并更新 VkDescriptorSet（这就是 `TextureHandle::userData` 要存的东西），`texture` 字段填成 `(ImTextureID)descriptorSet` |
| `destroyTexture(handle)` | 销毁 image / view / memory / descriptorSet，并把 handle 清空 |

构造函数里把 `this->ready = true` 放在设备建好之后——**`ready` 是 "能不能创建纹理" 的闸门**。

## 第 2 步：在平台里换上它

图形后端由平台在 `createBackend()` 里创建，编译期决定。例如
`src/bklv/platforms/sdl3_platform.cpp`：

```cpp
void PlatformSDL3::createBackend()
{
#if defined(BKLV_NULL_BACKEND)
    this->graphicsBackend = std::make_unique<NullBackend>();
#elif defined(BKLV_VULKAN)
    this->graphicsBackend = std::make_unique<VulkanBackend>();
#else
    this->graphicsBackend = std::make_unique<OpenGLBackend>();
#endif
}
```

再把 `.cpp` 加进 `build.sh` 的 `BKLV_SOURCES`：

```bash
    "${ROOT_DIR}/src/bklv/backends/vulkan_backend.cpp"
```

链接 Vulkan 库（macOS 上是 MoltenVK）：在 `LINK_FLAGS` 里加 `-lvulkan` 或对应的
framework，并保证 `vulkan.framework`/`libvulkan.dylib` 在运行时可被找到。

## 第 3 步：处理 ImGui 的字体纹理

ImGui 1.92 之后字体图集由渲染后端按需创建。每个后端都要：

- `initImGuiRenderer()` 里设置能力标记：
  ```cpp
  ImGuiIO& io = ImGui::GetIO();
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  io.BackendRendererName = "bklv_vulkan";
  ```
- 每帧处理 `io.Fonts->TexList` 里状态为 `WantCreate` / `WantUpdates` / `WantDestroy`
  的 `ImTextureData`，创建/更新对应图形资源，然后
  `texture->SetStatus(ImTextureStatus_OK)`。

`ImGui_ImplVulkan_*` 自己会处理这些，所以用官方后端时这步基本白送；
`NullBackend` 是个最小示例（见 `src/bklv/backends/null_backend.cpp`），
它不做任何创建，但会把请求全部置为 OK，避免 ImGui 断言。

---

## 验证方式

不用等 Vulkan 写完，随时可以验证抽象层有没有漏：

```bash
# 1. 上层不许出现图形 API 调用（应该没有输出）
grep -rn "gl[A-Z]\|OpenGL\|imgui_impl_opengl3" src/bklv/views src/bklv/core/view.cpp src/bklv/core/box.cpp src/bklv/core/input.cpp

# 2. 空后端自检：窗口能开、布局/焦点/滚动/回收都照常跑，只是不输出画面
BKLV_NULL_BACKEND=1 ./build.sh
```

Vulkan 后端在写出来之前，`-DBKLV_VULKAN` 会编译失败——这正是编译期选择的好处：
配置错了当场暴露，而不是运行时某条日志。

---

## 两个已知的坑

1. **相对路径的图片加载要 `realpath`。**
   `CFURLCreateWithFileSystemPath` 不解析相对路径，传 `resources/x.png` 会得到一个
   打不开的 URL。`graphics.cpp` 里已经处理。

2. **`initTextureStore` 换后端时要清缓存。**
   旧句柄属于上一个后端不能再使用；而且之前的"加载失败"标记会把新后端挡住
   （开发时踩过：先无后端加载失败一次，之后接了 OpenGL 后端仍然一直失败）。

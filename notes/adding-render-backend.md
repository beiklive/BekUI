# 加一个渲染后端（以 Vulkan 为例）

现在的分层：

```
bklv/ui/widgets/*            Image / Label / ScrollingFrame ...   ← 只认 ImDrawList 和 ImTextureID
bklv/foundation/core/graphics.*    RenderBackend 接口 + 纹理管理        ← 唯一的分界点
bklv/graphics/render/*         OpenGLBackend / VulkanBackend / GLES3Backend  ← 图形 API 代码全在这里
bklv/system/platform/*        PlatformSDL3 / PlatformSwitch        ← 窗口 + 事件循环 + 创建后端
bklv/ui/app/app.cpp       ImGui 上下文 + 帧调度                 ← 不碰图形 API
```

**后端是编译期选择的**，没有运行时注册表。加 Vulkan 时的接法就是在对应平台的
`createBackend()` 里换一个实现；链接库与 shader 版本随之改变，这属于构建层面的选择。

`Image`、`Label`、`Box`、`ScrollingFrame`、`RecyclerFrame` 里**没有任何图形 API 调用**，
所以加 Vulkan 时上层一行都不用改。全部改动集中在 3 个地方。

---

## 第 1 步：写 VulkanBackend

新建 `bklv/graphics/render/vk/vulkan_backend.hpp/.cpp`，实现 `RenderBackend` 的 10 个方法：

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

`bklv/graphics/render/vk/vulkan_backend.cpp` 是这份清单的完整实现，写新后端时可以直接照着看。
实现时踩到、但清单里不容易一眼看出的几点：

- **`appInfo.apiVersion` / `ImGui_ImplVulkan_InitInfo::ApiVersion` 别写 `VK_API_VERSION_1_0`。**
  实例请求的版本决定了 loader 能通过 `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr`
  返回哪些 core 函数名。请求 1.0 时 loader 只给 `vkCmdBeginRenderingKHR` 这种后缀名，
  而 ImGui 会先探 core 名 `vkCmdBeginRendering`，探不到直接断言。
- **动态渲染有两条启用路径，要按 API 版本选一条。**
  API >= 1.3 走 core（`VkPhysicalDeviceVulkan13Features::dynamicRendering`），
  1.1/1.2 走扩展（把 `VK_KHR_dynamic_rendering` 加进设备扩展列表 +
  `VkPhysicalDeviceDynamicRenderingFeatures`）。只挂 pNext、不加扩展名，在某些设备上
  会"设备创建成功但一用就挂"。
- **同步信号量按"谁保护它"来分组。** 等待图像可用的信号量按 frame-in-flight 分组
  （由帧栅栏保护），而 submit 完成、交给 present 的那个信号量必须**按交换链图像**
  分组——帧栅栏只能证明 submit 完成，证明不了呈现引擎已经释放它；只有重新 acquire
  到同一张图像才能证明。否则校验层会报
  `VUID-vkQueueSubmit-pSignalSemaphores-00067`。

## 第 2 步：在平台里换上它

图形后端由平台在 `createBackend()` 里创建，编译期决定。例如
`bklv/system/platform/sdl3_platform.cpp`：

```cpp
void PlatformSDL3::createBackend()
{
#if defined(BKLV_VULKAN)
    this->graphicsBackend = std::make_unique<VulkanBackend>();
#else
    this->graphicsBackend = std::make_unique<OpenGLBackend>();
#endif
}
```

再把它的 ImGui 后端一并编进去。Vulkan 的 `imgui_impl_vulkan.cpp` **不在**默认的
`IMGUI_SOURCES` 里，漏了会在链接期报一串 `undefined reference to
'ImGui_ImplVulkan_Init/NewFrame/RenderDrawData/Shutdown'`。`CMakeLists.txt` 现在的接法是：

```cmake
if(BKLV_GRAPHICS STREQUAL "vk")
    list(APPEND BKLV_IMGUI_SOURCES "${BKLV_3RD}/imgui/backends/imgui_impl_vulkan.cpp")
endif()
```

链接 Vulkan 库（macOS 上是 MoltenVK）：在 `LINK_FLAGS` 里加 `-lvulkan` 或对应的
framework，并保证 `vulkan.framework`/`libvulkan.dylib` 在运行时可被找到。
Windows 上要注意 MinGW 用的是 `libvulkan-1.dll.a`，LunarG SDK 自带的
`vulkan-1.lib` 是 MSVC 格式，MinGW 链接不了。

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

`ImGui_ImplVulkan_*` 与 `ImGui_ImplOpenGL3_*` 自己会处理这些，用官方后端时这步基本白送；
自己写后端才需要接。

---

## 验证方式

不用等 Vulkan 写完，随时可以验证抽象层有没有漏：

```bash
# 1. 上层不许出现图形 API 调用（应该没有输出）
grep -rn "gl[A-Z]\|OpenGL\|imgui_impl_opengl3" bklv/views bklv/ui/view/view.cpp bklv/ui/view/box.cpp bklv/ui/input/input.cpp

# 2. 两个后端都跑一遍同一个 demo：换后端能跑起来，才说明没有漏抽象
./dist/demo
./dist/vk/demo
```

Vulkan 后端已经写完，所以 `-DBKLV_VULKAN` 不再是"配置错了就会编译失败"的哨兵：
后端选择写错、或者新后端少接一段 ImGui 调用，都要跑到运行时才暴露。
现在的哨兵是
以及 Vulkan 的校验层（`BKLV_VULKAN_VALIDATION=1`，见 README）。

---

## 三个已知的坑

1. **图片是相对进程工作目录加载的，失败还会被缓存。**
   macOS 走 ImageIO，`loadTexture` 里对相对路径做过处理；Windows / Linux 走
   `stbi_load`，它按**进程当前工作目录**解析路径。所以可执行文件必须从仓库根目录
   启动（`dist/demo`、`dist/vk/demo` 都一样），
   否则图片全变成占位色块，而且**只打一条** `图片加载失败：<path>` 日志就再无声息
   （见下一条）。调试时先确认工作目录，不要一上来就怀疑渲染后端。

2. **`loadTexture` 失败会写进路径缓存，不再重试。**
   `graphics.cpp` 的 `g_pathCache` 把失败记为 `TEXTURE_CACHE_FAILED`（`-2`），
   之后同一个路径直接返回无效引用，连日志都不打。这是为了不给每个列表项刷屏，
   但代价是"第一次为什么失败"这个信息只出现一次，日志被刷掉就找不回来了。

3. **`initTextureStore` 换后端时要清缓存。**
   旧句柄属于上一个后端不能再使用；而且之前的"加载失败"标记会把新后端挡住
   （开发时踩过：先无后端加载失败一次，之后接了 OpenGL 后端仍然一直失败）。

# BekUI（bklv）依赖清单

> 一份可以直接转发的依赖说明。所有第三方都走 **git submodule**、随构建一起编，
> **不需要系统预装**（只有 Windows 的 Vulkan 构建要多装两个包，见文末）。
> 项目自身只依赖 C++17 标准库 + 平台图形 API。

## 一、第三方子模块（9 个，全部 vendored）

| 库 | 版本 | 用途 | 许可证 | 是否可选 |
|---|---|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | v1.92.9b | UI 绘制与输入；桌面/GL 的运行时函数加载复用其自带 imgl3w | MIT | 必须 |
| [Yoga](https://github.com/facebook/yoga) | v3.2.1 | Flexbox 布局引擎（视图树排版） | MIT | 必须 |
| [tweeny](https://github.com/mobius3/tweeny) | v4.1.0 | 补间动画（属性动画、状态过渡） | MIT | 必须（库本身用得少） |
| [SDL3](https://github.com/libsdl-org/SDL) | release-3.4.16 | 桌面窗口 / 输入 / GL 上下文（源码编译） | Zlib | 桌面必须（Switch 不用） |
| [stb](https://github.com/nothings/stb) | 2c980bb | 位图解码（PNG/JPEG/BMP）+ GIF（含动图）+ PNG 写出（截图） | 公有领域 / MIT 双许可 | 必须 |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.11.3 | 主题 JSON / 视图描述 / i18n 文案（单头文件，全 inline） | MIT | 必须 |
| [libwebp](https://github.com/webmproject/libwebp) | v1.5.0 | WebP 解码（静态图 + 动图） | BSD-3-Clause | 可选（`BKLV_WITH_WEBP`） |
| [lunasvg](https://github.com/sammycage/lunasvg) | v3.1.0 | SVG 栅格化 | MIT | 可选（`BKLV_WITH_SVG`） |
| [plutovg](https://github.com/sammycage/plutovg) | f63f9b5 | lunasvg 的矢量后端 | MIT | 随 lunasvg |

**可选 = 子模块不存在时构建照常成功**：CMake 先探测，缺了只打印一条提示并定义"不启用"，
对应 TU 编成"返回带错误信息的空图"的桩，调用方只会看到一句人话错误。
Switch 构建可用 `--no-webp / --no-svg` 显式关掉。

## 二、构建工具与系统依赖

| 平台 | 需要装的东西 | 说明 |
|---|---|---|
| macOS | Xcode Command Line Tools + `cmake` | GL 构建零额外依赖；Vulkan 构建需 `brew install vulkan-headers vulkan-loader molten-vk` |
| Linux | `build-essential cmake ninja-build pkg-config` | Vulkan 构建再加 `libvulkan-dev` + 头文件 |
| Windows | MSYS2 / MinGW-w64：`mingw-w64-x86_64-gcc`、`-cmake`、`-ninja` | Vulkan 构建再加 `mingw-w64-x86_64-vulkan-headers`、`-vulkan-loader` |
| Switch | devkitPro（devkitA64 + libnx） | `./scripts/build_switch.sh`，不链 SDL3，用 libnx + EGL/GLES3 |

**不需要** `brew install sdl3` / 系统 SDL3 —— SDL3 是子模块、随 CMake 一起编。

## 三、图形与平台 API（系统自带，非第三方库）

- **OpenGL**：桌面 GL 构建（macOS 4.1 Core / Linux / Windows 3.2 Core 运行时加载）
- **Vulkan**：`-DBKLV_GRAPHICS=vk` 构建（macOS 走 MoltenVK）
- **EGL / GLES3**：Switch
- 窗口/输入：桌面 SDL3，Switch libnx
- 字形：桌面读系统字体文件；Switch 走 `pl` 服务拿系统共享字体

## 四、随包资源（不是库，但会影响体积）

| 内容 | 体积 | 说明 |
|---|---|---|
| `resources/font/MaterialIcons-Regular.ttf` | 352 KB | 控件图标（Apache-2.0） |
| `resources/font/switch_icons.ttf` | 180 KB | 按键/系统图标（桌面回退用） |
| `resources/theme/`（3 套主题） | 612 KB | 纯 JSON + 少量图片，可裁剪 |

## 五、许可证注意事项（转发给使用方时建议一并说明）

- 项目自身与上表所有第三方均为 **MIT / Zlib / BSD / 公有领域** 这类宽松许可，
  **没有 GPL/LGPL 传染性依赖**，可商用、可闭源分发（保留版权声明即可）。
- 字体：`MaterialIcons-Regular.ttf` 是 Apache-2.0；`switch_icons.ttf` 为按键图标，
  若对外分发请按你的渠道要求确认（本项目内部用于界面提示）。
- 设计器的液态玻璃着色器移植自一个 MIT 项目（Android Liquid Glass），
  版权声明保留在着色器文件头部。

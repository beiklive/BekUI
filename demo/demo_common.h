#pragma once // 防止头文件被重复包含。

#include "imgui.h" // 引入 Dear ImGui 的公开 API。
#include "backends/imgui_impl_sdl3.h" // 引入 SDL3 平台后端。
#include "backends/imgui_impl_opengl3.h" // 引入 OpenGL3 渲染后端。
#include <SDL3/SDL.h> // 引入 SDL3 的窗口、事件和 OpenGL API。
#include "bklv/backends/gl_loader.hpp" // 跨平台 OpenGL 3 头（macOS framework / Linux libGL）。

namespace learn_imgui { // 将学习代码放进独立命名空间，避免名称冲突。

// 各平台常见的中文字体，按优先级尝试；一个都打不开就退回 ImGui 内置字体。
// 内置字体不含中文字形，中文会显示成方块，所以能找到系统字体就尽量用。
inline void LoadChineseFont() { // 为示例界面加载一个能显示中文的字体。
    ImGuiIO& io = ImGui::GetIO(); // 取得 ImGui 输入输出配置。
#if defined(_WIN32)
    const char* candidates[] = { // Windows 自带的中文字体。
        "C:/Windows/Fonts/msyh.ttc", // 微软雅黑，Vista 起的默认中文 UI 字体。
        "C:/Windows/Fonts/simhei.ttf", // 黑体，更老的系统上也有。
        "C:/Windows/Fonts/simsun.ttc", // 宋体，最后的兜底。
    };
#elif defined(__APPLE__)
    const char* candidates[] = { // macOS 自带的中文字体。
        "/System/Library/Fonts/Hiragino Sans GB.ttc", // 冬青黑体。
        "/System/Library/Fonts/PingFang.ttc", // 苹方。
    };
#else
    const char* candidates[] = { // Linux 常见的中文字体包路径。
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    };
#endif
    for (const char* path : candidates) { // 逐个尝试候选字体。
        if (io.Fonts->AddFontFromFileTTF(path, 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull()) != nullptr) { // 加载成功就用它。
            return; // 已经拿到可用字体，不必继续尝试。
        } // 结束加载成功判断。
    } // 结束候选字体遍历。
    io.Fonts->AddFontDefault(); // 全都没成功时退回内置字体，保证界面还能显示。
} // 结束中文字体加载函数。

template <typename DrawFunction> // 允许调用方传入任意可调用对象绘制每一帧。
int RunDemo(const char* window_title, DrawFunction draw_function) { // 创建窗口并持续运行 ImGui 帧循环。
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { // 初始化 SDL 视频和手柄子系统。
        return 1; // 返回错误码表示 SDL 初始化失败。
    } // 结束 SDL 初始化检查。
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0); // 创建没有额外调试标志的 OpenGL 上下文。
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); // 请求 OpenGL Core Profile。
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3); // 请求 OpenGL 主版本 3。
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2); // 请求 OpenGL 次版本 2。
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); // 启用双缓冲，避免绘制过程闪烁。
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); // 设置深度缓冲位数。
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8); // 设置模板缓冲位数。
    const SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY; // 设置 OpenGL、可缩放和高 DPI 窗口属性。
    SDL_Window* window = SDL_CreateWindow(window_title, 1280, 800, window_flags); // 创建 1280x800 的 macOS 窗口。
    if (window == nullptr) { // 检查窗口是否创建成功。
        SDL_Quit(); // 释放 SDL 已分配的全局资源。
        return 1; // 返回错误码表示窗口创建失败。
    } // 结束窗口创建检查。
    SDL_GLContext gl_context = SDL_GL_CreateContext(window); // 为窗口创建 OpenGL 上下文。
    if (gl_context == nullptr) { // 检查 OpenGL 上下文是否创建成功。
        SDL_DestroyWindow(window); // 销毁已经创建的窗口。
        SDL_Quit(); // 释放 SDL 已分配的全局资源。
        return 1; // 返回错误码表示 OpenGL 上下文创建失败。
    } // 结束 OpenGL 上下文检查。
    SDL_GL_MakeCurrent(window, gl_context); // 将窗口的 OpenGL 上下文设为当前上下文。
    SDL_GL_SetSwapInterval(1); // 开启垂直同步，避免无上限地渲染。
    // Windows 上 opengl32.dll 只导出 GL 1.1，3.2 Core 的函数要在这里取地址；
    // 其余平台这是空实现。必须在 ImGui_ImplOpenGL3_Init 之前完成。
    bklv::loadGLFunctions();
    IMGUI_CHECKVERSION(); // 检查 ImGui 头文件和库的版本及数据结构布局。
    ImGui::CreateContext(); // 创建 ImGui 上下文。
    ImGuiIO& io = ImGui::GetIO(); // 取得 ImGui 输入输出配置。

    // 关闭 ImGui 的 ini 自动读写，避免在运行目录生成 imgui.ini。
    // 必须在第一次 NewFrame 之前设置：ImGui 只在 io.IniFilename 非空时才读写。
    io.IniFilename = nullptr;
    LoadChineseFont(); // 加载当前平台的中文字体，保证中文不是方块。
    ImGui::StyleColorsDark(); // 使用内置深色主题。
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context); // 初始化 SDL3 输入与窗口事件后端。
    ImGui_ImplOpenGL3_Init("#version 150"); // 初始化 OpenGL3 绘制后端。
    bool running = true; // 保存主循环是否继续运行的状态。
    while (running) { // 窗口关闭前持续运行帧循环。
        SDL_Event event; // 声明 SDL 事件变量。
        while (SDL_PollEvent(&event)) { // 逐个读取待处理的键盘、鼠标和窗口事件。
            ImGui_ImplSDL3_ProcessEvent(&event); // 将事件转发给 ImGui 平台后端。
            if (event.type == SDL_EVENT_QUIT) { // 检查用户是否请求退出程序。
                running = false; // 收到退出事件后结束主循环。
            } // 结束退出事件判断。
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) { // 检查用户是否关闭当前窗口。
                running = false; // 收到当前窗口关闭事件后结束主循环。
            } // 结束窗口关闭判断。
        } // 结束 SDL 事件处理循环。
        ImGui_ImplOpenGL3_NewFrame(); // 为 OpenGL 渲染后端开始新帧。
        ImGui_ImplSDL3_NewFrame(); // 为 SDL3 平台后端更新输入与显示状态。
        ImGui::NewFrame(); // 开始构建当前帧的 ImGui 控件。
        draw_function(); // 调用具体示例绘制本帧界面。
        ImGui::Render(); // 将当前帧的控件转换为绘制数据。
        int display_width = 100; // 声明帧缓冲区宽度变量。
        int display_height = 100; // 声明帧缓冲区高度变量。
        SDL_GetWindowSizeInPixels(window, &display_width, &display_height); // 读取高 DPI 屏幕上的实际帧缓冲区尺寸。
        glViewport(0, 0, display_width, display_height); // 将 OpenGL 绘制区域设为整个帧缓冲区。
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f); // 设置窗口背景色。
        glClear(GL_COLOR_BUFFER_BIT); // 清空上一帧的画面。
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // 使用 OpenGL 将 ImGui 绘制数据显示到窗口。
        SDL_GL_SwapWindow(window); // 将完成绘制的后缓冲区显示出来。
    } // 结束持续渲染循环。
    ImGui_ImplOpenGL3_Shutdown(); // 释放 OpenGL3 后端资源。
    ImGui_ImplSDL3_Shutdown(); // 释放 SDL3 后端资源。
    ImGui::DestroyContext(); // 销毁 ImGui 上下文。
    SDL_GL_DestroyContext(gl_context); // 销毁 OpenGL 上下文。
    SDL_DestroyWindow(window); // 销毁应用窗口。
    SDL_Quit(); // 释放 SDL 全局资源。
    return 0; // 返回成功状态。
} // 结束窗口运行函数。

} // 结束学习代码命名空间。


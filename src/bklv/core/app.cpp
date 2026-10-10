#include "bklv/core/app.hpp"

#include <SDL3/SDL.h>

#include "backends/imgui_impl_sdl3.h"
#include "imgui.h"

#include "bklv/core/input.hpp"
#include "bklv/core/logger.hpp"
#include "bklv/core/time.hpp"
#include "bklv/views/debug_layer.hpp"
#include "bklv/views/hints.hpp"

namespace bklv
{

// 最近一次布局使用的画布尺寸。
float App::lastLayoutWidth  = 0.0f;
float App::lastLayoutHeight = 0.0f;

// ---------------------------------------------------------------------------
// 后端选择
// ---------------------------------------------------------------------------

void App::setGraphicsBackend(GraphicsBackendType type)
{
    this->backendPriority = { type };
}

void App::setGraphicsBackendPriority(const std::vector<GraphicsBackendType>& priority)
{
    if (!priority.empty())
        this->backendPriority = priority;
}

bool App::createBackend()
{
    for (GraphicsBackendType type : this->backendPriority)
    {
        if (!isGraphicsBackendAvailable(type))
        {
            Logger::info(std::string("跳过不可用的渲染后端：") + getGraphicsBackendName(type));
            continue;
        }

        RenderBackend* created = createRenderBackend(type);
        if (created != nullptr)
        {
            this->backend.reset(created);
            Logger::info(std::string("使用渲染后端：") + created->getName());
            return true;
        }
    }

    Logger::error("没有可用的渲染后端");
    return false;
}

// ---------------------------------------------------------------------------
// 根视图
// ---------------------------------------------------------------------------

App::~App()
{
    delete this->root;
    this->root = nullptr;
}

void App::setRootView(View* view)
{
    if (this->root == view)
        return;

    delete this->root;
    this->root = view;

    if (this->root != nullptr)
    {
        this->root->setDetached(true);
        this->root->setId("bklv/root");
    }

    // 焦点导航从根视图开始。
    getInputManager().setRoot(this->root);
    if (this->root != nullptr)
        getInputManager().giveFocus(this->root->getDefaultFocus());
}

// ---------------------------------------------------------------------------
// 帧
// ---------------------------------------------------------------------------

void App::frame()
{
    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float width             = viewport->WorkSize.x;
    const float height            = viewport->WorkSize.y;

    FrameContext ctx;
    // 画在主视口的前景绘制列表上：不依赖任何 ImGui 窗口，也不会被窗口遮挡。
    ctx.drawList   = ImGui::GetForegroundDrawList();
    ctx.theme      = &getDefaultTheme();
    ctx.pixelRatio = 1.0f;

    if (this->root != nullptr)
    {
        // 0. 字体是在窗口创建后才加载的，而视图树通常更早就建好了。
        //    字体就位后必须让所有依赖文本尺寸的节点重算一次。
        if (this->lastFontGeneration != getFontGeneration())
        {
            this->lastFontGeneration = getFontGeneration();
            this->root->refreshFontDependentLayout();
        }

        // 1. 布局：每帧按当前画布尺寸算一次（比 borealis 保守，但保证不会漏算）。
        App::lastLayoutWidth  = width;
        App::lastLayoutHeight = height;
        YGNodeCalculateLayout(this->root->getYGNode(), width, height, YGDirectionLTR);

        // 2. 动画与视图逐帧逻辑（滚动等）。
        Ticking::updateTickings();
        this->root->update();

        // 3. 输入与焦点（必须在绘制前，读取的是上一帧的命中结果）。
        getInputManager().update();

        // F1 开关调试层
        static bool f1Down = false;
        const bool f1Now   = ImGui::IsKeyDown(ImGuiKey_F1);
        if (f1Now && !f1Down)
            this->debugLayerEnabled = !this->debugLayerEnabled;
        f1Down = f1Now;

        // 4. 业务帧回调。
        if (this->onUpdate)
            this->onUpdate();

        // 5. 绘制视图树。
        this->root->frame(&ctx);

        // 6. 提示栏与调试层画在最上层。
        drawHints(ctx.drawList, width, height);
        if (this->debugLayerEnabled)
            drawDebugLayer(ctx.drawList, this->root, width);
    }

    ImGui::Render();
}

// ---------------------------------------------------------------------------
// 主循环
// ---------------------------------------------------------------------------

int App::run(const std::string& title, int width, int height)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        Logger::error(std::string("SDL 初始化失败：") + SDL_GetError());
        return 1;
    }

    if (!this->createBackend())
    {
        SDL_Quit();
        return 1;
    }

    // 窗口标记由后端决定：OpenGL 要 SDL_WINDOW_OPENGL，Vulkan 要 SDL_WINDOW_VULKAN。
    const SDL_WindowFlags windowFlags =
        this->backend->getWindowFlags() | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    SDL_Window* window = SDL_CreateWindow(title.c_str(), width, height, windowFlags);
    if (window == nullptr)
    {
        Logger::error(std::string("创建窗口失败：") + SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // 后端创建图形上下文（GL 上下文 / Vulkan instance + swapchain）。
    if (!this->backend->initContext(window))
    {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    loadFonts();

    // 手柄是可选的：没有手柄时键鼠照常工作。
    getInputManager().setupGamepad();

    // ImGui 的平台后端（键鼠/窗口事件），与图形 API 无关。
    ImGui_ImplSDL3_InitForOther(window);

    // 图形 API 那一侧的渲染后端。
    if (!this->backend->initImGuiRenderer())
    {
        Logger::error("ImGui 渲染后端初始化失败");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // 到这里上下文与渲染后端都就绪了，图片可以上传纹理了。
    initTextureStore(this->backend.get());

    const float clearColor[4] = { 0.10f, 0.10f, 0.12f, 1.0f };

    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        this->backend->newFrame();
        ImGui_ImplSDL3_NewFrame();

        this->frame();

        this->backend->clear(clearColor);
        this->backend->renderDrawData(ImGui::GetDrawData());
        this->backend->present();
    }

    // 释放顺序很重要：先释放纹理（还需要上下文），再关渲染后端，最后销毁上下文。
    shutdownTextureStore();

    this->backend->shutdownImGuiRenderer();
    ImGui_ImplSDL3_Shutdown();
    getInputManager().shutdownGamepad();
    ImGui::DestroyContext();

    // 后端的析构负责销毁图形上下文。
    this->backend.reset();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

int RunDemo(const std::string& title, View* root, int width, int height)
{
    App app;
    app.setRootView(root);
    return app.run(title, width, height);
}

} // namespace bklv
